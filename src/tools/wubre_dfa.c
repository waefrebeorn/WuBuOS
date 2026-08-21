/*
 * wubre_dfa.c - Subset-construction DFA for the Thompson NFA (wubre_compile).
 * ---------------------------------------------------------------------------
 * The Pike VM in wubre_match.c steps the NFA one state per byte - O(n * states)
 * with per-byte array/closure overhead, which is ~100-600x slower than ripgrep
 * on patterns like  a+  or  [a-z]+ .  A DFA collapses each NFA-state SET into a
 * single dense state: every input byte is ONE table lookup (trans[state][byte]),
 * so a 41 MB buffer scans in ~memory-bandwidth time. The transition table is a
 * FLAT array (one load per byte).
 *
 * The DFA is built ONCE at compile time (wubre_compile) and cached on the
 * WURegex object (re->dfa_cache), so it is shared safely across the parallel
 * file-scan worker threads (read-only at search time). Outside scope (anchors
 * / >60 NFA states / DFA blow-up past DFA_CAP_MAX) we return NULL and
 * wubre_search_buf falls back to the Pike VM. Self-contained C11.
 * License: WaefreBeorn Umbrella License v3.0.
 * ---------------------------------------------------------------------------
 */
#include "wubre_internal.h"
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>

#define DFA_CAP_INIT 256
#define DFA_CAP_MAX  8192

typedef struct {
    int  *trans;      /* flat [cap*256]: trans[state*256 + byte] -> next state, or -1 */
    int  *match;      /* [cap]: 1 if MATCH state is in this DFA state */
    int   n;
    int   cap;
    int   start;
    int   unanchored;
    uint64_t start_set;
    uint64_t *keys;   /* set -> id map (open addressing) */
    int      *vals;
    int       mcap;
    /* SIMD class-skip tables: for a non-matching self-loop state s, the bytes
     * whose transition differs from s form a few small ranges (skip_lo/hi).
     * The walker SIMD-skips runs of self-loop bytes (never skipping a match). */
    int  *skip_self;        /* [cap] self-loop target s if skippable, else -1 */
    int  *skip_nr;          /* [cap] number of boundary ranges */
    unsigned char *skip_lo; /* [cap*8] boundary range lo bytes (state s -> +s*8) */
    unsigned char *skip_hi; /* [cap*8] boundary range hi bytes */
} Dfa;

static uint64_t dfa_closure(const WURegex *re, uint64_t s){
    uint64_t out = s, prev;
    do {
        prev = out;
        uint64_t m = out;
        while (m){
            int b = __builtin_ctzll(m);
            m &= m - 1;
            const State *st = &re->st[b];
            if (st->type == SPLIT){
                if (st->out  >= 0) out |= (1ULL << st->out);
                if (st->out1 >= 0) out |= (1ULL << st->out1);
            }
        }
    } while (out != prev);
    return out;
}

static int dfa_has_match(const WURegex *re, uint64_t s){
    uint64_t m = s;
    while (m){ int b = __builtin_ctzll(m); m &= m-1; if (re->st[b].type == MATCH) return 1; }
    return 0;
}

static int state_matches(const WURegex *re, int s, unsigned char c){
    const State *st = &re->st[s];
    if (st->type != CHR) return 0;
    if (re->flags & WUBRE_ICASE) c = (unsigned char)cfold(c);
    if (st->cl == 0) return (int)c == st->c;
    if (st->cl == 1) return c != '\n';
    int in = (st->bits[c>>3] >> (c&7)) & 1;
    return st->neg ? !in : in;
}

static uint64_t dfa_move(const WURegex *re, uint64_t set, unsigned char c){
    uint64_t newset = 0;
    uint64_t m = set;
    while (m){
        int b = __builtin_ctzll(m);
        m &= m - 1;
        if (state_matches(re, b, c))
            newset |= (1ULL << re->st[b].out);
    }
    return dfa_closure(re, newset);
}

static int dfa_intern(Dfa *d, uint64_t set, int *added){
    uint64_t h = set * 0x9E3779B97F4A7C15ULL;
    h ^= h >> 29;
    int mask = d->mcap - 1;
    for (;;){
        int i = (int)(h & mask);
        if (d->vals[i] == -1){ d->keys[i] = set; int id = d->n++; d->vals[i] = id; *added = 1; return id; }
        if (d->keys[i] == set){ *added = 0; return d->vals[i]; }
        h = (h << 1) | (h >> 63);
    }
}

static void dfa_ensure(Dfa *d, int id){
    if (id < d->cap) return;
    int nc = d->cap * 2;
    if (nc > DFA_CAP_MAX) nc = DFA_CAP_MAX;
    d->trans = (int*)realloc(d->trans, (size_t)nc * 256 * sizeof(int));
    d->match = (int*)realloc(d->match, (size_t)nc * sizeof(int));
    for (int i=d->cap;i<nc;i++){
        d->match[i] = 0;
        for (int c=0;c<256;c++) d->trans[i*256 + c] = -1;
    }
    d->cap = nc;
}

