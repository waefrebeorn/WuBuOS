/*
 * wubre.c - WuBu Regex Engine. Our OWN implementation. C11. strict.
 * ---------------------------------------------------------------------------
 * Canonical Thompson NFA (Russ Cox "Regex matching can be simple and fast"):
 * fragments carry a single dangling out-pointer, quantifiers are built with
 * SPLIT states, simulation is a single left-to-right pass (O(n*states), never
 * catastrophic). Unanchored search is done by prepending a "skip any byte and
 * re-enter" SPLIT, so we never restart at every offset (which would be O(n^2)
 * and fatal on the 4GB corpus).
 *
 * Supported: . * + ? | ( ) [ ] [^ ] ranges, \n \t \r \0 \xHH escapes,
 *            ^ $ line anchors, BRE (metachars literal unless \escaped) and ERE.
 * Documented-unsupported (like ripgrep's default engine): counted {m,n} and
 * backreferences \1.. (NFA cannot express them).
 *
 * License: WaefreBeorn Umbrella License v3.0
 * ---------------------------------------------------------------------------
 */
#include "wubre.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_STATES 8000

typedef enum { CHR, SPLIT, MATCH, ANCH_START, ANCH_END } Stype;

typedef struct State {
    Stype type;
    int c;                 /* CHR byte (case-folded if icase) */
    int cl;                /* 0=char 1=dot 2=class */
    int neg;               /* class negated */
    unsigned char *bits;   /* 256-bit class membership */
    int out;               /* primary epsilon/consume target */
    int out1;              /* SPLIT secondary target */
} State;

struct WURegex {
    State st[MAX_STATES];
    int nst;
    int start;
    int flags;
    /* optional required-literal prefilter: if set, wubre_search returns false
     * immediately unless the line contains this literal. Lets us skip the NFA
     * for the common case (e.g. 'wubu.*grep' -> required suffix "grep"). */
    const unsigned char *prefilter;
    int preflen;
};

/* dangling-pointer list: a chain of (state,field) to patch to one target */
typedef struct Dangle { int s; int field; struct Dangle *next; struct Dangle *regnext; } Dangle;

typedef struct {
    int start;
    Dangle *out;
    const char *src;     /* source span of this atom (for {n,m} expansion) */
    const char *src_end;
} Frag;

#define FRAG_NULL ((Frag){.start=-1,.out=NULL,.src=NULL,.src_end=NULL})

static State *add_state(WURegex *re, Stype t){
    if (re->nst >= MAX_STATES) return NULL;
    State *s = &re->st[re->nst++];
    memset(s,0,sizeof *s);
    s->out = s->out1 = -1;
    s->type = t;
    return s;
}

/* Re-implement patch cleanly: we keep the WURegex pointer in Frag build ctx */
typedef struct { WURegex *re; Dangle *all; } Ctx;

static void patch_to(Ctx *c, Dangle *d, int target){
    while (d){
        Dangle *n = d->next;
        if (d->field==0) c->re->st[d->s].out = target;
        else             c->re->st[d->s].out1 = target;
        /* unlink from the registry and free */
        Dangle **pp = &c->all;
        while (*pp){ if (*pp==d){ *pp = d->regnext; break; } pp = &(*pp)->regnext; }
        free(d);
        d = n;
    }
}
static Dangle *dangle_one(Ctx *c, int s, int field){
    Dangle *d = malloc(sizeof *d);
    d->s=s; d->field=field; d->next=NULL;
    d->regnext = c->all; c->all = d;   /* register for leak cleanup */
    return d;
}
/* An EMPTY fragment (e.g. "()" or a bare "|") is a zero-width match that must
 * still FORWARD to whatever follows. Represent it as an epsilon SPLIT whose
 * out AND out1 both dangle to the continuation (a 2-node dangle chain; patch_to
 * walks .next and links both). A bare MATCH with out=NULL would be unfollowable
 * by concatenation, causing empty groups to match everything (over-match bug). */
static Frag empty_frag(Ctx *c){
    WURegex *re=c->re;
    State *sp=add_state(re,SPLIT);
    int si=(int)(sp-re->st);
    Dangle *d0=dangle_one(c,si,0);   /* sp->out  */
    Dangle *d1=dangle_one(c,si,1);   /* sp->out1 */
    d0->next=d1;                     /* chain both so patch_to links both */
    return (Frag){ .start=si, .out=d0, .src=NULL, .src_end=NULL };
}

static int cfold(int c){ return (c>='A'&&c<='Z') ? c-'A'+'a' : c; }

