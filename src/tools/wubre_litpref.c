/*
 * wubre_litpref.c - Literal-SET prefilter extraction for the WuBu regex engine.
 * ---------------------------------------------------------------------------
 * ripgrep's biggest speed win on patterns that have NO single literal (classes,
 * alternations, bounded repeats) is that it EXTRACTS literal runs from the
 * PATTERN and SIMD-scans the buffer for them BEFORE running the NFA/DFA:
 *     foo|bar   -> OR( foo, bar )          : if neither literal is present, the
 *                                            whole buffer cannot match.
 *     a{2,4}    -> requires "aa"           : a 2-4x 'a' run needs "aa" present.
 *     (ab)+     -> requires "ab"           : >=1 repetition needs "ab" present.
 *     a.*b      -> requires "a" AND "b"    : both literals must occur.
 *     [0-9]     -> no literal run          : gate is a no-op (class only).
 *
 * We parse the pattern into a SOUND OR-of-ANDs (DNF) of REQUIRED literal runs,
 * using AND-merge as we walk the sequence (the standard literal-extraction
 * technique). A match of an alternative needs every literal run in that
 * alternative present (AND), and any alternative matching is enough (OR).
 * Literals under an OPTIONAL quantifier (?, *, {0,}) are NOT required and are
 * dropped, so the gate is strictly permissive: it can never hide a real match.
 * The NFA/DFA always does the exact verification afterwards.
 *
 * Built ONCE at compile time, cached on WURegex (re->litpref); wubre_search_buf
 * calls wubre_litpref_gate() to skip the expensive NFA when no required literal
 * set is present anywhere in the buffer. Self-contained C11, opaque otherwise.
 * License: WaefreBeorn Umbrella License v3.0.
 * ---------------------------------------------------------------------------
 */
#include "wubre_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LP_MAXALT 32   /* max OR terms (alternatives) */
#define LP_MAXLIT 8    /* max AND terms (literal runs) per alternative */
#define LP_LMAX   31   /* max bytes per literal run */

typedef struct { unsigned char s[LP_LMAX]; int len; } LPLit;
typedef struct { LPLit lits[LP_MAXLIT]; int n; } LPAlt;
typedef struct { LPAlt alts[LP_MAXALT]; int n; } LitPref;

static void alt_add_lit(LPAlt *a, const unsigned char *s, int len){
    if (a->n >= LP_MAXLIT || len <= 0 || len > LP_LMAX) return;
    memcpy(a->lits[a->n].s, s, len);
    a->lits[a->n].len = len;
    a->n++;
}

/* AND a single literal onto EVERY alternative in cur (concatenate). */
static void dnf_and_lit(LitPref *cur, const unsigned char *s, int len){
    for (int i=0;i<cur->n;i++) alt_add_lit(&cur->alts[i], s, len);
}

/* AND-merge: cur = cur (x) sub  -- each cur-alt combined with each sub-alt. */
static void dnf_and(LitPref *cur, const LitPref *sub){
    if (sub->n == 0) return;            /* vacuous: identity */
    LitPref tmp; tmp.n = 0;
    for (int i=0;i<cur->n && tmp.n<LP_MAXALT;i++){
        for (int j=0;j<sub->n && tmp.n<LP_MAXALT;j++){
            LPAlt a; a.n = 0;
            for (int k=0;k<cur->alts[i].n && a.n<LP_MAXLIT;k++) a.lits[a.n++] = cur->alts[i].lits[k];
            for (int k=0;k<sub->alts[j].n && a.n<LP_MAXLIT;k++) a.lits[a.n++] = sub->alts[j].lits[k];
            tmp.alts[tmp.n++] = a;
        }
    }
    *cur = tmp;
}

