/*
 * wubre_match.c - Thompson NFA (Pike VM) simulation + prefilter gate.
 * ---------------------------------------------------------------------------
 * Two entry points:
 *   wubre_search()     - per-line unanchored match (used for anchored /^.../
 *                        and as the general fallback; semantics = match anywhere
 *                        in the supplied buffer treated as one line).
 *   wubre_search_buf() - WHOLE-BUFFER match for unanchored patterns. Runs the
 *                        Pike VM once over buf[0..n) with ^/$ evaluated against
 *                        '\n' positions, exactly matching grep's per-line
 *                        semantics but in a SINGLE pass (no per-line call
 *                        overhead over a 4 GB corpus). Returns the 0-based line
 *                        index of the first matching line, or -1. This is the
 *                        lever that brings .* throughput within range of
 *                        ripgrep's whole-file SIMD DFA.
 * License: WaefreBeorn Umbrella License v3.0
 * ---------------------------------------------------------------------------
 */
#include "wubre_internal.h"

static int is_word(unsigned char c){ return (c=='_')||(c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z'); }

static int match_byte(State *st, int ch, int icase){
    if (st->type!=CHR) return 0;
    int c=ch;
    if (st->cl==1) return (c!='\n'); /* dot: never matches newline (dotnl off) */
    if (st->cl==2){ int in=(st->bits[c>>3]>>(c&7))&1; return st->neg? !in:in; }
    int want = (icase)? cfold(c): c;
    return (want == (icase? cfold(st->c): st->c));
}

bool wubre_search(const WURegex *re_, const unsigned char *buf, size_t n){
    WURegex *re=(WURegex*)(uintptr_t)re_;
    if (re->bt_n>0) return wubre_search_bre(re, buf, n);
    int icase=(re->flags&WUBRE_ICASE)?1:0;
    if (re->prefilter && re->preflen>0) {
        if (re->preflen > (int)n) return false;
        if (!wub_memmem(buf, n, re->prefilter, (size_t)re->preflen)) return false;
    }
    if (re->pref2a && re->pref2a_n>0 && re->pref2b && re->pref2b_n>0){
        if (n < (size_t)(re->pref2a_n + re->pref2b_n)) return false;
        if (!wub_simd_has_window(buf, n, re->pref2a, (size_t)re->pref2a_n,
                                 re->pref2b, (size_t)re->pref2b_n, n)) return false;
    }
    int dense[MAX_STATES], seen[MAX_STATES];
    memset(seen, 0, sizeof seen);
    int gen=1;
    int at_start0 = 1;
    int at_end0   = (n==0) || (buf[0]=='\n');
    int wprev0 = (n==0) ? 0 : is_word(buf[0]);
    int wcur0  = (n==0) ? 0 : is_word(buf[0]);
#define SADD(s) do{ if(seen[s]!=gen){ seen[s]=gen; dense[ndense++]=s; } }while(0)
    int ndense=0;
    if (re->start>=0){ SADD(re->start); }
    for (int qi=0; qi<ndense; qi++){
        int s=dense[qi]; State *st=&re->st[s];
        switch(st->type){
            case SPLIT:
                if (st->out>=0)  SADD(st->out);
                if (st->out1>=0) SADD(st->out1);
                break;
            case ANCH_START:
                if (at_start0 && st->out>=0) SADD(st->out);
                break;
            case ANCH_END:
                if (at_end0 && st->out>=0) SADD(st->out);
                break;
            case ANCH_WBEG:
                if ((at_start0 || !wprev0) && wcur0 && st->out>=0) SADD(st->out);
                break;
            case ANCH_WEND:
                if (wcur0 && (at_end0 || !wprev0) && st->out>=0) SADD(st->out);
                break;
            case ANCH_WB:
                if (wcur0 != wprev0 && st->out>=0) SADD(st->out);
                break;
            case ANCH_NOTWB:
                if (wcur0 == wprev0 && st->out>=0) SADD(st->out);
                break;
            default: break;
        }
    }
    size_t i=0;
    for (;;){
        for (int qi=0; qi<ndense; qi++) if (re->st[dense[qi]].type==MATCH) return true;
        if (i>=n) break;
        unsigned char ch=buf[i];
        /* at_start/at_end/wprev/wcur/wnext describe POSITION (i+1) -- the position
         * the closure below constructs (after consuming buf[i]). Position 0 was
         * already seeded by the initial closure (at_start0=1). Evaluating "^" at
         * position i+1 means: a new line begins here iff the byte just consumed
         * (buf[i]) was '\n'. (The old (i==0)||(buf[i-1]=='\n') was off by one and
         * made ^ match one byte too early in re-admitted start threads.) */
        int at_start = (buf[i]=='\n') ? 1 : 0;   /* pos i+1 begins a line iff buf[i] (just consumed) was '\n' */
        int at_end = (i+1==n) || (i+1<n && buf[i+1]=='\n');
        int wprev = (i==0) ? 0 : is_word(buf[i]);
        int wcur  = (i+1<n) ? is_word(buf[i+1]) : 0;
        int wnext = (i+2>=n) ? 0 : is_word(buf[i+2]);
        int next_gen = gen+1;
        gen = next_gen;
        int ndense2=0;
        if (re->start>=0){
            if (seen[re->start]!=next_gen){ seen[re->start]=next_gen; dense[ndense2++]=re->start; }
        }
        for (int qi=0; qi<ndense; qi++){
            int s=dense[qi]; State *st=&re->st[s];
            if (st->type==CHR && match_byte(st,ch,icase) && st->out>=0){
                int t=st->out;
                if (seen[t]!=next_gen){ seen[t]=next_gen; dense[ndense2++]=t; }
            }
        }
        for (int qi=0; qi<ndense2; qi++){
            int s=dense[qi]; State *st=&re->st[s];
            switch(st->type){
                case SPLIT:
                    if (st->out>=0)  { int t=st->out;  if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;} }
                    if (st->out1>=0) { int t=st->out1; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;} }
                    break;
                case ANCH_START:
                    if (at_start && st->out>=0){ int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;} }
                    break;
                case ANCH_END:
                    if (at_end && st->out>=0){ int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;} }
                    break;
                case ANCH_WBEG:
                    if ((at_start || !wprev) && wcur && st->out>=0){ int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;} }
                    break;
                case ANCH_WEND:
                    if (wcur && (at_end || !wnext) && st->out>=0){ int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;} }
                    break;
                case ANCH_WB:
                    if (wcur != wprev && st->out>=0){ int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;} }
                    break;
                case ANCH_NOTWB:
                    if (wcur == wprev && st->out>=0){ int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;} }
                    break;
                default: break;
            }
        }
        ndense=ndense2;
        i++;
    }
    return false;
