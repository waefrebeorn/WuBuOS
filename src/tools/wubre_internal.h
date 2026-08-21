/*
 * wubre_internal.h - Internal types shared by the WuBu regex engine modules.
 * ---------------------------------------------------------------------------
 * This is an INTERNAL header (not installed, not part of the public API).
 * The public API is wubre.h, which keeps WURegex fully OPAQUE. Modules that
 * need the concrete layout (compile, match, bre) include this.
 *
 * Module map (each file self-contained, C11-only, opaque-to-API):
 *   wubre_compile.c  - parser + NFA builder + prefilter extraction
 *   wubre_match.c    - Pike-VM (Thompson NFA) simulation + SIMD prefilter
 *   wubre_simd.c     - AVX2 literal search (our own, no vectorscan dep)
 *   wubre_bre.c      - BRE backreference backtracking engine
 *   wubre.c          - public API dispatch + wubre_free
 *
 * License: WaefreBeorn Umbrella License v3.0
 * ---------------------------------------------------------------------------
 */
#ifndef WUBRE_INTERNAL_H
#define WUBRE_INTERNAL_H

#include "wubre.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_STATES 8000

/* ---- NFA state machine ---- */
typedef enum { CHR, SPLIT, MATCH, ANCH_START, ANCH_END, ANCH_WBEG, ANCH_WEND,
               ANCH_WB, ANCH_NOTWB } Stype;

typedef struct State {
    Stype type;
    int   c;        /* CHR byte (case-folded if icase) */
    int   cl;       /* 0=char 1=dot 2=class */
    int   neg;      /* class negated */
    unsigned char *bits; /* 256-bit class membership (owned when cl==2) */
    int   out;      /* primary epsilon/consume target */
    int   out1;     /* SPLIT secondary target */
} State;

/* ---- backtracking (BRE backref) program ---- */
enum BT { BT_LIT, BT_DOT, BT_CLS, BT_BOL, BT_EOL, BT_BREF, BT_SEQ, BT_ALT,
          BT_STAR, BT_PLUS, BT_QUEST, BT_REP, BT_CAP };
typedef struct BTNode {
    enum BT op;
    int   c;            /* BT_LIT byte (folded if icase) */
    int   neg;          /* BT_CLS negated */
    unsigned char *bits;/* BT_CLS 256-bit membership */
    int   g;            /* BT_BREF group, BT_CAP group */
    int   min, max;     /* BT_REP {min,max} (max=-1 unbounded) */
    int   a, b;         /* child indices (BT_SEQ/BT_ALT/BT_STAR/.../BT_CAP) */
    int   next;         /* continuation node (what follows this node) */
} BTNode;

typedef struct {
    BTNode *nodes; int n, cap;
    int ngroups; int icase; int dotnl;
    const unsigned char *buf; size_t blen;
    int *group;         /* 2*ngroups: [start0,end0,...] */
    long steps;         /* remaining backtracking budget (DoS guard) */
} BTProg;

/* ---- concrete regex object (opaque in the public header) ---- */
struct WURegex {
    State st[MAX_STATES];
    int nst;
    int start;
    int flags;
    /* required-literal prefilter: skip the NFA unless the buffer contains it */
    const unsigned char *prefilter;
    int preflen;
    /* two-literal window prefilter for X.*Y patterns (Hyperscan "literal
     * acceleration"): if set, the buffer must contain pref2a then pref2b
     * within a bounded forward window; otherwise no match is possible and we
     * reject the line without running the NFA. */
    const unsigned char *pref2a, *pref2b;
    int pref2a_n, pref2b_n;
    /* BRE backtracking program (used when bt_n>0) */
    BTNode *bt;
    int bt_n;
    int bt_ngroups;
    int bt_root;
    /* 1 when the pattern is unanchored (no leading ^) -- the NFA may begin
     * matching at any buffer position; wubre_search_buf re-admits the start
     * state each step to implement "match anywhere" over a whole buffer. */
    int unanchored;
    /* 1 when the pattern is exactly  LIT .* LIT  (two literal runs separated
     * by an unanchored .* and nothing else). In that case a line matches iff
     * litA precedes litB within the same line, so the NFA is unnecessary and
     * wubre_search_buf uses the SIMD window scanner instead (Hyperscan-style
     * literal acceleration) -- turning a pathological O(n) ".*" scan into an
     * O(occurrences) SIMD pass. */
    int win_only;
     /* Pure-literal pattern (no metacharacters at all): the whole search key
      * is 'lit' of length 'lit_n'. wubre_search_buf may use the SIMD literal
      * scanner instead of the Pike VM. Set to 0 for case-insensitive use (the
      * fast path is case-sensitive only). */
     int lit_only;
     const unsigned char *lit;
     int lit_n;
     /* Literal-SET prefilter (whole-buffer gate). Extracted from the
      * PATTERN STRING at compile time (wubre_litpref.c). wubre_search_buf
      * scans for these before running the NFA/DFA. OR: any literal present
      * -> pass. AND: all present -> pass. Permissive: only REQUIRED,
      * non-optional literal runs are recorded, so a matching line always
      * contains them -- the gate can NEVER drop a real match. */
     char lit_buf[8][32];
     const unsigned char *lit_set[8];
     int lit_set_n[8];
     int nlit;
     int lit_or;
     /* Cached subset-construction DFA (wubre_dfa.c), built once per pattern and
      * reused across searches. dfa_cache is an opaque Dfa*; dfa_failed marks a
      * pattern the DFA can't represent (caller falls back to the Pike VM). */
     void *litpref;
     void *dfa_cache;
     int dfa_failed;
     };

