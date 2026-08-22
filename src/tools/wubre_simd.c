/*
 * wubre_simd.c - Our own SIMD literal search (no vectorscan dependency).
 * ---------------------------------------------------------------------------
 * Two accelerators, both in-house C11 + AVX2 intrinsics:
 *   1. wub_memmem_avx2  - 32B/cycle first-byte scan for a required literal.
 *   2. wub_simd_find2   - "Teddy"-style two-literal windowing for X.*Y:
 *                         SIMD-scans for the FIRST literal, then verifies the
 *                         SECOND literal within a bounded forward window, so the
 *                         NFA only runs inside confirmed literal-bounded regions
 *                         (the Hyperscan "literal acceleration" model).
 * The functions are target('avx2') so the TU compiles on any x86-64; a scalar
 * fallback is provided for non-AVX2 paths.
 * License: WaefreBeorn Umbrella License v3.0
 * ---------------------------------------------------------------------------
 */
#include "wubre_internal.h"
#include <immintrin.h>

/* ---- 1. required-literal memmem (32 bytes/cycle) ---- */
__attribute__((target("avx2,popcnt,bmi,bmi2")))
static const unsigned char *wub_memmem_avx2(const unsigned char *hay, size_t hn,
                                           const unsigned char *needle, size_t nn){
    if (nn==0) return hay;
    if (nn>hn) return NULL;
    if (nn==1) return (const unsigned char*)memchr(hay, needle[0], hn);
    unsigned char fch = needle[0];
    __m256i vf = _mm256_set1_epi8((char)fch);
    size_t i=0;
    size_t lim = hn - nn;
    while (i + 32 <= lim + nn){
        __m256i blk = _mm256_loadu_si256((const __m256i*)(hay+i));
        __m256i cmp = _mm256_cmpeq_epi8(blk, vf);
        unsigned mask = (unsigned)_mm256_movemask_epi8(cmp);
        while (mask){
            int bit = __builtin_ctz(mask);
            const unsigned char *c = hay + i + bit;
            size_t off=(size_t)(c-hay);
            if (off+nn<=hn){
                size_t k=1; for(;k<nn;k++) if(c[k]!=needle[k]) break;
                if (k==nn) return c;
            }
            mask &= mask-1;
        }
        i += 32;
        if (i > lim) break;
    }
    for (size_t j=0; j+nn<=hn; j++){
        if (hay[j]==fch){
            size_t k=1; for(;k<nn;k++) if(hay[j+k]!=needle[k]) break;
            if (k==nn) return hay+j;
        }
    }
    return NULL;
}

const unsigned char *wub_memmem(const unsigned char *hay, size_t hn,
                                const unsigned char *needle, size_t nn){
#if defined(__x86_64__) || defined(__i386__)
    if (nn>=2 && nn<=16) return wub_memmem_avx2(hay,hn,needle,nn);
#endif
    /* scalar fallback for nn>16 or non-AVX2 platforms */
    if (nn==0) return hay;
    if (nn>hn) return NULL;
    unsigned char fch = needle[0];
    const unsigned char *p = hay;
    size_t rem = hn;
    while (rem >= nn) {
        const unsigned char *c = memchr(p, fch, rem);
        if (!c) return NULL;
        size_t off = (size_t)(c - hay);
        if (off + nn > hn) return NULL;
        size_t i=1; for (; i<nn; i++) if (c[i]!=needle[i]) break;
        if (i==nn) return c;
        p = c + 1; rem = hn - (size_t)(p - hay);
    }
    return NULL;
}
/* ---- 4. single-pass literal scanner (the ripgrep/Teddy model) ----
 * One AVX2 sweep over the whole buffer finds every occurrence of `needle` and
 * reports the 0-based line index of each DISTINCT matching line via on_match
 * (grep -n semantics). Newlines are counted in the SAME block scan with a
 * second SIMD compare (no per-match memchr), so the whole pass is O(bytes) at
 * memory bandwidth -- not O(hits x n). */