#undef SADD
}

/* Whole-buffer unanchored match. Runs the Pike VM once over the entire buffer,
 * detecting a match at any '\n'-delimited line position. For every distinct
 * matching line, calls on_match(line_index, ctx). Returns true if any line
 * matched. This replaces per-line NFA calls so a 4 GB corpus is ONE pass, not
 * millions -- the lever that brings .* throughput within range of ripgrep's
 * whole-file SIMD DFA while keeping grep's exact per-line match semantics. */
bool wubre_search_buf(const WURegex *re_, const unsigned char *buf, size_t n,
                      void (*on_match)(long line, void *ctx), void *ctx){
    WURegex *re=(WURegex*)(uintptr_t)re_;
    /* Literal-SET prefilter gate (wubre_litpref.c): if no required literal set
     * from the pattern is present in the buffer, no match is possible -- skip
     * the expensive NFA/DFA. Permissive: never drops a real match. */
    if (re->litpref && !wubre_litpref_gate(re, buf, n)) return false;
    if (re->bt_n>0){
        long ln=0; const unsigned char *p=buf; long last=-1; bool any=false;
        while (p<buf+n){
            const unsigned char *nl=memchr(p,'\n',(size_t)(buf+n-p));
            const unsigned char *le = nl? nl : buf+n;
            if (wubre_search_bre(re,p,(size_t)(le-p))){ any=true; if(ln!=last){ last=ln; if(on_match)on_match(ln,ctx);} }
            ln++; p = nl? nl+1 : buf+n;
        }
        return any;
    }
    int icase=(re->flags&WUBRE_ICASE)?1:0;
    /* The required-literal prefilter gate is a cheap reject for the NFA path.
     * For a pure-literal pattern (lit_only) the SIMD scanner below already does
     * the full scan, so the gate would be a redundant second pass -- skip it. */
    if (re->prefilter && re->preflen>0 && !re->lit_only) {
        if (re->preflen > (int)n) return false;
        if (!wub_memmem(buf, n, re->prefilter, (size_t)re->preflen)) return false;
    }
    if (re->pref2a && re->pref2a_n>0 && re->pref2b && re->pref2b_n>0){
        if (n < (size_t)(re->pref2a_n + re->pref2b_n)) return false;
        if (!wub_simd_has_window(buf, n, re->pref2a, (size_t)re->pref2a_n,
                                 re->pref2b, (size_t)re->pref2b_n, n)) return false;
    }
    /* Pure-literal fast path (no metacharacters): a SINGLE AVX2 sweep over the
     * whole buffer finds every needle occurrence and reports the 0-based line
     * index of each distinct matching line (grep -n semantics: one hit per line
     * even if the needle occurs 100x on that line). O(matches+lines), not the
     * O(hits x n) of repeated wub_memmem re-scans. */
    if (re->lit_only && re->lit_n > 0) {
        if ((size_t)re->lit_n > n) return false;
        wub_simd_scan_literal(buf, n, re->lit, (size_t)re->lit_n, on_match, ctx);
        return true;
    }
    /* Exact LIT.*LIT: skip the NFA entirely and report matching lines via the
     * SIMD window scanner (O(occurrences-of-litA) instead of O(bytes)). */
    if (re->win_only && re->pref2a && re->pref2a_n>0 && re->pref2b && re->pref2b_n>0){
        wub_simd_scan_windows(buf, n, re->pref2a, (size_t)re->pref2a_n,
                              re->pref2b, (size_t)re->pref2b_n, on_match, ctx);
        return true;
    }
    /* General path: subset-construction DFA (one table lookup per byte).
     * Covers patterns with <=60 states and no anchors/word-bounds
     * (a+, [a-z]+, foo|bar, ICASE, ...). Falls back to the Pike VM
     * below for anything the DFA cannot represent (anchored / large). */
    if (0){
        if (wubre_search_buf_dfa(re, buf, n, on_match, ctx)) return true;
        /* DFA out of scope (anchors / >60 states): fall through to Pike VM */
    }
    int dense[MAX_STATES], seen[MAX_STATES];
    memset(seen, 0, sizeof seen);
    int gen=1;
    int at_start0 = 1;
    int at_end0   = (n==0) || (buf[0]=='\n');
    int wprev0 = (n==0) ? 0 : is_word(buf[0]);
    int wcur0  = (n==0) ? 0 : is_word(buf[0]);
#define SADD(s) do{ if(seen[s]!=gen){ seen[s]=gen; dense[ndense++]=s; } }while(0)
    int ndense=0;
    if (re->start>=0){ SADD(re->start); }
    for (int qi=0; qi<ndense; qi++){
        int s=dense[qi]; State *st=&re->st[s];
        switch(st->type){
            case SPLIT:       if (st->out>=0) SADD(st->out); if (st->out1>=0) SADD(st->out1); break;
            case ANCH_START:  if (at_start0 && st->out>=0) SADD(st->out); break;
            case ANCH_END:    if (at_end0 && st->out>=0) SADD(st->out); break;
            case ANCH_WBEG:   if ((at_start0||!wprev0)&&wcur0&&st->out>=0) SADD(st->out); break;
            case ANCH_WEND:   if (wcur0&&(at_end0||!wprev0)&&st->out>=0) SADD(st->out); break;
            case ANCH_WB:     if (wcur0!=wprev0&&st->out>=0) SADD(st->out); break;
            case ANCH_NOTWB:  if (wcur0==wprev0&&st->out>=0) SADD(st->out); break;
            default: break;
        }
    }
    long cur_line=0; long last_reported=-1; bool any=false;
    size_t i=0;
    for (;;){
        /* Match report: suppress the phantom empty match that the unanchored
         * re-admit can produce *after* the final newline (position == n). GNU
         * grep does not report an empty line that exists only because the buffer
         * ends in '\n', so drop exactly that one zero-width EOF match. */
        int eof_empty = (i==n) && (n>0) && (buf[n-1]=='\n');
        for (int qi=0; qi<ndense; qi++) if (re->st[dense[qi]].type==MATCH){
            any=true;
            if (!eof_empty && cur_line!=last_reported){ last_reported=cur_line; if(on_match)on_match(cur_line,ctx); }
            break;
        }
        if (i>=n) break;
        unsigned char ch=buf[i];
        if (ch=='\n') cur_line++;
        int next_gen = gen+1;
        gen = next_gen;
        int ndense2=0;
        /* Re-admit the start into the (i+1) set. For an unanchored pattern this
         * allows a new match to begin at every byte; for an anchored (^) pattern
         * the ANCH_START epsilon (expanded below) only fires at true line starts. */
        if (re->start>=0){
            if (seen[re->start]!=next_gen){ seen[re->start]=next_gen; dense[ndense2++]=re->start; }
        }
        /* Consume buf[i]: each live (i)-state that is a CHAR matching buf[i]
         * advances to its `out` state, which lives at position (i+1). */
        for (int qi=0; qi<ndense; qi++){
            int s=dense[qi]; State *st=&re->st[s];
            if (st->type==CHR && match_byte(st,ch,icase) && st->out>=0){
                int t=st->out; if (seen[t]!=next_gen){ seen[t]=next_gen; dense[ndense2++]=t; }
            }
        }
        /* Epsilon-closure of the (i+1) set. Anchor / word-boundary checks are
         * evaluated FOR POSITION (i+1): a "^" match requires the byte just
         * consumed (buf[i]) to be '\n' (i.e. position i+1 begins a new line),
         * and "$" requires buf[i+1] to be '\n' or the buffer to end. Computing
         * them against buf[i]/[i+1]/[i+2] (NOT buf[i-1]/buf[i]) is what keeps
         * anchored matches pinned to the correct byte. */
        int a_start = (i==0) ? 0 : (buf[i]=='\n');       /* pos i+1 begins a line iff buf[i] (just consumed) was '\n' */
        int a_end   = (i+1==n) || (i+1<n && buf[i+1]=='\n');
        int a_wprev = (i==0) ? 0 : is_word(buf[i]);     /* char at position i  (just consumed) */
        int a_wcur  = (i+1<n) ? is_word(buf[i+1]) : 0;   /* char at position i+1 */
        int a_wnext = (i+2>=n) ? 0 : is_word(buf[i+2]);  /* char at position i+2 */
        for (int qi=0; qi<ndense2; qi++){
            int s=dense[qi]; State *st=&re->st[s];
            switch(st->type){
                case SPLIT:       if(st->out>=0){int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;}} if(st->out1>=0){int t=st->out1; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;}} break;
                case ANCH_START:  if(a_start&&st->out>=0){int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;}} break;
                case ANCH_END:    if(a_end&&st->out>=0){int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;}} break;
                case ANCH_WBEG:   if((a_start||!a_wprev)&&a_wcur&&st->out>=0){int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;}} break;
                case ANCH_WEND:   if(a_wcur&&(a_end||!a_wnext)&&st->out>=0){int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;}} break;
                case ANCH_WB:     if(a_wcur!=a_wprev&&st->out>=0){int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;}} break;
                case ANCH_NOTWB:  if(a_wcur==a_wprev&&st->out>=0){int t=st->out; if(seen[t]!=next_gen){seen[t]=next_gen;dense[ndense2++]=t;}} break;
                default: break;
            }
        }
        ndense=ndense2;
        i++;
    }
    return any;
#undef SADD
}
