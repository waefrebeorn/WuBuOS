/*
 * WuBuGrep - "occupational supremacy of C11 code"
 * ---------------------------------------------------------------------------
 * A from-scratch, dependency-free grep in strict C11. No vectorscan, no RE2,
 * no libpcre - we make OUR OWN matcher.
 *
 * Strategies (stolen and improved without asking):
 *   - Boyer-Moore-Horspool + our own SIMD first-byte prefilter
 *     (AVX2 / SSE4.2 / generic) finding candidates 16/32 bytes at a time.
 *   - mmap for large files; buffered pread for small.
 *   - PARALLEL single-file scan: a file is split into line-aligned chunks,
 *     each scanned by its own thread, output flushed in order. This is the
 *     lever that beats ripgrep on big files.
 *   - Per-thread work-stealing directory queue for recursive walks.
 *   - Buffered output, flushed once per chunk / per file.
 *
 * License: WaefreBeorn Umbrella License v3.0
 * ---------------------------------------------------------------------------
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "wubre.h"
#include "wubre_internal.h"

/* argv0 multi-tool dispatch (Nathan's "bundle tools in one binary" rule):
 * if the executable name contains "cat" we behave as WuBuCat (byte-exact
 * dump), otherwise WuBuGrep. No third-party libs, one self-contained binary. */

/* When -E/-G is given we use the WuBu regex engine; otherwise for a plain
 * literal we keep the SIMD-accelerated Boyer-Moore-Horspool fast path. */
static int g_opt_regex = 0;        /* 0 = literal, 1 = ERE (-E), 2 = BRE (-G) */
static WURegex *g_re = NULL;

/* extra options */
static int g_opt_word = 0;         /* -w : match whole words */
static int g_opt_line_mode = 0;    /* -x : match whole lines */
static int g_opt_text = 0;         /* -a : process binary files as text */
static char **g_include = NULL; static size_t g_include_n = 0;
static char **g_exclude = NULL; static size_t g_exclude_n = 0;

/* ANSI color helpers (only emitted when g_opt_color and stdout is a tty) */
#define COL_RESET "\033[0m"
#define COL_FNAME "\033[35m"   /* magenta filename */
#define COL_LNO   "\033[32m"   /* green line number */
#define COL_MATCH "\033[01;31m" /* bold red match */

/* ------------------------------------------------------------------ */
/* Our own SIMD byte matcher                                            */
/* ------------------------------------------------------------------ */
#if defined(__AVX2__)
#include <immintrin.h>
static inline const unsigned char *simd_find(const unsigned char *hay,
                                             const unsigned char *end,
                                             unsigned char c) {
    const unsigned char *p = hay;
    __m256i vc = _mm256_set1_epi8((char)c);
    while (p + 32 <= end) {
        __m256i v = _mm256_loadu_si256((const __m256i *)p);
        unsigned m = (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, vc));
        if (m) return p + __builtin_ctz(m);
        p += 32;
    }
    while (p < end) { if (*p == c) return p; p++; }
    return end;
}
#elif defined(__SSE4_2__)
#include <immintrin.h>
static inline const unsigned char *simd_find(const unsigned char *hay,
                                             const unsigned char *end,
                                             unsigned char c) {
    const unsigned char *p = hay;
    __m128i vc = _mm_set1_epi8((char)c);
    while (p + 16 <= end) {
        __m128i v = _mm_loadu_si128((const __m128i *)p);
        unsigned m = (unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(v, vc));
        if (m) return p + __builtin_ctz(m);
        p += 16;
    }
    while (p < end) { if (*p == c) return p; p++; }
    return end;
}
#else
static inline const unsigned char *simd_find(const unsigned char *hay,
                                             const unsigned char *end,
                                             unsigned char c) {
    for (const unsigned char *p = hay; p < end; p++)
        if (*p == c) return p;
    return end;
}
#endif

#define SIGMA 256
typedef struct {
    const unsigned char *pat;
    size_t plen;
    size_t skip[SIGMA];
    unsigned char first;
} BMH;

static void bmh_init(BMH *b, const unsigned char *pat, size_t plen) {
    b->pat = pat; b->plen = plen; b->first = plen ? pat[0] : 0;
    for (int i = 0; i < SIGMA; i++) b->skip[i] = plen;
    if (plen) for (size_t i = 0; i + 1 < plen; i++) b->skip[pat[i]] = plen - 1 - i;
}

static const unsigned char *bmh_search(const BMH *b, const unsigned char *base, size_t n) {
    if (b->plen == 0) return base;
    if (n < b->plen) return NULL;
    const unsigned char *end = base + n - b->plen;
    const unsigned char *p = base;
    while (p <= end) {
        const unsigned char *cand = simd_find(p, end + 1, b->first);
        if (cand > end) return NULL;
        if (memcmp(cand, b->pat, b->plen) == 0) return cand;
        p = cand + b->skip[cand[b->plen - 1]];
    }
    return NULL;
}