__attribute__((target("avx2,popcnt,bmi,bmi2")))
void wub_simd_scan_literal(const unsigned char *hay, size_t hn,
                           const unsigned char *needle, size_t nn,
                           void (*on_match)(long line, void *ctx), void *ctx){
    if (nn==0 || nn>hn) return;
    unsigned char fch = needle[0];
    __m256i vf = _mm256_set1_epi8((char)fch);
    __m256i vnl = _mm256_set1_epi8((char)'\n');
    long cur = 0;          /* newlines seen so far (== line index of next byte) */
    long last = -1;
    size_t i = 0;
    size_t lim = hn - nn;  /* needle may start up to here */
    while (i + 32 <= lim + nn){
        __m256i blk = _mm256_loadu_si256((const __m256i*)(hay + i));
        unsigned m  = (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(blk, vf));
        unsigned nl = (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(blk, vnl));
        /* For each needle first-byte candidate in this block */
        while (m){
            int bit = __builtin_ctz(m);
            size_t off = i + (size_t)bit;
            /* newlines strictly before this match within the block */
            unsigned nl_before = nl & ((1u << bit) - 1u);
            long line = cur + (long)__builtin_popcount(nl_before);
            if (off + nn > hn){ m &= m - 1; continue; }
            const unsigned char *c = hay + off;
            size_t k = 1; for (; k < nn; k++) if (c[k] != needle[k]) break;
            if (k == nn && line != last){ last = line; if (on_match) on_match(line, ctx); }
            m &= m - 1;
        }
        /* advance line counter by newlines in the whole block */
        cur += (long)__builtin_popcount(nl);
        i += 32;
    }
    /* scalar tail */
    for (size_t j = i; j + nn <= hn; j++){
        if (hay[j] == fch){
            size_t k = 1; for (; k < nn; k++) if (hay[j+k] != needle[k]) break;
            if (k == nn){
                long line = cur;
                for (size_t t = i; t < j; t++) if (hay[t] == '\n') line++;
                if (line != last){ last = line; if (on_match) on_match(line, ctx); }
            }
        }
    }
}

/* ---- 2. "Teddy"-style two-literal windowing for X.*Y patterns ----
 * The caller (wubre_search) has already extracted two mandatory literals
 * (litA, litB, both length>=2) from a pattern such as  foo.*bar.  We SIMD-scan
 * for litA; for each hit we check litB occurs within a bounded forward window
 * (window bytes). If so, the NFA is invoked only on [hitA .. hitB+lenB). This
 * collapses a pathological O(n) ".*" scan into a literal-gated O(matches) pass.
 * Returns 1 if a literal-bounded window exists anywhere, 0 otherwise. ---- */
/* ---- 3. SIMD window-line scanner for exact  LIT .* LIT  patterns ----
 * Replaces the NFA entirely for patterns of the form  A.*B  (two literal runs
 * with nothing else). A line matches iff litA (la) occurs before litB (lb)
 * within the SAME line ('.' never crosses '\n', matching GNU grep). We SIMD-
 * scan for every litA occurrence, verify litB in the remainder of that line,
 * and report the 0-based line index of each match via on_match. This is the
 * Hyperscan/vectorscan "literal acceleration" model realized in pure C11+AVX2:
 * an O(occurrences-of-la) SIMD pass instead of an O(bytes) scalar NFA. ---- */
__attribute__((target("avx2,popcnt,bmi,bmi2")))
void wub_simd_scan_windows(const unsigned char *buf, size_t n,
                           const unsigned char *la, size_t la_n,
                           const unsigned char *lb, size_t lb_n,
                           void (*on_match)(long line, void *ctx), void *ctx){
    if (la_n==0 || lb_n==0 || n < la_n) return;
    __m256i va = _mm256_set1_epi8((char)la[0]);
    long cur_line = 0;
    long last_line = -1;
    size_t scan_pos = 0;        /* next newline count advances from here */
    size_t i = 0;
    size_t lim = n - la_n;      /* la may start up to here */
    while (i + 32 <= n){
        __m256i blk = _mm256_loadu_si256((const __m256i*)(buf + i));
        __m256i cmp = _mm256_cmpeq_epi8(blk, va);
        unsigned mask = (unsigned)_mm256_movemask_epi8(cmp);
        while (mask){
            int bit = __builtin_ctz(mask);
            size_t off = i + (size_t)bit;
            if (off + la_n > n) { mask &= mask - 1; continue; }
            const unsigned char *ca = buf + off;
            /* verify full litA */
            size_t k = 1; for (; k < la_n; k++) if (ca[k] != la[k]) break;
            if (k == la_n){
                /* advance newline counter from scan_pos up to this hit (O(n) total) */
                const unsigned char *p = buf + scan_pos;
                while (p < ca){ const unsigned char *nl = memchr(p, '\n', (size_t)(ca - p)); if(!nl) break; cur_line++; p = nl + 1; }
                scan_pos = off;
                /* litB must appear in the remainder of THIS line */
                size_t rem = n - (off + la_n);
                const unsigned char *eol = (const unsigned char*)memchr((const char*)(ca + la_n), '\n', rem);
                const unsigned char *le = eol ? eol : buf + n;
                const unsigned char *baseB = ca + la_n;
                for (size_t j = 0; j + lb_n <= (size_t)(le - baseB); j++){
                    if (baseB[j] == lb[0]){
                        size_t m = 1; for (; m < lb_n; m++) if (baseB[j+m] != lb[m]) break;
                        if (m == lb_n){ if (cur_line != last_line){ last_line = cur_line; on_match(cur_line, ctx); } break; }
                    }
                }
            }
            mask &= mask - 1;
        }
        if (i > lim) break;
        i += 32;
    }
    /* Scalar tail: buffers < 32 bytes, or the final partial block. Mirrors the
     * AVX2 path so small inputs (single short line) still match. */
    for (; i + la_n <= n; i++){
        if (buf[i] == la[0]){
            size_t k = 1; for (; k < la_n; k++) if (buf[i+k] != la[k]) break;
            if (k == la_n){
                const unsigned char *ca = buf + i;
                const unsigned char *p = buf + scan_pos;
                while (p < ca){ const unsigned char *nl = memchr(p, '\n', (size_t)(ca - p)); if(!nl) break; cur_line++; p = nl + 1; }
                scan_pos = i;
                size_t rem = n - (i + la_n);
                const unsigned char *eol = (const unsigned char*)memchr((const char*)(ca + la_n), '\n', rem);
                const unsigned char *le = eol ? eol : buf + n;
                const unsigned char *baseB = ca + la_n;
                for (size_t j = 0; j + lb_n <= (size_t)(le - baseB); j++){
                    if (baseB[j] == lb[0]){
                        size_t m = 1; for (; m < lb_n; m++) if (baseB[j+m] != lb[m]) break;
                        if (m == lb_n){ if (cur_line != last_line){ last_line = cur_line; on_match(cur_line, ctx); } break; }
                    }
                }
            }
        }
    }
}