/* ---- dangling-pointer patch list (NFA construction) ---- */
typedef struct Dangle { int s; int field; struct Dangle *next;
                        struct Dangle *regnext; } Dangle;

typedef struct { WURegex *re; Dangle *all; } Ctx;

typedef struct {
    int   start;
    Dangle *out;
    const char *src;     /* source span of this atom (for {n,m} expansion) */
    const char *src_end;
    int   empty;         /* 1 = zero-width match only (no content) */
} Frag;

#define FRAG_NULL ((Frag){.start=-1,.out=NULL,.src=NULL,.src_end=NULL})

/* ---- parser context ---- */
typedef struct {
    const char *p, *end;
    Ctx *cx;
    char *err; size_t errsz;
} P;

/* ---- case fold ---- */
static inline int cfold(int c){ return (c>='A'&&c<='Z') ? c-'A'+'a' : c; }

/* ---- shared helpers (defined in wubre_compile.c) ---- */

/* ---- BRE backtracking engine (wubre_bre.c) ---- */
WURegex *wubre_compile_bre(const char *pat, int flags, char *err, size_t errsz);
bool     wubre_search_bre(const WURegex *re, const unsigned char *buf, size_t n);
bool wubre_search(const WURegex *re, const unsigned char *buf, size_t n);
/* whole-buffer unanchored match (one NFA pass over the whole file). Calls
 * on_match(line_index, ctx) for each distinct matching line. Returns true if any
 * line matched. */
bool wubre_search_buf(const WURegex *re, const unsigned char *buf, size_t n,
                      void (*on_match)(long line, void *ctx), void *ctx);

/* ---- literal-set prefilter extraction (wubre_litpref.c) ---- */
void wubre_litpref_build(WURegex *re, const char *pat, int flags);
int  wubre_litpref_gate(const WURegex *re, const unsigned char *buf, size_t n);
void wubre_litpref_free(void *p);

/* ---- DFA (subset construction, wubre_dfa.c) ---- */
int wubre_search_buf_dfa(const WURegex *re, const unsigned char *buf, size_t n,
                         void (*on_match)(long line, void *ctx), void *ctx);
void wubre_dfa_free(void *d);   /* frees a cached Dfa* (opaque) */
int  wubre_dfa_nstates(const WURegex *re);

/* ---- literal-set prefilter extraction (wubre_litpref.c) ----
void wubre_extract_literals(WURegex *re, const char *pat, int flags);

/* ---- shared class helpers (defined in wubre_compile.c, used by BRE too) ---- */
int  is_known_posix_class(const char *name);
void set_class_posix(unsigned char *bits, const char *name);

/* ---- SIMD literal/window search (wubre_simd.c) ---- */
const unsigned char *wub_memmem(const unsigned char *hay, size_t hn,
                                const unsigned char *needle, size_t nn);
/* Single-pass literal matcher (AVX2): one sweep finds every needle occurrence
 * and reports the 0-based line index of each distinct matching line via
 * on_match (grep -n semantics). O(matches+lines), not O(hits x n). */
void wub_simd_scan_literal(const unsigned char *hay, size_t hn,
                           const unsigned char *needle, size_t nn,
                           void (*on_match)(long line, void *ctx), void *ctx);
int wub_simd_has_window(const unsigned char *buf, size_t n,
                        const unsigned char *la, size_t la_n,
                        const unsigned char *lb, size_t lb_n,
                        size_t window);
/* Exact LIT.*LIT matcher: SIMD-scans for every litA, verifies litB in the same
 * line, and reports each matching line index via on_match. NFA-free. */
void wub_simd_scan_windows(const unsigned char *buf, size_t n,
                           const unsigned char *la, size_t la_n,
                           const unsigned char *lb, size_t lb_n,
                           void (*on_match)(long line, void *ctx), void *ctx);

#endif /* WUBRE_INTERNAL_H */