/* ----- class/atom parsing ----- */
typedef struct {
    const char *p, *end;
    Ctx *cx;
    char *err; size_t errsz;
} P;

static Frag parse_alt(P *ps);  /* forward */

static int parse_escaped_byte(P *ps){
    /* assumes *ps->p == '\\' */
    ps->p++;
    if (ps->p >= ps->end) return -1;
    char e = *ps->p++;
    switch(e){
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '0': return 0;
        case 'x': {
            int v=0,k=0;
            while (ps->p<ps->end && k<2 && isxdigit((unsigned char)*ps->p)){
                int d = isdigit((unsigned char)*ps->p)?*ps->p-'0':(tolower((unsigned char)*ps->p)-'a'+10);
                v=v*16+d; ps->p++; k++;
            }
            return v & 0xff;
        }
        default: return (unsigned char)e;
    }
}

static Frag parse_class(P *ps){
    ps->p++; /* [ */
    int neg = 0;
    if (ps->p<ps->end && *ps->p=='^'){ neg=1; ps->p++; }
    unsigned char *bits = calloc(32,1);
    int prev=-1;
    while (ps->p<ps->end && *ps->p!=']'){
        int b;
        if (*ps->p=='\\') b = parse_escaped_byte(ps);
        else { b=(unsigned char)*ps->p; ps->p++; }
        if (b<0) break;
        /* range? the just-read char is '-' and follows a real char, and the
         * NEXT char is a real class member (not ']' or another '-') */
        if (b=='-' && prev>=0 && ps->p<ps->end && *ps->p!=']' && *ps->p!='-'){
            int e;
            if (*ps->p=='\\') e=parse_escaped_byte(ps);
            else { e=(unsigned char)*ps->p; ps->p++; }
            if (e<0) e='-';
            int lo=prev, hi=e; if (lo>hi){int t=lo;lo=hi;hi=t;}
            for (int x=lo; x<=hi && x<256; x++) bits[x>>3]|=(1u<<(x&7));
            prev=-1;
        } else {
            bits[b>>3]|=(1u<<(b&7));
            prev=b;
        }
    }
    if (ps->p<ps->end && *ps->p==']') ps->p++;
    State *s = add_state(ps->cx->re, CHR);
    s->cl=2; s->bits=bits; s->neg=neg?1:0;
    return (Frag){ .start=(int)(s-ps->cx->re->st), .out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0) };
}

static Frag parse_atom(P *ps){
    const char *s0 = ps->p;
    if (ps->p>=ps->end){ return empty_frag(ps->cx); }
    char ch=*ps->p;
    if (ch=='('){
        ps->p++;
        Frag f=parse_alt(ps);
        if (ps->p<ps->end && *ps->p==')') ps->p++;
        f.src=s0; f.src_end=ps->p;
        return f;
    }
    if (ch=='['){ Frag f=parse_class(ps); f.src=s0; f.src_end=ps->p; return f; }
    if (ch=='^'){ ps->p++; State *s=add_state(ps->cx->re,ANCH_START);
        Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f; }
    if (ch=='$'){ ps->p++; State *s=add_state(ps->cx->re,ANCH_END);
        Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f; }
    if (ch=='.'){ ps->p++; State *s=add_state(ps->cx->re,CHR); s->cl=1;
        Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f; }
    int c; int esc = (ch=='\\' && ps->p+1<ps->end);
    if (esc) c = parse_escaped_byte(ps);
    else { c=(unsigned char)ch; ps->p++; }
    if (c<0) c=0;
    State *s=add_state(ps->cx->re,CHR);
    s->cl=0; s->c = (ps->cx->re->flags & WUBRE_ICASE)? cfold(c):c;
    Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f;
}