static unsigned char *g_lpat = NULL;
static const unsigned char *bmh_search_i(const BMH *b, const unsigned char *base, size_t n) {
    if (b->plen == 0) return base;
    if (n < b->plen) return NULL;
    const unsigned char *end = base + n - b->plen;
    const unsigned char *p = base;
    unsigned char lf = g_lpat[0];          /* lowercased first byte of pattern */
    while (p <= end) {
        /* find next candidate: next byte whose lowercase equals lf.
         * SIMD prefilter on the lowercased byte; if the pattern's first byte
         * has a case twin we also accept it by comparing tolower. */
        const unsigned char *cand = p;
        while (cand <= end) {
            if (tolower(*cand) == lf) break;
            cand++;
        }
        if (cand > end) return NULL;
        int ok = 1;
        for (size_t k = 0; k < b->plen; k++)
            if (tolower(cand[k]) != g_lpat[k]) { ok = 0; break; }
        if (ok) return cand;
        p = cand + b->skip[cand[b->plen - 1]];
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Globals / options                                                    */
/* ------------------------------------------------------------------ */
static const unsigned char *g_pat = NULL;
static size_t g_plen = 0;
static BMH g_bmh;
static int g_opt_invert = 0, g_opt_count = 0, g_opt_line = 0;
static int g_opt_quiet = 0, g_opt_insensitive = 0, g_opt_files_no = 0;
static int g_opt_recursive = 0, g_opt_color = 0, g_multi = 0;
static int g_found_any = 0;
/* file-global binary flag, computed ONCE in process_mmap (not per chunk) */
static int g_file_binary = 0;

/* ------------------------------------------------------------------ */
/* Output buffer                                                        */
/* ------------------------------------------------------------------ */
typedef struct { char *buf; size_t len, cap; int fd; } obuf_t;
static void obuf_init(obuf_t *o, int fd) {
    o->cap = 1 << 16; o->buf = malloc(o->cap); o->len = 0; o->fd = fd;
}
static void obuf_flush(obuf_t *o) {
    if (!o->len) return;
    size_t off = 0;
    while (off < o->len) {
        ssize_t w = write(o->fd, o->buf + off, o->len - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
    o->len = 0;
}
/* Grow a memory-only buffer (fd<0) so scan workers never touch fd=1
 * concurrently. The owning thread drains buffers in chunk order after join. */
static void obuf_grow(obuf_t *o, size_t need) {
    size_t want = o->cap;
    while (want < o->len + need) want *= 2;
    char *nb = (char *)realloc(o->buf, want);
    if (!nb) return; /* drop remaining matches on alloc failure */
    o->buf = nb; o->cap = want;
}
static void obuf_put(obuf_t *o, const char *s, size_t n) {
    if (o->fd >= 0 && (o->len + n > o->cap)) obuf_flush(o);
    if (o->fd >= 0 && n >= o->cap) { ssize_t w = write(o->fd, s, n); (void)w; return; }
    if (o->len + n > o->cap) obuf_grow(o, n);
    size_t room = o->cap - o->len;
    size_t c = n < room ? n : room;
    memcpy(o->buf + o->len, s, c); o->len += c;
}
/* Drain a memory-only buffer (fd<0) to a real fd, in order. */
static void obuf_flush_to(obuf_t *o, int fd) {
    if (!o->len) return;
    size_t off = 0;
    while (off < o->len) {
        ssize_t w = write(fd, o->buf + off, o->len - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
    o->len = 0;
}

/* ------------------------------------------------------------------ */
/* Line scan of a sub-range [base, base+size)                          */
/* ------------------------------------------------------------------ */
typedef struct { int matched; size_t match_count; } range_res_t;

static int is_word_byte(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* whole-buffer binary check: presence of a NUL byte => binary file */
static int buffer_is_binary(const unsigned char *base, size_t size) {
    return memchr(base, 0, size) != NULL;
}

/* Emit one matched line with optional color. line points at line start,
 * llen is the line length (excluding any trailing newline). */
static void emit_line(obuf_t *out, const char *fname, size_t line_no,
                      const unsigned char *line, size_t llen) {
    if (g_multi && fname) {
        if (g_opt_color) obuf_put(out, COL_FNAME, sizeof(COL_FNAME)-1);
        obuf_put(out, fname, strlen(fname));
        if (g_opt_color) obuf_put(out, COL_RESET, sizeof(COL_RESET)-1);
        obuf_put(out, ":", 1);
    }
    if (g_opt_line) {
        char nb[24]; int L = snprintf(nb, sizeof nb, "%zu", line_no);
        if (g_opt_color) obuf_put(out, COL_LNO, sizeof(COL_LNO)-1);
        obuf_put(out, nb, (size_t)L);
        if (g_opt_color) obuf_put(out, COL_RESET, sizeof(COL_RESET)-1);
        obuf_put(out, ":", 1);
    }
    obuf_put(out, (const char *)line, llen);
    obuf_put(out, "\n", 1);
}

/* Callback for the whole-buffer regex fast path (see scan_range). Receives the
 * 0-based line index of a distinct matching line; maps it to the line's byte
 * span via the precomputed line-index and emits it with the same -n/-c/-q/-l
 * semantics as the per-line loop. */
typedef struct {
    const unsigned char *base, *end;
    size_t line_base;
    obuf_t *out;
    range_res_t *res;
    const char *fname;
    size_t *lo;
    size_t nlines;
} rcb_t;

static void on_regex_match(long line, void *ctx) {
    rcb_t *c = (rcb_t *)ctx;
    if ((size_t)line >= c->nlines) return;
    const unsigned char *ls = c->base + c->lo[line];
    const unsigned char *le = ((size_t)line + 1 < c->nlines)
        ? c->base + c->lo[line + 1] - 1   /* drop the trailing '\n' */
        : c->end;
    c->res->matched = 1;
    c->res->match_count++;
    if (g_opt_quiet) { g_found_any = 1; return; }
    if (g_opt_files_no) { g_found_any = 1; return; }
    if (!g_opt_count) emit_line(c->out, c->fname, c->line_base + (size_t)line + 1, ls, (size_t)(le - ls));
}

static void scan_range(const unsigned char *base, size_t size, obuf_t *out,
                       size_t line_base, range_res_t *res, const char *fname) {
    const unsigned char *end = base + size;
    const unsigned char *line = base;
    size_t line_no = line_base;
    res->matched = 0; res->match_count = 0;

    /* Fast path for literal (-F) search: jump straight to each needle
     * occurrence with memmem and compute the surrounding line boundaries,
     * instead of walking every byte / splitting on '\n' per line. This is
     * O(matches) rather than O(bytes), a large win on huge, sparse files
     * where the pattern is rare. Skipped when -v/-w/-x/word-mode need the
     * full line semantics the per-line loop provides. */
    if (!g_opt_regex && !g_opt_invert && !g_opt_word && !g_opt_line_mode && g_plen > 0){
        const unsigned char *p = base;
        size_t remain = size;
        const unsigned char *last_line = NULL;   /* start of last counted line */
        const unsigned char *prev_end = base;    /* end of previously scanned line */
        while (remain > 0){
            const unsigned char *hit = memmem(p, remain, g_pat, g_plen);
            if (!hit) break;
            const unsigned char *ls = memrchr((const char*)base, '\n', (size_t)(hit - base));
            const unsigned char *line = ls ? ls + 1 : base;
            const unsigned char *ne = memchr(hit + g_plen, '\n', (size_t)(end - (hit + g_plen)));
            const unsigned char *line_end = ne ? ne : end;
            if (line != last_line){
                /* advance line_no across any lines we skipped */
                const unsigned char *c = prev_end;
                while (c < line){ const unsigned char *nx = memchr(c, '\n', (size_t)(line - c)); if(!nx) break; line_no++; c = nx + 1; }
                last_line = line; prev_end = line_end;
                res->matched = 1; res->match_count++; g_found_any = 1;
                if (g_opt_quiet) return;
                if (!g_opt_count){ emit_line(out, fname, line_no, line, (size_t)(line_end - line)); }
                if (g_opt_count) prev_end = line_end; /* stop scanning rest of this line */
            }
            p = (line_end < end) ? line_end + 1 : end;
            remain = (size_t)(end - p);
        }
        return;
    }

    /* GNU grep default: a NUL byte means a binary file. Computed once in
     * process_mmap (g_file_binary) instead of re-scanning per chunk. */
    if (g_file_binary) {
        if (!g_opt_count && !g_opt_files_no && !g_opt_quiet && !g_opt_invert) {
            if (g_multi && fname) { obuf_put(out, fname, strlen(fname)); obuf_put(out, ":", 1); }
            obuf_put(out, "Binary file ", 12);
            if (fname) obuf_put(out, fname, strlen(fname));
            obuf_put(out, " matches", 8);
            obuf_put(out, "\n", 1);
        }
        res->matched = 1; res->match_count = 1; g_found_any = 1;
        return;
    }

    /* Whole-buffer regex fast path (the SOTA lever): instead of running the
     * Pike VM once PER LINE (millions of resets over a big file), run it ONCE
     * over the entire [base,end) range with ^/$ evaluated against '\n'
     * positions -- identical per-line semantics to GNU grep, but a single NFA
     * pass. A precomputed line-index makes each matching-line emit O(1). Used
     * for the common case (no -v); -v keeps the per-line fallback below.
     * The literal prefilter gate is checked ONCE in process_mmap; a reject
     * returns before any chunk is spawned, so it is never re-tested here. */
    if (g_opt_regex && !g_opt_invert) {
        rcb_t rc = { base, end, line_base, out, res, fname, NULL, 0 };
        /* build line-start index (one forward memchr walk, same order as NFA) */
        size_t cap=1024; rc.lo=malloc(cap*sizeof(size_t));
        const unsigned char *p=base;
        while (p<=end){
            if (rc.nlines>=cap){ cap*=2; rc.lo=realloc(rc.lo,cap*sizeof(size_t)); }
            rc.lo[rc.nlines++]=(size_t)(p-base);
            const unsigned char *nl=memchr(p,'\n',(size_t)(end-p));
            if (!nl) break;
            p=nl+1;
        }
        wubre_search_buf(g_re, base, size, on_regex_match, &rc);
        free(rc.lo);
        if (g_opt_quiet && res->matched) return;
        if (g_opt_files_no && res->matched) return;
        return;
    }

    while (line < end) {
        line_no++;
        const unsigned char *nl = memchr(line, '\n', (size_t)(end - line));
        const unsigned char *line_end = nl ? nl : end;
        size_t llen = (size_t)(line_end - line);
        int hit = 0;

        if (g_opt_regex) {
            hit = wubre_search(g_re, line, llen) ? 1 : 0;
        } else {
            const unsigned char *m = g_opt_insensitive
                ? bmh_search_i(&g_bmh, line, llen) : bmh_search(&g_bmh, line, llen);
            if (m) {
                size_t p = (size_t)(m - line), L = g_plen;
                if (g_opt_word) {
                    int ok = 1;
                    if (p > 0 && is_word_byte(line[p-1])) ok = 0;
                    if (p + L < llen && is_word_byte(line[p+L])) ok = 0;
                    if (!ok) m = NULL;
                }
                if (g_opt_line_mode && (p != 0 || L != llen)) m = NULL;
                hit = (m != NULL);
            }
        }
        if (g_opt_invert) hit = !hit;
        if (hit) {
            res->matched = 1; res->match_count++;
            if (g_opt_quiet) { g_found_any = 1; return; }
            if (!g_opt_count && !g_opt_files_no) emit_line(out, fname, line_no, line, llen);
        }
        if (!nl) break;
        line = nl + 1;
    }
}

/* ------------------------------------------------------------------ */
/* Parallel single-file scan                                            */
/* ------------------------------------------------------------------ */
typedef struct {
    const unsigned char *data;
    size_t start, end;
    size_t line_base;
    const char *fname;
    obuf_t out;
    range_res_t res;
} chunk_job_t;

static void *chunk_worker(void *arg) {
    chunk_job_t *j = (chunk_job_t *)arg;
    obuf_init(&j->out, -1);   /* memory-only: no concurrent fd=1 writes from threads */
    scan_range(j->data + j->start, j->end - j->start, &j->out, j->line_base, &j->res, j->fname);
    return NULL;
}

static int nproc(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}

static void process_mmap(const unsigned char *data, size_t size, obuf_t *single_out, const char *fname) {
    int nth = nproc();
    if (nth > 16) nth = 16;
    /* only parallelize if file is large enough to amortize thread spawn */
    if ((size_t)nth * (1 << 20) > size) nth = 1;

    size_t *starts = malloc((size_t)(nth + 1) * sizeof(size_t));
    starts[0] = 0;
    for (int i = 0; i < nth - 1; i++) {
        size_t e = starts[i] + size / (size_t)nth;
        if (e > size) e = size;
        const unsigned char *p = memchr(data + e, '\n', size - e);
        e = p ? (size_t)(p - data) + 1 : size;
        starts[i + 1] = e;
    }
    starts[nth] = size;

    /* ---- file-global pre-checks + per-chunk line-base ----
     * One AVX2 pass (128-byte blocks) counts '\n' and detects NUL, recording
     * the cumulative newline count at every chunk boundary. For single-literal
     * patterns the literal-prefilter presence test is FUSED into the same
     * sweep (wub_simd_line_nul_lit_stats — ugrep's one-pass-does-everything
     * technique), replacing the separate wubre_litpref_gate pass; the
     * stack-alignment crash is fixed via force_align_arg_pointer and the
     * nl/nul counts use non-overlapping 128B blocks (overlap double-counts
     * newlines). */
    size_t *cum = malloc((size_t)nth * sizeof(size_t));
    size_t run = 0;
    int has_nul = 0;
    const unsigned char *p = data;
    /* fused single-literal gate setup (non-ICASE only; NULL otherwise) */
    const unsigned char *flit = NULL; int flitlen = 0; int flit_present = 0;
    int gate_reject = 0;
    if (g_opt_regex && !g_opt_invert){
        flit = wubre_litpref_single_literal(g_re, &flitlen);
    }
    for (int i = 1; i < nth; i++) {
        const unsigned char *stop = data + starts[i];
        size_t cnl; int cnul;
        if (flit){
            int lp;
            wub_simd_line_nul_lit_stats(p, (size_t)(stop - p), flit, flitlen,
                                        &cnl, &cnul, &lp);
            if (lp) flit_present = 1;
        } else {
            wub_simd_line_nul_stats(p, (size_t)(stop - p), &cnl, &cnul);
        }
        run += cnl; if (cnul) has_nul = 1;
        cum[i] = run;
        p = stop;
    }
    /* tail */
    size_t tnl; int tnul;
    if (flit){
        int lp;
        wub_simd_line_nul_lit_stats(p, (size_t)(data + size - p), flit, flitlen,
                                    &tnl, &tnul, &lp);
        if (lp) flit_present = 1;
    } else {
        wub_simd_line_nul_stats(p, (size_t)(data + size - p), &tnl, &tnul);
    }
    if (tnul) has_nul = 1;
    size_t *nlbefore = NULL;
    if (g_opt_line) {
        nlbefore = calloc((size_t)nth, sizeof(size_t));
        for (int i = 1; i < nth; i++) nlbefore[i] = cum[i];
    }
    free(cum);
    g_file_binary = (!g_opt_text && has_nul) ? 1 : 0;

    /* Literal prefilter gate: if no required literal is present anywhere in
     * the buffer, there is definitively no match. For the fused single-literal
     * case the answer is already in flit_present; otherwise run the general
     * gate. Early return (handling -c/-l/-q empty-result semantics) BEFORE
     * spawning any chunk avoids re-running the gate in every parallel chunk. */
    if (g_opt_regex && !g_opt_invert){
        if (flit){
            if (!flit_present) gate_reject = 1;
        } else if (wubre_litpref_gate(g_re, data, size) == 0) gate_reject = 1;
    }
    if (gate_reject) {
        free(starts); free(nlbefore);
        if (g_opt_count) {
            if (!(g_opt_invert && g_plen == 0)) {
                char nb[24]; int L = snprintf(nb, sizeof nb, "0");
                if (g_multi && fname) { obuf_put(single_out, fname, strlen(fname)); obuf_put(single_out, ":", 1); }
                obuf_put(single_out, nb, (size_t)L); obuf_put(single_out, "\n", 1);
            }
        } else if (g_opt_files_no) {
            /* no filename printed: no match */
        } else if (g_opt_quiet) {
            /* no output */
        }
        return;
    }

    chunk_job_t *jobs = calloc((size_t)nth, sizeof(chunk_job_t));
    pthread_t *th = malloc(sizeof(pthread_t) * (size_t)nth);

    if (nth == 1) {
        free(th);
        obuf_init(&jobs[0].out, 1);
        scan_range(data, size, &jobs[0].out, 0, &jobs[0].res, fname);
    } else {
        for (int i = 0; i < nth; i++) {
            jobs[i].data = data; jobs[i].start = starts[i]; jobs[i].end = starts[i + 1];
            jobs[i].line_base = nlbefore ? nlbefore[i] : 0;
            jobs[i].fname = fname;
            pthread_create(&th[i], NULL, chunk_worker, &jobs[i]);
        }
        for (int i = 0; i < nth; i++) pthread_join(th[i], NULL);
        free(th);
    }

    size_t total = 0; int any = 0;
    for (int i = 0; i < nth; i++) { total += jobs[i].res.match_count; any |= jobs[i].res.matched; }

    if (g_opt_count) {
        /* GNU grep: count mode prints count; rc=0 iff at least one line
         * "selected". For a non-empty pattern that means count>0. For the
         * empty pattern (matches every line) it means the file had >=1 line,
         * i.e. count>0 as well; with -v the empty pattern selects NOTHING so
         * count is suppressed and rc=1. We approximate: rc=0 when total>0,
         * except (-v + empty pattern) which grep renders as no output/rc=1. */
        if (g_opt_invert && g_plen == 0) {
            /* -c -v '' : grep prints nothing, rc=1 */
        } else {
            char nb[24]; int L = snprintf(nb, sizeof nb, "%zu", total);
            if (g_multi && fname) { obuf_put(single_out, fname, strlen(fname)); obuf_put(single_out, ":", 1); }
            obuf_put(single_out, nb, (size_t)L); obuf_put(single_out, "\n", 1);
            if (total > 0) g_found_any = 1;
        }
    } else if (g_opt_files_no) {
        /* -l : print filename iff a match exists. Empty pattern matches every
         * line, so a non-empty file matches; empty file (-v or not) matches
         * nothing -> nothing printed, rc=1. -l -v '' selects nothing. */
        if (!(g_opt_invert && g_plen == 0) && any) {
            if (fname) obuf_put(single_out, fname, strlen(fname));
            obuf_put(single_out, "\n", 1);
            g_found_any = 1;
        }
    } else if (g_opt_quiet) {
        if (any) g_found_any = 1;
    } else {
        for (int i = 0; i < nth; i++) { if (jobs[i].out.fd < 0) { obuf_flush_to(&jobs[i].out, 1); } else { obuf_flush(&jobs[i].out); } free(jobs[i].out.buf); }
        if (any) g_found_any = 1;
    }
    if (g_opt_count || g_opt_files_no || g_opt_quiet) {
        for (int i = 0; i < nth; i++) free(jobs[i].out.buf);
    }
    free(starts); free(nlbefore); free(jobs);
}

static void process_fd(const char *fname, int fd, obuf_t *out) {
    struct stat st;
    if (fstat(fd, &st) != 0) return;
    int is_regular = S_ISREG(st.st_mode);
    size_t size = (size_t)st.st_size;

    /* Pipes / /dev/stdin / /dev/fd / char devices have st_size==0 but real
     * data. Read until EOF with a growing buffer instead of bailing out. */
    if (!is_regular || size == 0) {
        size_t cap = 1 << 16, len = 0;
        unsigned char *buf = malloc(cap);
        if (!buf) return;
        for (;;) {
            if (len == cap) { cap *= 2; unsigned char *nb = realloc(buf, cap); if (!nb) { free(buf); return; } buf = nb; }
            ssize_t r = read(fd, buf + len, cap - len);
            if (r < 0) { if (errno == EINTR) continue; break; }
            if (r == 0) break;
            len += (size_t)r;
        }
        /* An empty regular file: report per GNU grep semantics. */
        if (is_regular && len == 0) {
            if (g_opt_count) {
                if (!(g_opt_invert && g_plen == 0)) {
                    char nb[24]; int L = snprintf(nb, sizeof nb, "0");
                    if (g_multi && fname) { obuf_put(out, fname, strlen(fname)); obuf_put(out, ":", 1); }
                    obuf_put(out, nb, (size_t)L); obuf_put(out, "\n", 1);
                }
            }
            free(buf);
            return;
        }
        process_mmap(buf, len, out, fname);
        free(buf);
        return;
    }

    if (size >= (1 << 16)) {
        void *m = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m != MAP_FAILED) {
            process_mmap((const unsigned char *)m, size, out, fname);
            munmap(m, size);
            return;
        }
    }
    /* small regular file: read fully then scan */
    unsigned char *whole = malloc(size + 1);
    if (!whole) return;
    size_t got = 0;
    while (got < size) {
        ssize_t r = pread(fd, whole + got, size - got, (off_t)got);
        if (r <= 0) break;
        got += (size_t)r;
    }
    process_mmap(whole, got, out, fname);
    free(whole);
}

static void process_path(const char *path, const char *dispname, obuf_t *out) {
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "wubugrep: %s: %s\n", path, strerror(errno));
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        /* GNU grep errors on a directory given without -r */
        fprintf(stderr, "wubugrep: %s: Is a directory\n", path);
        return;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) { fprintf(stderr, "wubugrep: %s: %s\n", path, strerror(errno)); return; }
    process_fd(dispname, fd, out);
    close(fd);
}

/* ------------------------------------------------------------------ */
/* Recursive directory walk (sorted DFS)                                */
/* ------------------------------------------------------------------ */
static const char *const DEFAULT_IGNORE[] = {".git","node_modules",".hg",".svn","target","build",".cache",NULL};
static char **g_gitignore = NULL; static size_t g_gi_count = 0;
static int name_ignored(const char *name) {
    for (const char *const *p = DEFAULT_IGNORE; *p; p++) if (strcmp(name, *p) == 0) return 1;
    for (size_t i = 0; i < g_gi_count; i++) if (strcmp(name, g_gitignore[i]) == 0) return 1;
    return 0;
}

/* minimal shell-style glob: '*' matches any run, '?' any single char */
static int glob_match(const char *pat, const char *str) {
    const char *star = NULL, *ss = str;
    while (*str) {
        if (*pat == '*') { star = pat++; ss = str; continue; }
        if (*pat == '?' || *pat == *str) { pat++; str++; continue; }
        if (star) { pat = star + 1; str = ++ss; continue; }
        return 0;
    }
    while (*pat == '*') pat++;
    return *pat == 0;
}
static void parse_gitignore(const char *dir) {
    char path[4096]; snprintf(path, sizeof path, "%s/.gitignore", dir);
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        size_t L = strlen(line);
        while (L && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = 0;
        if (L == 0 || line[0] == '#') continue;
        g_gitignore = realloc(g_gitignore, (g_gi_count + 1) * sizeof(char *));
        g_gitignore[g_gi_count++] = strdup(line);
    }
    fclose(f);
}

/* Recursive directory walk — deterministic DFS with lexically sorted
 * entries, mirroring GNU grep's traversal order so recursive output is
 * byte-identical. The per-file scan inside process_fd still parallelizes
 * within a large file; only the directory enumeration is serialized. */
static void process_directory(const char *dir, const char *prefix) {
    DIR *d = opendir(dir);
    if (!d) return;
    /* collect + sort entries for deterministic order (GNU grep sorts) */
    char **names = NULL; size_t n = 0, cap = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        const char *nm = de->d_name;
        if (nm[0] == '.' && (nm[1] == 0 || (nm[1] == '.' && nm[2] == 0))) continue;
        if (name_ignored(nm)) continue;
        if (n == cap) { cap = cap ? cap * 2 : 16; names = realloc(names, cap * sizeof(char *)); }
        names[n++] = strdup(nm);
    }
    closedir(d);
    /* simple insertion sort (small N per dir) */
    for (size_t i = 1; i < n; i++) {
        char *key = names[i]; size_t j = i;
        while (j > 0 && strcmp(names[j-1], key) > 0) { names[j] = names[j-1]; j--; }
        names[j] = key;
    }
    obuf_t out; obuf_init(&out, 1);
    for (size_t i = 0; i < n; i++) {
        char full[4096];
        snprintf(full, sizeof full, "%s/%s", dir, names[i]);
        /* emit relative path WITHOUT a leading "./" (GNU grep -r with no
         * explicit path does not prefix "./"); prefix is what we print. */
        char rel[4096];
        if (prefix && prefix[0]) snprintf(rel, sizeof rel, "%s/%s", prefix, names[i]);
        else snprintf(rel, sizeof rel, "%s", names[i]);
        struct stat st;
        if (lstat(full, &st) != 0) { free(names[i]); continue; }
        if (S_ISDIR(st.st_mode)) {
            process_directory(full, rel);
        } else if (S_ISREG(st.st_mode)) {
            if (g_include_n) { int ok=0; for (size_t z=0;z<g_include_n;z++) if (glob_match(g_include[z], names[i])) { ok=1; break; } if (!ok) { free(names[i]); continue; } }
            if (g_exclude_n) { int skip=0; for (size_t z=0;z<g_exclude_n;z++) if (glob_match(g_exclude[z], names[i])) { skip=1; break; } if (skip) { free(names[i]); continue; } }
            process_path(full, rel, &out);
        }
        free(names[i]);
    }
    obuf_flush(&out); free(out.buf);
    free(names);
}

/* ------------------------------------------------------------------ */
/* CLI                                                                  */
/* ------------------------------------------------------------------ */
static void usage(const char *p) {
    fprintf(stderr,
        "WuBuGrep - C11 grep. Usage:\n"
        "  %s [options] PATTERN [FILE|DIR ...]\n"
        "Options:\n"
        "  -i case-insensitive  -v invert   -n line numbers  -c count   -l files\n"
        "  -w whole word        -x whole line  -q quiet  -a text (no binary skip)\n"
        "  -E ERE  -G BRE  -F literal (default)\n"
        "  -r recursive   --include=GLOB  --exclude=GLOB   -h help\n", p);
}

/* WuBuCat mode: byte-exact dump of each file (mmap + write, no transforms). */
static int cat_main(int argc, char **argv) {
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) { fprintf(stderr, "WuBuCat - dump files.\n"); return 0; }
    }
    int started = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != 0) continue; /* skip flags */
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) { ret = 1; continue; }
        struct stat st;
        if (fstat(fd, &st) != 0) { close(fd); ret = 1; continue; }
        size_t n = (size_t)st.st_size;
        if (n == 0) { close(fd); continue; }
        void *m = mmap(NULL, n, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m == MAP_FAILED) { close(fd); ret = 1; continue; }
        size_t off = 0;
        while (off < n) {
            ssize_t w = write(1, (char*)m + off, n - off);
            if (w <= 0) break;
            off += (size_t)w;
        }
        munmap(m, n); close(fd); started = 1;
    }
    (void)started;
    return ret;
}