static int lp_count(const char *p, const char *end){
    /* Returns the MINIMUM count (0 if the repeat is optional, e.g. {0,},
     * {0,1}, {0,3}). The prefilter gate must only require literals that are
     * GUARANTEED to appear; a min-0 repeat makes its literal optional. */
    if (p >= end || *p != '{') return 1;
    const char *q = p+1; int n = 0, seen = 0;
    while (q < end && *q >= '0' && *q <= '9'){ n = n*10 + (*q-'0'); q++; seen = 1; }
    if (!seen) return 1;
    if (q < end && *q == ','){  /* {n,m} or {n,} -- min is n */
        return n;                /* n may be 0 -> optional literal */
    }
    return n < 1 ? 1 : n;        /* {m}: min == max == m, require if m>=1 */
}

/* Parse a sequence of atoms from *pp..end (group nesting = depth). Returns the
 * DNF for the sequence. '|' splits into OR; ')' (depth>0) ends the sub-sequence
 * and returns; end of input returns the accumulated DNF. */
static LitPref parse_seq(const char **pp, const char *end, int depth){
    LitPref cur; cur.n = 0;
    LPAlt e; e.n = 0; cur.alts[cur.n++] = e;   /* start: one empty alternative */
    const char *p = *pp;
    unsigned char run[LP_LMAX]; int runn = 0;

    while (p < end){
        char c = *p;
        if (c == '\\' && p+1 < end){ if (runn<LP_LMAX) run[runn++]=(unsigned char)p[1]; p+=2; continue; }
        if (c == '|'){
            if (runn) dnf_and_lit(&cur, run, runn);   /* flush pending run */
            runn = 0;
            LitPref left = cur;
            const char *rest = p+1;
            LitPref right = parse_seq(&rest, end, depth);
            *pp = rest;
            for (int i=0;i<right.n && left.n<LP_MAXALT;i++) left.alts[left.n++] = right.alts[i];
            return left;
        }
        if (c == '('){
            if (runn) dnf_and_lit(&cur, run, runn);   /* flush pending run */
            runn = 0;
            int d = 1; const char *q = p+1;
            while (q < end && d){
                if (*q == '\\'){ q += 2; continue; }
                if (*q == '['){ while (q<end && *q!=']') q++; if (q<end) q++; continue; }
                if (*q == '(') d++;
                else if (*q == ')'){ d--; if (!d){ q++; break; } }
                if (q < end) q++;
            }
            const char *body = p+1;
            LitPref sub = parse_seq(&body, q, depth+1);
            dnf_and(&cur, &sub);
            p = q; runn = 0; continue;
        }
        if (c == ')'){ if (depth>0){ if (runn) dnf_and_lit(&cur, run, runn); *pp = p; return cur; } p++; continue; }
        if (c == '['){
            if (runn) dnf_and_lit(&cur, run, runn);
            runn = 0;
            while (p < end && *p != ']') p++;
            if (p < end) p++;
            continue;
        }
        if (c == '^' || c == '$' || c == '.'){
            if (runn) dnf_and_lit(&cur, run, runn);
            runn = 0; p++; continue;
        }
        if (c == '*' || c == '?'){
            /* optional: only the LAST char of the run is optional -> keep the
             * required prefix (all but the last char). */
            if (runn > 1) dnf_and_lit(&cur, run, runn-1);
            runn = 0; p++; continue;
        }
        if (c == '+'){
            if (runn) dnf_and_lit(&cur, run, runn);
            runn = 0; p++; continue;
        }
        if (c == '{'){
            int minc = lp_count(p, end);   /* min repeat: 0 => literal optional */
            if (minc >= 1 && runn){
                int m = minc;              /* require min copies of the run */
                if (m >= 2){
                    unsigned char mult[LP_LMAX]; int ml=0;
                    for (int k=0;k<m && ml<LP_LMAX;k++){ memcpy(mult+ml,run,runn); ml+=runn; }
                    dnf_and_lit(&cur, mult, ml);
                } else dnf_and_lit(&cur, run, runn);
            }
            /* minc == 0: literal is optional -> do NOT require it (sound gate). */
            runn = 0;
            while (p < end && *p != '}') p++;
            if (p < end) p++;
            continue;
        }
        if (runn < LP_LMAX) run[runn++] = (unsigned char)c;
        p++;
    }
    if (runn) dnf_and_lit(&cur, run, runn);
    *pp = p;
    return cur;
}

