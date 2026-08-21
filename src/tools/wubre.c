/*
 * wubre.c - WuBu regex engine. "occupational supremacy of C11 code".
 * ---------------------------------------------------------------------------
 * Public API implementation: wubre_free. The compiler (wubre_compile), the
 * Thompson NFA simulator (wubre_search), the BRE backtracking engine, and the
 * SIMD literal accelerators live in their own self-contained modules (see
 * wubre_internal.h for the module map). This file owns object lifetime/teardown.
 *
 * License: WaefreBeorn Umbrella License v3.0
 * ---------------------------------------------------------------------------
 */
#include "wubre_internal.h"

void wubre_free(WURegex *re){
    if (!re) return;
    for (int i=0;i<re->nst;i++) if (re->st[i].cl==2) free(re->st[i].bits);
    free((void*)re->prefilter);   /* owned copy, may be NULL */
    free((void*)re->pref2a);
    free((void*)re->pref2b);
    if (re->bt) free(re->bt);
    if (re->litpref) wubre_litpref_free(re->litpref);
    if (re->dfa_cache) wubre_dfa_free(re->dfa_cache);
    free(re);
}