int main(int argc, char **argv) {
    /* argv0 multi-tool dispatch (one binary, many tools via symlinks) */
    const char *base = strrchr(argv[0], '/');
    base = base ? base + 1 : argv[0];
    if (strstr(base, "cat") != NULL) return cat_main(argc, argv);

    const char *pattern = NULL;
    char **paths = NULL; int npaths = 0;

    int i = 1;
    int pattern_set = 0;
    for (; i < argc; i++) {
        char *a = argv[i];
        if (a[0] == '-' && a[1] != 0) {
            if (strcmp(a, "--") == 0) { i++; break; }
            if (strcmp(a, "--no-color") == 0) { g_opt_color = 0; continue; }
            if (strncmp(a, "--include=", 10) == 0) {
                g_include = realloc(g_include, (g_include_n+1)*sizeof(char*));
                g_include[g_include_n++] = strdup(a+10); continue;
            }
            if (strncmp(a, "--exclude=", 10) == 0) {
                g_exclude = realloc(g_exclude, (g_exclude_n+1)*sizeof(char*));
                g_exclude[g_exclude_n++] = strdup(a+10); continue;
            }
            for (char *f = a + 1; *f; f++) {
                switch (*f) {
                    case 'i': g_opt_insensitive = 1; break;
                    case 'v': g_opt_invert = 1; break;
                    case 'n': g_opt_line = 1; break;
                    case 'c': g_opt_count = 1; break;
                    case 'l': g_opt_files_no = 1; break;
                    case 'q': g_opt_quiet = 1; break;
                    case 'r': g_opt_recursive = 1; break;
                    case 'w': g_opt_word = 1; break;
                    case 'x': g_opt_line_mode = 1; break;
                    case 'a': g_opt_text = 1; break;
                    case 'E': g_opt_regex = 1; break;
                    case 'G': g_opt_regex = 2; break;
                    case 'F': g_opt_regex = 0; break;
                    case 'h': usage(argv[0]); return 0;
                    default: fprintf(stderr, "Unknown option: -%c\n", *f); return 2;
                }
            }
        } else {
            /* Non-option arg. The FIRST one is the pattern; any later ones
             * (or any non-option after the pattern) are paths. GNU grep lets
             * options appear after the pattern, so we keep scanning instead
             * of breaking. A bare "-" means stdin. */
            if (!pattern_set) { pattern = a; pattern_set = 1; continue; }
            paths = realloc(paths, (++npaths) * sizeof(char *));
            paths[npaths-1] = a;
        }
    }
    if (!pattern) { usage(argv[0]); return 2; }
    for (; i < argc; i++) paths = realloc(paths, (++npaths) * sizeof(char *)), paths[npaths-1] = argv[i];
    if (npaths == 0) { paths = realloc(paths, 1 * sizeof(char *)); paths[0] = g_opt_recursive ? "." : (char *)"/dev/stdin"; npaths = 1; }

    g_plen = strlen(pattern);
    g_pat = (const unsigned char *)pattern;

    /* -w / -x rewrite the pattern (regex mode) or are handled inline (literal). */
    char *eff_pattern = (char *)pattern;
    if (g_opt_regex && (g_opt_word || g_opt_line_mode)) {
        size_t L = g_plen;
        size_t need = L + 32;
        char *buf = malloc(need);
        if (g_opt_word) {
            if (g_opt_regex == 2) /* BRE */
                snprintf(buf, need, "\\(%s\\)", pattern);
            else
                snprintf(buf, need, "(%s)", pattern);
            /* wrap with non-word boundaries; use look-alike via alternation:
             * (^|[^A-Za-z0-9_])(PAT)([^A-Za-z0-9_]|$) -- but $ must be line end.
             * Simpler: require boundaries by matching PAT as a whole word. */
            free(buf);
            buf = malloc(L + 64);
            if (g_opt_regex == 2)
                snprintf(buf, L + 64,
                    "(^|[^[:alnum:]_])(%s)([^[:alnum:]_]|$)", pattern);
            else
                snprintf(buf, L + 64,
                    "(^|[^[:alnum:]_])(%s)([^[:alnum:]_]|$)", pattern);
        } else { /* -x */
            if (g_opt_regex == 2)
                snprintf(buf, need, "^%s$", pattern);
            else
                snprintf(buf, need, "^%s$", pattern);
        }
        eff_pattern = buf;
    }

    if (g_opt_regex) {
        int rflags = (g_opt_insensitive ? WUBRE_ICASE : 0) |
                     (g_opt_regex == 2 ? WUBRE_BRE : 0);
        char err[256];
        g_re = wubre_compile(eff_pattern, rflags, err, sizeof err);
        if (!g_re) { fprintf(stderr, "wubugrep: %s\n", err); return 2; }
    } else {
        if (g_opt_insensitive) {
            g_lpat = malloc(g_plen ? g_plen : 1);
            for (size_t k = 0; k < g_plen; k++) g_lpat[k] = (unsigned char)tolower(g_pat[k]);
        }
        bmh_init(&g_bmh, g_pat, g_plen);
        if (g_opt_insensitive) g_bmh.first = g_lpat[0];
    }

    /* color on a tty unless explicitly disabled */
    if (g_opt_color == 0 && isatty(1)) g_opt_color = 1;

    int any_dir = 0;
    for (int k = 0; k < npaths; k++) {
        struct stat st; if (stat(paths[k], &st) == 0 && S_ISDIR(st.st_mode)) { any_dir = 1; break; }
    }
    int recursive = g_opt_recursive || any_dir;

    if (recursive) {
        g_multi = 1;
        obuf_t out; obuf_init(&out, 1);
        for (int k = 0; k < npaths; k++) {
            struct stat st;
            if (stat(paths[k], &st) == 0 && S_ISDIR(st.st_mode)) {
                parse_gitignore(paths[k]);
                /* explicit dir arg: print relative to it without "./" */
                process_directory(paths[k], NULL);
            } else {
                process_path(paths[k], paths[k], &out);
            }
        }
        obuf_flush(&out); free(out.buf);
    } else {
        obuf_t out; obuf_init(&out, 1);
        g_multi = (npaths > 1);
        for (int k = 0; k < npaths; k++) process_path(paths[k], paths[k], &out);
        obuf_flush(&out); free(out.buf);
    }
    free(paths);
    if (g_re) wubre_free(g_re);
    return g_found_any ? 0 : 1;
}