__attribute__((target("avx2,popcnt,bmi,bmi2")))
int wub_simd_has_window(const unsigned char *buf, size_t n,
                        const unsigned char *la, size_t la_n,
                        const unsigned char *lb, size_t lb_n,
                        size_t window){
    if (la_n==0 || lb_n==0 || n < la_n + lb_n) return 0;
    __m256i va = _mm256_set1_epi8((char)la[0]);
    size_t i=0;
    /* AVX2 scan over full 32-byte blocks. */
    while (i + 32 <= n){
        __m256i blk = _mm256_loadu_si256((const __m256i*)(buf+i));
        __m256i cmp = _mm256_cmpeq_epi8(blk, va);
        unsigned mask = (unsigned)_mm256_movemask_epi8(cmp);
        while (mask){
            int bit = __builtin_ctz(mask);
            const unsigned char *ca = buf + i + bit;
            size_t off=(size_t)(ca-buf);
            if (off+la_n<=n){
                size_t k=1; for(;k<la_n;k++) if(ca[k]!=la[k]) break;
                if (k==la_n){
                    /* litA confirmed; look for litB in [ca+la_n, ca+la_n+window) */
                    const unsigned char *base = ca + la_n;
                    size_t bw = (ca + la_n + window < buf + n) ? window
                                : (size_t)((buf+n) - base);
                    for (size_t j=0; j+lb_n<=bw; j++){
                        if (base[j]==lb[0]){
                            size_t m=1; for(;m<lb_n;m++) if(base[j+m]!=lb[m]) break;
                            if (m==lb_n) return 1;
                        }
                    }
                }
            }
            mask &= mask-1;
        }
        i += 32;
    }
    /* Scalar tail: buffers < 32 bytes, or the final partial block. Without
     * this, small inputs never enter the AVX2 loop and a valid LIT.*LIT match
     * (e.g. unit-test "a.*c" on "axxxc") would be wrongly rejected. */
    for (; i + la_n <= n; i++){
        if (buf[i] == la[0]){
            size_t k=1; for(;k<la_n;k++) if(buf[i+k]!=la[k]) break;
            if (k==la_n){
                const unsigned char *base = buf + i + la_n;
                size_t bw = (i + la_n + window < n) ? window : (n - (i + la_n));
                for (size_t j=0; j+lb_n<=bw; j++){
                    if (base[j]==lb[0]){
                        size_t m=1; for(;m<lb_n;m++) if(base[j+m]!=lb[m]) break;
                        if (m==lb_n) return 1;
                    }
                }
            }
        }
    }
    return 0;
}