void wubre_litpref_build(WURegex *re, const char *pat, int flags){
    /* 'pat' is the ERE-translated pattern (for BRE, the caller passes the
     * translated form), so the literal extraction is sound for both flavors. */
    LitPref *lp = (LitPref*)calloc(1, sizeof(LitPref));
    const char *p = pat, *end = pat + strlen(pat);
    *lp = parse_seq(&p, end, 0);
    /* ICASE: fold every stored literal run to lowercase so the gate's scan is
     * case-insensitive (cfold('E')==cfold('e')); the gate folds haystack bytes
     * the same way. Without this, an uppercase literal like ERROR would be
     * rejected against a lowercase corpus (error) before the DFA runs. */
    if (flags & WUBRE_ICASE){
        for (int i=0;i<lp->n;i++)
            for (int j=0;j<lp->alts[i].n;j++){
                LPLit *L = &lp->alts[i].lits[j];
                for (int k=0;k<L->len;k++) L->s[k] = (unsigned char)cfold(L->s[k]);
            }
    }
    int any = 0;
    for (int i=0;i<lp->n;i++) if (lp->alts[i].n > 0){ any = 1; break; }
    if (!any){ free(lp); ((WURegex*)re)->litpref = NULL; return; }
    ((WURegex*)re)->litpref = lp;
}

int wubre_litpref_gate(const WURegex *re, const unsigned char *buf, size_t n){
    LitPref *lp = (LitPref*)re->litpref;
    if (!lp || lp->n == 0) return 1;       /* no gate -> always pass */
    int icase = (re->flags & WUBRE_ICASE) != 0;
    if (!icase){
        /* Single-pass SIMD presence check: gather every literal across all
         * alternatives into one array and prove, in ONE sweep, whether ANY of
         * them is present. If none is, the gate can soundly reject (no match
         * is possible) -- collapsing the previous N serial full-buffer memmem
         * scans into a single O(bytes) pass. For the common case (0 matches),
         * this is the whole cost of the gate. */
        int maxlen = 0, total = 0;
        for (int i=0;i<lp->n;i++) for (int j=0;j<lp->alts[i].n;j++){
            if (lp->alts[i].lits[j].len > maxlen) maxlen = lp->alts[i].lits[j].len;
            total++;
        }
        if (total > 0 && total <= 16 && maxlen <= 16){
            const unsigned char *lits[16]; int lens[16]; int idx=0;
            for (int i=0;i<lp->n;i++) for (int j=0;j<lp->alts[i].n;j++){
                lits[idx] = lp->alts[i].lits[j].s;
                lens[idx] = lp->alts[i].lits[j].len;
                idx++;
            }
            int r = wub_simd_any_literal_present(buf, n, lits, lens, total, maxlen);
            if (r == 1) return 1;            /* some literal present -> pass */
            if (r == 0) return 0;            /* soundly absent -> reject */
            /* r == -1 (unsupported) -> fall through to exact check */
        }
    }
    /* Exact (scalar or ICASE-folded) check: a required literal set is present
     * iff for SOME alternative all its literals are present (OR across alts,
     * AND within an alt). Never drops a real match. */
    for (int i=0;i<lp->n;i++){             /* OR across alternatives */
        LPAlt *a = &lp->alts[i];
        int all = 1;
        for (int j=0;j<a->n;j++){          /* AND within an alternative */
            const unsigned char *needle = a->lits[j].s;
            int nlen = a->lits[j].len;
            int found = 0;
            if (!icase){
                found = (wub_memmem(buf, n, needle, (size_t)nlen) != NULL);
            } else {
                if (nlen == 0) found = 1;
                else for (size_t off=0; off + (size_t)nlen <= n; off++){
                    int ok = 1;
                    for (int k=0;k<nlen;k++)
                        if (cfold(buf[off+k]) != needle[k]){ ok = 0; break; }
                    if (ok){ found = 1; break; }
                }
            }
            if (!found){ all = 0; break; }
        }
        if (all) return 1;
    }
    return 0;   /* no required literal set present -> no possible match */
}

void wubre_litpref_free(void *p){ if (p) free(p); }