/* Build the DFA (cached on re->dfa_cache). Returns Dfa* or NULL if out of scope
 * or the DFA would exceed DFA_CAP_MAX states (caller falls back to Pike VM). */
void *wubre_dfa_compile(const WURegex *re){
    if (re->nst > 60) return NULL;
    for (int i=0;i<re->nst;i++){
        Stype t = re->st[i].type;
        if (t==ANCH_START||t==ANCH_END||t==ANCH_WBEG||t==ANCH_WEND||t==ANCH_WB||t==ANCH_NOTWB)
            return NULL;
    }
    Dfa *d = (Dfa*)calloc(1, sizeof(Dfa));
    d->cap = DFA_CAP_INIT;
    d->trans = (int*)malloc((size_t)d->cap * 256 * sizeof(int));
    d->match = (int*)malloc((size_t)d->cap * sizeof(int));
    for (int i=0;i<d->cap;i++){ d->match[i]=0; for (int c=0;c<256;c++) d->trans[i*256+c] = -1; }
    d->mcap = 1 << 14;
    d->keys = (uint64_t*)calloc(d->mcap, sizeof(uint64_t));
    d->vals = (int*)malloc(d->mcap * sizeof(int));
    for (int i=0;i<d->mcap;i++) d->vals[i] = -1;
    d->skip_self = (int*)malloc((size_t)DFA_CAP_MAX * sizeof(int));
    d->skip_nr   = (int*)malloc((size_t)DFA_CAP_MAX * sizeof(int));
    d->skip_lo   = (unsigned char*)malloc((size_t)DFA_CAP_MAX * 8);
    d->skip_hi   = (unsigned char*)malloc((size_t)DFA_CAP_MAX * 8);
    for (int i=0;i<DFA_CAP_MAX;i++){ d->skip_self[i] = -1; d->skip_nr[i] = 0; }
    d->n = 0;
    d->unanchored = re->unanchored;
    d->start_set = dfa_closure(re, (1ULL << re->start));
    /* Empty (zero-width) matches need special output handling; let the Pike VM
     * own them so we never regress grep's zero-width line reporting. */
    if (dfa_has_match(re, d->start_set)) { wubre_dfa_free(d); return NULL; }

    int added = 0;
    d->start = dfa_intern(d, d->start_set, &added);
    dfa_ensure(d, d->start);
    d->match[d->start] = dfa_has_match(re, d->start_set);

    for (int i=0; i<d->n; i++){
        /* recover the NFA-set for DFA state i from the set->id map */
        int slot = -1;
        for (int k=0;k<d->mcap;k++) if (d->vals[k]==i){ slot=k; break; }
        uint64_t set = d->keys[slot];
        for (int c=0;c<256;c++){
            uint64_t mv = dfa_move(re, set, (unsigned char)c);
            if (d->unanchored){ mv |= d->start_set; mv = dfa_closure(re, mv); }
            if (mv == 0){ d->trans[i*256 + c] = -1; continue; }
            int a = 0;
            int id = dfa_intern(d, mv, &a);
            if (id >= DFA_CAP_MAX){ wubre_dfa_free(d); return NULL; }
            dfa_ensure(d, id);
            d->trans[i*256 + c] = id;
            if (a) d->match[id] = dfa_has_match(re, mv);
        }
    }

    /* Precompute SIMD class-skip ranges. For a non-matching self-loop state s,
     * the bytes whose transition differs from s form a few small ranges; the
     * walk SIMD-skips runs of self-loop bytes (which can never match). */
    for (int s=0; s<d->n; s++){
        if (d->match[s]) continue;               /* never skip a match-state */
        int nr = 0, lo = -1, prev = -1;
        unsigned char *loP = d->skip_lo + (size_t)s*8;
        unsigned char *hiP = d->skip_hi + (size_t)s*8;
        for (int b=0; b<256; b++){
            int raw = d->trans[s*256 + b];
            int e = raw >= 0 ? raw : (d->unanchored ? d->start : s);
            int boundary = (e != s);
            if (boundary){
                if (lo < 0) lo = b;
                else if (b != prev + 1){ if (nr < 8){ loP[nr] = (unsigned char)lo; hiP[nr] = (unsigned char)prev; nr++; } lo = b; }
                prev = b;
            }
        }
        if (lo >= 0 && nr < 8){ loP[nr] = (unsigned char)lo; hiP[nr] = (unsigned char)prev; nr++; }
        d->skip_nr[s] = nr;
        if (nr >= 1 && nr <= 8) d->skip_self[s] = s;   /* skippable non-match self-loop */
    }

    return d;
}

void wubre_dfa_free(void *pd){
    Dfa *d = (Dfa*)pd;
    if (!d) return;
    if (d->trans) free(d->trans);
    if (d->match) free(d->match);
    if (d->keys) free(d->keys);
    if (d->vals) free(d->vals);
    if (d->skip_self) free(d->skip_self);
    if (d->skip_nr) free(d->skip_nr);
    if (d->skip_lo) free(d->skip_lo);
    if (d->skip_hi) free(d->skip_hi);
    free(d);
}