/* ---- 5. single-pass multi-literal PRESENCE gate (the prefilter reject) ----
 * One AVX2 sweep over the whole buffer proves whether ANY of the required
 * literals is present. This collapses the gate's N serial full-buffer memmem
 * scans (one per literal in an OR-alternation like foo|bar) into a SINGLE
 * 28MB pass: O(bytes) at memory bandwidth instead of O(N x bytes). Sound: a
 * per-literal block overlap (advance by 32-maxlen+1) guarantees every length-L
 * substring is fully contained in some 32B load, so a non-match is definitive.
 * Returns 1 if any literal is present, 0 if none is (sound reject), -1 if the
 * literal set is unsupported (caller falls back to the exact per-literal path).
 * Case-sensitive only (ICASE handled by the caller's scalar fold path). ---- */
#define WUB_SIMD_MAXLIT 16
#define WUB_SIMD_MAXLEN 16
__attribute__((target("avx2,popcnt,bmi,bmi2")))
int wub_simd_any_literal_present(const unsigned char *buf, size_t n,
                                 const unsigned char *const *lits,
                                 const int *lens, int nlits, int maxlen){
    if (nlits<=0 || nlits>WUB_SIMD_MAXLIT) return -1;
    if (maxlen<=0 || maxlen>WUB_SIMD_MAXLEN) return -1;
    if ((int)n < maxlen) return 0;                 /* cannot fit -> absent */
    /* Scan in 128-byte blocks (4 x 32B AVX2 loads), advancing by 128-maxlen+1
     * so every length-L substring is fully contained in some 32B load (sound:
     * a non-match is definitive). This matches the ugrep/RE-flex 128-byte-block
     * stride used for nlcount and is ~4x fewer block iterations than the naive
     * 32B-step design, so an absent literal is proven in a single memory pass.
     * Consecutive blocks overlap by (128-step) bytes, keeping coverage
     * continuous; `covered` tracks the end of the continuously-scanned region. */
    size_t step = 128 - (size_t)maxlen + 1;        /* body per 128B group */
    size_t pos = 0;
    size_t covered = 0;
    while (pos + 128 <= n){
        const unsigned char *base = buf + pos;
        for (int q=0; q<4; q++){
            const unsigned char *p = base + (size_t)q*32;
            __m256i blk = _mm256_loadu_si256((const __m256i*)p);
            for (int k=0; k<nlits; k++){
                __m256i vf = _mm256_set1_epi8((char)lits[k][0]);
                unsigned m = (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(blk, vf));
                while (m){
                    int bit = __builtin_ctz(m);
                    size_t bpos = (size_t)(p - buf) + (size_t)bit;
                    if (bpos + (size_t)lens[k] <= n){
                        int ok = 1;
                        for (int t=1; t<lens[k]; t++)
                            if (buf[bpos+t] != lits[k][t]){ ok = 0; break; }
                        if (ok) return 1;           /* present -> gate passes */
                    }
                    m &= m - 1;
                }
            }
        }
        covered = pos + 128;
        pos += step;
    }
    /* scalar tail: the region [covered, n) not yet scanned for presence */
    for (size_t j=covered; j + (size_t)maxlen <= n; j++){
        for (int k=0; k<nlits; k++){
            if (buf[j] == lits[k][0]){
                int ok = 1;
                for (int t=1; t<lens[k]; t++) if (buf[j+t] != lits[k][t]){ ok = 0; break; }
                if (ok) return 1;
            }
        }
    }
    return 0;   /* soundly absent -> gate rejects, no match possible */
}

/* ---- 6. SIMD newline + NUL scan (RE/flex/ugrep technique, native C11) ----
 * One pass over the buffer counts '\n' and detects any NUL byte, processing
 * 128 bytes at a time (four 32-byte AVX2 loads). This is the ugrep/RE-flex
 * simd_nlcount approach: a single SIMD sweep replaces the scalar memchr walks
 * we used for the line-index and the binary-file check. Returns the newline
 * count; *has_nul is set if a '\0' was encountered. ---- */