static Frag parse_quant(P *ps){
    Frag f = parse_atom(ps);
    int guard=0;
    for (;;){
        if (++guard>100000){ fprintf(stderr,"parse_quant LOOP GUARD nst=%d remaining=%ld\n", ps->cx->re->nst, (long)(ps->end-ps->p)); fflush(stderr); break; }
        if (ps->p>=ps->end) break;
        char q=*ps->p; int kind=0;
        if (q=='*') kind=1;
        else if (q=='+') kind=2;
        else if (q=='?') kind=3;
        if (kind){
            ps->p++;
            WURegex *re=ps->cx->re;
            if (kind==1 || kind==3){ /* f*  and f? : SPLIT(out=f.start, out1=EXIT) */
                State *sp=add_state(re,SPLIT);
                sp->out=f.start;                 /* enter f */
                patch_to(ps->cx, f.out, (int)(sp-re->st)); /* after f, loop back to sp */
                f.start=(int)(sp-re->st);
                f.out=dangle_one(ps->cx,(int)(sp-re->st),1); /* EXIT = sp->out1 */
            } else { /* f+ : tail split loops; entry must be f.start */
                State *sp=add_state(re,SPLIT);
                sp->out1=f.start;                /* loop back into f */
                patch_to(ps->cx, f.out, (int)(sp-re->st)); /* after f -> decider */
                f.start=f.start;                 /* require >=1 f */
                f.out=dangle_one(ps->cx,(int)(sp-re->st),0); /* EXIT = sp->out */
            }
            continue;
        }
        /* ERE counted repetition {n} {n,} {n,m} ; in BRE braces are literal */
        if (q=='{' && !(ps->cx->re->flags & WUBRE_BRE)){
            const char *p=ps->p+1; int n=0,m=-1; int ok=0;
            if (p<ps->end && *p>='0' && *p<='9'){
                while (p<ps->end && *p>='0' && *p<='9'){ n=n*10+(*p-'0'); p++; }
                ok=1;
                if (p<ps->end && *p==','){
                    p++;
                    if (p<ps->end && *p>='0' && *p<='9'){ m=0; while(p<ps->end && *p>='0' && *p<='9'){ m=m*10+(*p-'0'); p++; } }
                    else m=-1; /* {n,} */
                } else m=n; /* {n} */
                if (p<ps->end && *p=='}'){ p++; }
                else ok=0;
            }
            if (!ok) break; /* not a quantifier; { is literal */
            const char *after = p;   /* resume parsing AFTER the consumed {n,m} */
            int total = (m<0) ? n : m;       /* max copies */
            Frag acc = FRAG_NULL; int have=0;
            for (int i=0;i<total;i++){
                ps->p = f.src;                /* replay the atom source */
                Frag c = parse_atom(ps);      /* one copy, no quantifier */
                if (i>=n){                     /* optional copies: wrap as c? */
                    State *sp=add_state(ps->cx->re,SPLIT);
                    sp->out=c.start;
                    patch_to(ps->cx, c.out, (int)(sp-ps->cx->re->st));
                    c.start=(int)(sp-ps->cx->re->st); c.out=dangle_one(ps->cx,(int)(sp-ps->cx->re->st),1);
                }
                if (!have){ acc=c; have=1; } else { patch_to(ps->cx, acc.out, c.start); acc.out=c.out; }
            }
            ps->p = after;                    /* resume after the quantifier */
            if (!have){ acc = empty_frag(ps->cx); }
            f=acc;
            break;   /* do NOT re-enter the quantifier loop on the same '{' */
        }
        break;
    }
    return f;
}

static Frag parse_concat(P *ps){
    Frag f = FRAG_NULL; int have=0;
    while (ps->p<ps->end){
        char ch=*ps->p;
        if (ch=='|'||ch==')') break;
        Frag a=parse_quant(ps);
        if (!have){ f=a; have=1; }
        else { patch_to(ps->cx, f.out, a.start); f.out=a.out; }
    }
    if (!have){ f = empty_frag(ps->cx); }
    return f;
}

static Frag parse_alt(P *ps){
    Frag f=parse_concat(ps);
    while (ps->p<ps->end && *ps->p=='|'){
        ps->p++;
        Frag g=parse_concat(ps);
        State *sp=add_state(ps->cx->re,SPLIT);
        sp->out=f.start;
        sp->out1=g.start;
        State *join=add_state(ps->cx->re,SPLIT); /* shared continuation */
        patch_to(ps->cx, f.out, (int)(join-ps->cx->re->st));
        patch_to(ps->cx, g.out, (int)(join-ps->cx->re->st));
        f.start=(int)(sp-ps->cx->re->st);
        f.out=dangle_one(ps->cx,(int)(join-ps->cx->re->st),0); /* EXIT = join->out */
    }
    return f;
}