int wubre_dfa_nstates(const WURegex *re){ Dfa *d=(Dfa*)re->dfa_cache; return d ? d->n : -1; }

/* AVX2 block scanner: returns a 32-bit mask where bit k is set iff block byte k
 * lies inside any of the nr boundary ranges [lo,hi]. Unsigned-correct via the
 * 0x80 re-centering trick (so byte values >127 compare correctly). */
__attribute__((target("avx2")))
static unsigned dfa_block_boundary(const __m256i blk, const unsigned char *lo,
                                   const unsigned char *hi, int nr){
    const __m256i x80 = _mm256_set1_epi8((char)0x80);
    __m256i blk2 = _mm256_xor_si256(blk, x80);
    __m256i acc = _mm256_setzero_si256();
    for (int k=0;k<nr;k++){
        __m256i vlo = _mm256_xor_si256(_mm256_set1_epi8((char)lo[k]), x80);
        __m256i vhi = _mm256_xor_si256(_mm256_set1_epi8((char)hi[k]), x80);
        /* inclusive unsigned range test: lo <= blk <= hi.
         * below = (lo > blk);  above = (blk > hi);  then NOT each. */
        __m256i below = _mm256_cmpgt_epi8(vlo, blk2);   /* lo > blk        */
        __m256i above = _mm256_cmpgt_epi8(blk2, vhi);   /* blk > hi        */
        __m256i ge = _mm256_xor_si256(below, _mm256_set1_epi8((char)0xFF)); /* blk>=lo */
        __m256i le = _mm256_xor_si256(above, _mm256_set1_epi8((char)0xFF)); /* blk<=hi */
        acc = _mm256_or_si256(acc, _mm256_and_si256(ge, le));
    }
    return (unsigned)_mm256_movemask_epi8(acc);
}

/* Whole-buffer DFA walk. Reports the 0-based line index of each matching line.
 * Uses the cached DFA on re (built at compile time). Returns 1 if run, 0 if the
 * pattern is out of DFA scope (re->dfa_cache == NULL; caller falls back to the
 * Pike VM).
 *
 * SIMD class-skip: when the current state is a non-matching self-loop, runs of
 * self-loop bytes (which can never match or leave the state) are skipped 32
 * bytes at a time with a SIMD boundary check + SIMD newline count. Only the
 * genuine boundary bytes are processed scalar. Sound: a skipped byte stays in
 * a non-matching state, so no match is ever dropped. */
__attribute__((target("avx2")))
int wubre_search_buf_dfa(const WURegex *re, const unsigned char *buf, size_t n,
                         void (*on_match)(long line, void *ctx), void *ctx){
    /* The DFA is built once at compile time (wubre_compile -> wubre_dfa_compile)
     * and cached on re->dfa_cache, shared read-only across the parallel scan.
     * If NULL, the pattern is out of DFA scope -> caller falls back to Pike VM. */
    Dfa *d = (Dfa*)re->dfa_cache;
    if (!d) return 0;
    int cur = d->start;
    long line = 0, last = -1;
    const int *trans = d->trans;
    const int *match = d->match;

#if defined(__x86_64__)
    if (d->skip_self){
        const __m256i vnl = _mm256_set1_epi8('\n');
        size_t i = 0;
        while (i < n){
            if (d->skip_self[cur] == cur){
                size_t j = i;
                while (j + 32 <= n){
                    __m256i blk = _mm256_loadu_si256((const __m256i*)(buf + j));
                    unsigned m = dfa_block_boundary(blk, d->skip_lo + (size_t)cur*8,
                                                    d->skip_hi + (size_t)cur*8, d->skip_nr[cur]);
                    if (m == 0){
                        __m256i nl = _mm256_cmpeq_epi8(blk, vnl);
                        line += __builtin_popcount((unsigned)_mm256_movemask_epi8(nl));
                        j += 32;
                    } else {
                        int first = __builtin_ctz(m);
                        for (int k=0;k<first;k++) if (buf[j+k]=='\n') line++;
                        j += first;
                        break;
                    }
                }
                i = j;
                if (i >= n) break;
            }
            unsigned char c = buf[i];
            if (c == '\n') line++;
            int nx = trans[cur*256 + c];
            if (nx < 0) nx = d->unanchored ? d->start : cur;
            cur = nx;
            if (match[cur] && line != last){ last = line; if (on_match) on_match(line, ctx); }
            i++;
        }
        return 1;
    }
#endif

    for (size_t i=0;i<n;i++){
        unsigned char c = buf[i];
        if (c == '\n') line++;
        int nx = trans[cur*256 + c];
        if (nx < 0) nx = d->unanchored ? d->start : cur;
        cur = nx;
        if (match[cur] && line != last){ last = line; if (on_match) on_match(line, ctx); }
    }
    return 1;
}