__attribute__((target("avx2"), force_align_arg_pointer))
void wub_simd_line_nul_stats(const unsigned char *buf, size_t n,
                             size_t *nl_out, int *has_nul){
    const unsigned char *s = buf;
    const unsigned char *e = buf + n;
    size_t nl = 0;
    int nul = 0;
    /* align to 32 bytes for the vector hot loop */
    while (((uintptr_t)s & 0x1f) != 0 && s < e){
        unsigned char c = *s++;
        if (c == '\n') nl++;
        else if (c == '\0') nul = 1;
    }
    const __m256i vnl  = _mm256_set1_epi8('\n');
    const __m256i v00  = _mm256_setzero_si256();
    while (s + 128 <= e){
        __m256i a = _mm256_loadu_si256((const __m256i*)(s));
        __m256i b = _mm256_loadu_si256((const __m256i*)(s + 32));
        __m256i c = _mm256_loadu_si256((const __m256i*)(s + 64));
        __m256i d = _mm256_loadu_si256((const __m256i*)(s + 96));
        nl += __builtin_popcount((unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, vnl)))
            + __builtin_popcount((unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(b, vnl)))
            + __builtin_popcount((unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(c, vnl)))
            + __builtin_popcount((unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(d, vnl)));
        /* NUL detection: a byte equals 0 iff (v00 == vi) */
        if (!nul){
            if (_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, v00)) ||
                _mm256_movemask_epi8(_mm256_cmpeq_epi8(b, v00)) ||
                _mm256_movemask_epi8(_mm256_cmpeq_epi8(c, v00)) ||
                _mm256_movemask_epi8(_mm256_cmpeq_epi8(d, v00)))
                nul = 1;
        }
        s += 128;
    }
    while (s < e){
        unsigned char c = *s++;
        if (c == '\n') nl++;
        else if (c == '\0') nul = 1;
    }
    *nl_out = nl;
    *has_nul = nul;
}

/* ---- 7. COMBINED scan: newline + NUL + single-literal presence ----
 * The ugrep/RE-flex technique: do the line-count, binary-detection AND the
 * literal-prefilter presence test in ONE 128-byte-block AVX2 pass instead of
 * two or three serial full-buffer scans. Used for the common single-literal
 * pattern (e.g. `the`, or a reject like a custom literal) so the gate and the
 * line-index pre-pass cost the same as one memory read. `lit_present` is set
 * iff the `lit`/`litlen` needle occurs anywhere (sound: a 32B block fully
 * contains every length-L window thanks to the stride overlap). ---- */
__attribute__((target("avx2"), force_align_arg_pointer))
void wub_simd_line_nul_lit_stats(const unsigned char *buf, size_t n,
                                 const unsigned char *lit, int litlen,
                                 size_t *nl_out, int *has_nul, int *lit_present){
    const unsigned char *s = buf;
    const unsigned char *e = buf + n;
    size_t nl = 0;
    int nul = 0, litfound = 0;
    if (litlen <= 0 || (int)n < litlen) litfound = 0;
    while (((uintptr_t)s & 0x1f) != 0 && s < e){
        unsigned char c = *s++;
        if (c == '\n') nl++;
        else if (c == '\0') nul = 1;
    }
    const __m256i vnl = _mm256_set1_epi8('\n');
    const __m256i v00 = _mm256_setzero_si256();
    __m256i vlit = v00;
    if (litlen > 0) vlit = _mm256_set1_epi8((char)lit[0]);
    /* NON-overlapping 128B blocks for the nl/nul counts (overlapping strides
     * would count shared newlines multiple times). The literal scan uses each
     * block's cmpeq for candidates and verifies with a litlen-byte lookahead
     * that may extend up to litlen-1 bytes PAST the block (still < n). */
    size_t pos = 0, covered = 0;
    while (pos + 128 <= n){
        for (int q=0; q<4; q++){
            const unsigned char *p = buf + pos + (size_t)q*32;
            __m256i blk = _mm256_loadu_si256((const __m256i*)p);
            nl += __builtin_popcount((unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(blk, vnl)));
            if (!nul && _mm256_movemask_epi8(_mm256_cmpeq_epi8(blk, v00))) nul = 1;
            if (litlen > 0 && !litfound){
                unsigned m = (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(blk, vlit));
                while (m){
                    int bit = __builtin_ctz(m);
                    size_t bp = pos + (size_t)q*32 + (size_t)bit;
                    if (bp + (size_t)litlen <= n){
                        int ok = 1;
                        for (int t=1; t<litlen; t++) if (buf[bp+t] != lit[t]){ ok = 0; break; }
                        if (ok) litfound = 1;
                    }
                    m &= m - 1;
                }
            }
        }
        pos += 128;
    }
    covered = pos;
    while (buf + covered < e){
        unsigned char c = buf[covered++];
        if (c == '\n') nl++;
        else if (c == '\0') nul = 1;
    }
    /* scalar tail for the literal (the [covered,n) region not yet scanned) */
    if (litlen > 0 && !litfound){
        for (size_t j=covered; j + (size_t)litlen <= n; j++){
            if (buf[j] == lit[0]){
                int ok = 1;
                for (int t=1; t<litlen; t++) if (buf[j+t] != lit[t]){ ok = 0; break; }
                if (ok) { litfound = 1; break; }
            }
        }
    }
    *nl_out = nl;
    *has_nul = nul;
    *lit_present = litfound;
}