WURegex *wubre_compile(const char *pat, int flags, char *err, size_t errsz){
    /* BRE mode: translate to ERE. In BRE, ( ) | + ? { } are literal unless
     * backslashed; \( \) \| \+ \? become the ERE metachars. We rewrite the
     * pattern into ERE form and parse normally. */
    char *trans = NULL;
    const char *use = pat;
    if (flags & WUBRE_BRE){
        size_t L = strlen(pat);
        trans = malloc(L*2+1);
        char *o = trans;
        for (size_t i=0;i<L;i++){
            char c = pat[i];
            if (c=='\\' && i+1<L){
                char n = pat[i+1];
                /* \( ) | + ? { }  ->  metachar ; other \X stays literal \X */
                if (n=='('||n==')'||n=='|'||n=='+'||n=='?'||n=='{'||n=='}'){
                    *o++ = n; i++; continue;
                }
                *o++ = '\\'; *o++ = n; i++; continue;
            }
            /* bare BRE metachars are literal -> escape for ERE parser */
            if (c=='('||c==')'||c=='|'||c=='+'||c=='?'||c=='{'||c=='}'){
                *o++ = '\\'; *o++ = c; continue;
            }
            *o++ = c;
        }
        *o = 0;
        use = trans;
        /* The pattern is now ERE text; the BRE-ness has been encoded into the
         * transformed string, so clear WUBRE_BRE so parse_quant treats {}
         * as an ERE interval quantifier (not a literal). ICASE stays. */
        flags &= ~WUBRE_BRE;
    }
    WURegex *re=calloc(1,sizeof *re);
    if (!re){ if(errsz)snprintf(err,errsz,"oom"); free(trans); return NULL; }
    re->flags=flags;
    /* Prefilter: extract the longest literal run in the ERE text as a REQUIRED
     * substring. Any match must contain it, so wubre_search can memmem-skip lines
     * that don't. Metacharacters (. * + ? ( ) [ ] { } | ^ $ \\) and their escaped
     * forms are not literal runs. Only safe for non-icase (case-exact memmem).
     * SAFETY: a literal inside a top-level alternation (foo|bar) is NOT required,
     * so if the pattern contains a top-level '|' (outside [...] classes) we skip
     * the prefilter entirely to stay correct. */
    re->prefilter=NULL; re->preflen=0;
    if (!(flags & WUBRE_ICASE)) {
        /* A prefilter literal is only SAFE if every char in the run is
         * individually MANDATORY: not inside a class, not part of an escape,
         * and crucially not directly quantified by * ? + {n} (which would make
         * it optional) and not preceding '(' (a group that may repeat). We also
         * reject any top-level alternation (foo|bar): a literal on one branch is
         * not required. The chosen run is copied into an owned buffer so it
         * survives the BRE-translation free(trans). */
        /* detect top-level alternation '|' (not inside [...]) */
        int has_alt=0; int in_class=0;
        for (const char *q=use; *q; q++){
            if (*q=='\\' && q[1]) { q++; continue; }
            if (*q=='[') in_class=1;
            else if (*q==']') in_class=0;
            else if (*q=='|' && !in_class) { has_alt=1; break; }
        }
        if (!has_alt) {
            const char *p = use;
            const char *best=NULL; int bestlen=0;
            /* A char is a PLAIN LITERAL iff it is not a regex metachar/anchor. */
            const char *META = ".^$*+?()[]{}|\\";
            while (*p) {
                if (*p=='\\') { p++; if (*p) p++; continue; } /* skip escapes */
                if (*p=='[') { /* skip whole class [..] */
                    const char *q=p+1;
                    if (*q=='^') q++;
                    if (*q==']') q++; /* a ] right after [ or [^ is literal */
                    while (*q && *q!=']') q++;
                    if (*q==']') q++;
                    p=q; continue;
                }
                if (*p=='{') { /* skip whole counted-quantifier {n,m} body */
                    const char *q=p+1;
                    while (*q && *q!='}') q++;
                    if (*q=='}') q++;
                    p=q; continue;
                }
                if (strchr(META, *p)!=NULL) { p++; continue; } /* anchors/metachars */
                /* *p is plain literal. It is MANDATORY only if the NEXT char is
                 * not an optional-maker (* ? + { ( [ ). Measure a run of such
                 * chars; always advance p at least one so the outer loop ends. */
                const char *OPT="?*+{(";
                const char *run=p; int rl=0;
                while (*p) {
                    if (*p=='\\') { p++; if (*p) p++; break; } /* run ends at escape */
                    if (*p=='[') break;                         /* ends at class */
                    if (strchr(META, *p)!=NULL) break;          /* ends at meta/anchor */
                    const char *nx = p+1;
                    if (*nx && strchr(OPT, *nx)!=NULL) break;   /* *p itself optional */
                    p++; rl++;
                }
                if (rl==0) p++;   /* this char was not part of a mandatory run; skip it */
                if (rl>bestlen){ bestlen=rl; best=run; }
            }
            if (bestlen>=2) { /* need length>=2 to be worth a memmem */
                char *buf=malloc((size_t)bestlen);
                if (buf){ memcpy(buf,best,(size_t)bestlen); re->prefilter=(unsigned char*)buf; re->preflen=bestlen; }
            }
        }
    }
    Ctx cx; cx.re=re; cx.all=NULL;
    P ps; ps.p=use; ps.end=use+strlen(use); ps.cx=&cx; ps.err=err; ps.errsz=errsz;
    if (ps.p>=ps.end){ State *m=add_state(re,MATCH); re->start=(int)(m-re->st); free(trans); return re; }
    Frag f=parse_alt(&ps);
    State *m=add_state(re,MATCH);
    patch_to(&cx, f.out, (int)(m-re->st));
    int start=f.start;
    /* unanchored: prepend SPLIT that can skip one byte and re-enter */
    if (!(flags & 0)) { /* always unanchored unless ^ present at very start */
        /* detect leading ^ */
        int anchored = (ps.p>use && use[0]=='^') || (use[0]=='^');
        if (!anchored){
            State *sp=add_state(re,SPLIT);
            sp->out=start;
            State *skip=add_state(re,CHR);
            skip->cl=1; /* any byte */
            skip->out=(int)(sp-re->st); /* loop back to allow start anywhere */
            sp->out1=(int)(skip-re->st);
            start=(int)(sp-re->st);
        }
    }
    re->start=start;
    /* free any dangle nodes not yet consumed by patch_to (parse discards) */
    while (cx.all){ Dangle *n=cx.all->regnext; free(cx.all); cx.all=n; }
    free(trans);
    return re;
}

/* ----- simulation ----- */
static int match_byte(State *st, int ch, int icase){
    if (st->type!=CHR) return 0;
    int c=ch;
    if (st->cl==1) return (c!='\n'); /* dot: never matches newline (dotnl off) */
    if (st->cl==2){ int in=(st->bits[c>>3]>>(c&7))&1; return st->neg? !in:in; }
    int want=st->c;
    if (icase) return cfold(c)==want;
    return c==want;
}

/* portable substring search with memchr first-byte skip (memmem is GNU-only) */
static const unsigned char *wub_memmem(const unsigned char *hay, size_t hn,
                                       const unsigned char *needle, size_t nn){
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

bool wubre_search(const WURegex *re_, const unsigned char *buf, size_t n){
    WURegex *re=(WURegex*)(uintptr_t)re_;
    int icase=(re->flags&WUBRE_ICASE)?1:0;
    /* required-literal prefilter: if the line lacks the mandatory literal, no
     * match is possible -> skip the NFA entirely (the big speed lever). */
    if (re->prefilter && re->preflen>0) {
        if (re->preflen > (int)n) return false;
        if (!wub_memmem(buf, n, re->prefilter, (size_t)re->preflen)) return false;
    }
    /* Sparse-set active states with a generation counter (Russ Cox / RE2 Pike
     * VM). We never memset an 8000-int bitset per byte; we bump a generation and
     * only touch the live states. seen[s]==gen means s is currently in the set. */
    int dense[MAX_STATES], seen[MAX_STATES];
    memset(seen, 0, sizeof seen);   /* zero once per line (NOT per byte) */
    int gen=1;                     /* gen starts at 1 since seen[] is zeroed */
    int at_start0 = 1;
    int at_end0   = (n==0) || (buf[0]=='\n');
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
            default: break;
        }
    }
    size_t i=0;
    for (;;){
        for (int qi=0; qi<ndense; qi++) if (re->st[dense[qi]].type==MATCH) return true;
        if (i>=n) break;
        unsigned char ch=buf[i];
        int at_start = (i==0)||(buf[i-1]=='\n');
        int at_end = (i+1==n)||(buf[i+1]=='\n');
        int next_gen = gen+1;       /* the set we are building */
        gen = next_gen;
        int ndense2=0;
        for (int qi=0; qi<ndense; qi++){
            int s=dense[qi]; State *st=&re->st[s];
            if (st->type==CHR && match_byte(st,ch,icase) && st->out>=0){
                int t=st->out;
                if (seen[t]!=next_gen){ seen[t]=next_gen; dense[ndense2++]=t; }
            }
        }
        /* expand epsilons of the next set (honoring anchors) */
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
                default: break;
            }
        }
        ndense=ndense2;            /* dense now holds the next_gen set */
        i++;
    }
    return false;
#undef SADD
}

void wubre_free(WURegex *re){
    if (!re) return;
    for (int i=0;i<re->nst;i++) if (re->st[i].cl==2) free(re->st[i].bits);
    free((void*)re->prefilter);   /* owned copy, may be NULL */
    free(re);
}
