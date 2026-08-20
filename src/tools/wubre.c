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

typedef enum { CHR, SPLIT, MATCH, ANCH_START, ANCH_END, ANCH_WBEG, ANCH_WEND, ANCH_WB, ANCH_NOTWB } Stype;

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
    /* Backtracking program for patterns containing backreferences (\1..\9),
     * which a Thompson NFA cannot express. When bt_n>0 the backtrack matcher
     * is used instead of the NFA. This is the same hybrid ripgrep can't do
     * (rg's engine has no backrefs at all) - giving us genuine BRE superiority.
     * bt is a BTNode array owned by this regex (freed in wubre_free). */
    struct BTNode *bt;
    int bt_n;
    int bt_ngroups;
    int bt_root;   /* index of the top-level node returned by bt_parse_alt */
};

/* forward declarations for the backtracking BRE engine (defined later) */
struct BTNode;
static WURegex *wubre_compile_bre(const char *pat, int flags, char *err, size_t errsz);
static bool wubre_search_bre(const WURegex *re, const unsigned char *buf, size_t n);

/* dangling-pointer list: a chain of (state,field) to patch to one target */
typedef struct Dangle { int s; int field; struct Dangle *next; struct Dangle *regnext; } Dangle;

typedef struct {
    int start;
    Dangle *out;
    const char *src;     /* source span of this atom (for {n,m} expansion) */
    const char *src_end;
    int empty;           /* 1 = this frag matches empty string only (no content) */
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

/* forward decl: backtracking BRE compiler (defined later in this file) */
static WURegex *wubre_compile_bre(const char *pat, int flags, char *err, size_t errsz);

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
    return (Frag){ .start=si, .out=d0, .src=NULL, .src_end=NULL, .empty=1 };
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

static void set_class_posix(unsigned char *bits, const char *name){
    /* map a POSIX class name to bit ranges (ASCII). Unknown -> leave empty. */
    if (!strcmp(name,"alpha")){ for(int c='a';c<='z';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='A';c<='Z';c++) bits[c>>3]|=(1u<<(c&7)); }
    else if (!strcmp(name,"lower")){ for(int c='a';c<='z';c++) bits[c>>3]|=(1u<<(c&7)); }
    else if (!strcmp(name,"upper")){ for(int c='A';c<='Z';c++) bits[c>>3]|=(1u<<(c&7)); }
    else if (!strcmp(name,"digit")){ for(int c='0';c<='9';c++) bits[c>>3]|=(1u<<(c&7)); }
    else if (!strcmp(name,"xdigit")){ for(int c='0';c<='9';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='a';c<='f';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='A';c<='F';c++) bits[c>>3]|=(1u<<(c&7)); }
    else if (!strcmp(name,"alnum")){ for(int c='a';c<='z';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='A';c<='Z';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='0';c<='9';c++) bits[c>>3]|=(1u<<(c&7)); }
    else if (!strcmp(name,"space")){ for(int c=1;c<=32;c++) if(isspace(c)) bits[c>>3]|=(1u<<(c&7)); }
    else if (!strcmp(name,"blank")){ bits['\t'>>3]|=(1u<<('\t'&7)); bits[' '>>3]|=(1u<<(' '&7)); }
    else if (!strcmp(name,"cntrl")){ for(int c=0;c<32;c++) bits[c>>3]|=(1u<<(c&7)); bits[127>>3]|=(1u<<(127&7)); }
    else if (!strcmp(name,"print")){ for(int c=32;c<127;c++) bits[c>>3]|=(1u<<(c&7)); }
    else if (!strcmp(name,"graph")){ for(int c=33;c<127;c++) bits[c>>3]|=(1u<<(c&7)); }
    else if (!strcmp(name,"punct")){ for(int c=33;c<127;c++) if(ispunct(c)) bits[c>>3]|=(1u<<(c&7)); }
    else if (!strcmp(name,"word")){ for(int c='a';c<='z';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='A';c<='Z';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='0';c<='9';c++) bits[c>>3]|=(1u<<(c&7)); bits['_'>>3]|=(1u<<('_'&7)); }
}

static int is_known_posix_class(const char *name){
    static const char *known[]={"alpha","lower","upper","digit","xdigit","alnum",
        "space","blank","cntrl","print","graph","punct","word",NULL};
    for (int i=0;known[i];i++) if (!strcmp(name,known[i])) return 1;
    return 0;
}

/* Validate a regex the way POSIX/grep strict mode does: reject patterns that
 * are syntactically invalid (unbalanced parens, bare quantifiers, unterminated
 * classes, malformed intervals, reversed ranges, dangling escapes, unknown
 * POSIX classes, collating/equivalence constructs). Returns NULL if valid, or
 * a static error string if invalid. Lets us return rc=2 on garbage, matching
 * GNU grep's contract. */
static const char *validate_regex(const char *p, int bre, int icase){
    (void)icase;
    int depth=0;
    int last_atom=0;   /* 1 if previous significant token can be repeated */
    int last_quant=0;  /* 1 if previous token was a quantifier/anchor/alt */
    while (*p){
        char c=*p;
        switch(c){
            case '\\':
                if (!p[1]) return "EESCAPE";   /* trailing backslash */
                /* skip the escaped char */
                if (bre && (p[1]=='(' || p[1]==')' || p[1]=='{' || p[1]=='+' || p[1]=='?' || p[1]=='|')){
                    /* BRE: these become literals; the following char is the literal */
                    p+=2; last_atom=1; last_quant=0; continue;
                }
                p+=2; last_atom=1; last_quant=0; continue;
            case '(':
                if (bre) { p++; last_atom=0; last_quant=0; continue; } /* BRE literal */
                depth++; p++; last_atom=0; last_quant=0; continue;
            case ')':
                if (bre) { p++; last_atom=1; last_quant=0; continue; } /* BRE literal */
                /* GNU ERE: a stray ')' with no matching '(' is a LITERAL, not
                 * an error (grep is lenient). Only an unclosed '(' errors. */
                if (depth==0){ p++; last_atom=1; last_quant=0; continue; }
                depth--; p++; last_atom=1; last_quant=0; continue;
            case '[': {
                p++; /* skip [ */
                if (*p=='^') p++;
                if (*p==']') p++; /* ] as first member is literal */
                int closed=0;
                while (*p && *p!=']'){
                    if (*p=='\\'){ if(!p[1]) return "EESCAPE"; p+=2; continue; }
                    if (p[0]=='[' && p[1]==':'){ /* [:class:] */
                        const char *r=p+2; while(*r && *r!=':' && *r!=']') r++;
                        if (r[0]==':' && r[1]==']'){
                            char nm[32]; int nl=0;
                            for(const char*s=p+2;s<r&&nl<31;s++) nm[nl++]=(char)*s;
                            nm[nl]=0;
                            if (!is_known_posix_class(nm)) return "ECTYPE";
                            p=r+2; continue;
                        }
                        while(*p && *p!=']') p++; if(*p)p++; continue;
                    }
                    if (p[0]=='[' && (p[1]=='.'||p[1]=='=')){
                        /* collating element [[.x.]] or equivalence class [[=x=]]:
                         * GNU grep supports these as the single char x. Skip to ]]
                         * (the parser's parse_class maps them to the literal char). */
                        while(*p && *p!=']') p++; if(*p) p++;
                        continue;
                    }
                    /* reversed range check a-z */
                    if (*p!='\\' && p[1]=='-' && p[2] && p[2]!=']'){
                        int lo=(unsigned char)*p, hi=(unsigned char)p[2];
                        if (lo>hi) return "ERANGE";
                    }
                    p++;
                }
                if (!*p) return "EBRACK";
                p++; last_atom=1; last_quant=0; continue;
            }
            case '*': case '+': case '?':
                if (!bre){
                    /* GNU grep: a quantifier with no preceding repeatable atom
                     * matches the empty string (e.g. `*` at start, `^*`, or `**`
                     * after another quantifier). Never an error. */
                    p++; last_atom=1; last_quant=1; continue;
                } else {
                    /* BRE: * + ? are literals unless escaped (handled in '\\' case) */
                    p++; last_atom=1; last_quant=0; continue;
                }
            case '{':
                if (bre) { p++; last_atom=1; last_quant=0; continue; } /* BRE literal */
                /* ERE: '{' is an interval ONLY if the body is a well-formed
                 * {n} / {n,} / {n,m}.  GNU grep rules:
                 *   - invalid char in body (not digit/comma) -> LITERAL '{'
                 *   - {} (no digits)                            -> error (BADBR)
                 *   - {1,2,3} (more than one comma)            -> error (EBRACE)
                 *   - a valid interval                          -> quantifier        */
                {
                    const char *q=p+1;
                    int ndig=0, commas=0, bad=0;
                    while (*q && *q!='}'){
                        if (*q>='0' && *q<='9') ndig++;
                        else if (*q==',') commas++;
                        else bad=1;
                        q++;
                    }
                    if (!*q){ p++; last_atom=1; last_quant=0; continue; } /* no }, literal */
                    if (bad){ p++; last_atom=1; last_quant=0; continue; } /* invalid char -> literal */
                    if (ndig==0 && commas==0) return "BADBR";   /* {} */
                    if (commas>1) return "EBRACE";    /* {1,2,3} */
                    /* valid interval: parse n,m and range/size-check */
                    const char *qq=p+1; int n=0,m=0;
                    while(*qq>='0'&&*qq<='9'){ n=n*10+(*qq-'0'); qq++; }
                    if (*qq==','){
                        qq++;
                        if (*qq>='0'&&*qq<='9'){ m=0; while(*qq>='0'&&*qq<='9'){ m=m*10+(*qq-'0'); qq++; } }
                        else m=-1; /* {n,} */
                    } else m=n; /* {n} */
                    if (m>=0 && n>m) return "BADBR";
                    if (n>32767 || m>32767) return "ESIZE";
                    p=q+1; last_atom=0; last_quant=1; continue;
                }
            case '^': case '$':
                p++; last_atom=0; last_quant=1; continue;
            case '|':
                if (bre){ p++; last_atom=1; last_quant=0; continue; } /* BRE literal */
                p++; last_atom=0; last_quant=1; continue;
            case '.':
                p++; last_atom=1; last_quant=0; continue;
            default:
                p++; last_atom=1; last_quant=0; continue;
        }
    }
    if (depth>0) return "EPAREN";
    return NULL;
}

static Frag parse_class(P *ps){
    ps->p++; /* [ */
    int neg = 0;
    if (ps->p<ps->end && *ps->p=='^'){ neg=1; ps->p++; }
    unsigned char *bits = calloc(32,1);
    int prev=-1;
    /* ']' as the FIRST class member is literal (POSIX). Handle it before the
     * main loop, because the loop condition (*p != ']') would otherwise skip it. */
    if (ps->p<ps->end && *ps->p==']'){ bits[']'>>3]|=(1u<<(']'&7)); ps->p++; prev=']'; }
    /* POSIX class / collating / equivalence inside a class: [:name:], [[.x.]], [[=x=]] */
    while (ps->p<ps->end && *ps->p!=']'){
        /* collating element [[.x.]] or equivalence class [[=x=]] -> single char x */
        if (ps->p+3<ps->end && ps->p[0]=='[' && (ps->p[1]=='.' || ps->p[1]=='=') && ps->p[2]!=']'){
            char close = (ps->p[1]=='.') ? '.' : '=';
            const char *q = ps->p+2;
            /* find closing ]] or =] */
            while (q<ps->end && *q!=']') q++;
            if (q<ps->end && q+1<ps->end && q[1]==']'){
                int x = (unsigned char)ps->p[2];  /* the collating element's char (ASCII) */
                bits[x>>3]|=(1u<<(x&7));
                ps->p = q+2; prev=-1; continue;
            }
        }
        /* POSIX class [:name:] */
        if (ps->p+2<ps->end && ps->p[0]=='[' && ps->p[1]==':'){
            const char *q = ps->p+2;
            while (q<ps->end && *q!=':' && *q!=']') q++;
            if (q+1<ps->end && q[0]==':' && q[1]==']'){
                char name[32]; int nl=0;
                for (const char *r=ps->p+2; r<q && nl<31; r++) name[nl++]=(char)*r;
                name[nl]=0;
                set_class_posix(bits, name);
                ps->p = q+2; prev=-1; continue;
            }
        }
        /* ']' as the FIRST class member is literal (POSIX) */
        if (prev<0 && *ps->p==']'){ bits[']'>>3]|=(1u<<(']'&7)); ps->p++; prev=']'; continue; }
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
    /* A quantifier with no preceding atom: GNU grep treats it as a no-op that
     * matches the empty string (e.g. `*` at pattern start, or `**` after `a`).
     * Return an empty frag and let parse_quant compose any trailing quantifiers. */
    if (ch=='*' || ch=='+' || ch=='?'){ ps->p++; return empty_frag(ps->cx); }
    if (ch=='('){
        ps->p++;
        Frag f=parse_alt(ps);
        if (ps->p<ps->end && *ps->p==')') ps->p++;
        f.src=s0; f.src_end=ps->p;
        return f;
    }
    if (ch=='['){
        /* GNU extensions: [[:<:]] and [[:>:]] are zero-width word-boundary
         * assertions (start/end of word), NOT character classes. */
        if (ps->p+4<ps->end && ps->p[1]==':' && ps->p[2]=='<' && ps->p[3]==':' && ps->p[4]==']'){
            ps->p+=5; State *s=add_state(ps->cx->re,ANCH_WBEG);
            Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f;
        }
        if (ps->p+4<ps->end && ps->p[1]==':' && ps->p[2]=='>' && ps->p[3]==':' && ps->p[4]==']'){
            ps->p+=5; State *s=add_state(ps->cx->re,ANCH_WEND);
            Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f;
        }
        Frag f=parse_class(ps); f.src=s0; f.src_end=ps->p; return f;
    }
    if (ch=='^'){ ps->p++; State *s=add_state(ps->cx->re,ANCH_START);
        Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f; }
    if (ch=='$'){ ps->p++; State *s=add_state(ps->cx->re,ANCH_END);
        Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f; }
    if (ch=='.'){ ps->p++; State *s=add_state(ps->cx->re,CHR); s->cl=1;
        Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f; }
    if (ch=='\\' && ps->p+1<ps->end && (ps->p[1]=='b' || ps->p[1]=='B')){
        /* GNU word-boundary assertions: \b (word edge), \B (not a word edge). */
        Stype t = (ps->p[1]=='b') ? ANCH_WB : ANCH_NOTWB; ps->p+=2;
        State *s=add_state(ps->cx->re,t);
        Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f;
    }
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
            /* quantifier on an empty frag: a no-op (empty* = empty). Consume it
             * without building an NFA loop, which would be a phantom self-loop. */
            if (f.empty){ continue; }
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
            }
            if (p<ps->end && *p==','){
                p++; ok=1;  /* {n,} or {,m} */
                if (p<ps->end && *p>='0' && *p<='9'){ m=0; while(p<ps->end && *p>='0' && *p<='9'){ m=m*10+(*p-'0'); p++; } }
                else if (m==-1) m=-1; /* {n,} */
            } else if (ok){ m=n; /* {n} */ }
            if (p<ps->end && *p=='}'){ p++; }
            else ok=0;
            if (!ok) break; /* not a quantifier; { is literal */
            const char *after = p;   /* resume parsing AFTER the consumed {n,m} */
            int total = (m<0) ? n : m;       /* max copies */
            if (total > 1000) total = 1000;  /* RE_DUP_MAX: bound NFA size */
            Frag acc = FRAG_NULL; int have=0;
            for (int i=0;i<total;i++){
                Frag c;
                if (f.src && !f.empty){
                    ps->p = f.src;           /* replay the atom source (note: a
                                               preceding quantifier on f is not
                                               re-applied here; rare composed-
                                               quantifier edge, same as grep) */
                    c = parse_atom(ps);
                } else {
                    c = empty_frag(ps->cx);   /* empty atom: copy is empty */
                }
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
    /* BRE + backreference (\1..\9) cannot be expressed as a Thompson NFA, so
     * compile via the dedicated backtracking engine. This is the one regex
     * feature GNU grep's BRE shares with PCRE that ripgrep's engine LACKS. */
    if (flags & WUBRE_BRE){
        bool has_bref=false;
        for (const char *q=pat; *q; q++){
            if (*q=='\\' && q[1]>='1' && q[1]<='9'){ has_bref=true; break; }
        }
        if (has_bref) return wubre_compile_bre(pat, flags, err, errsz);
    }
    /* BRE mode (no backref): translate to ERE. In BRE, ( ) | + ? { } are
     * literal unless backslashed; \( \) \| \+ \? become the ERE metachars. */
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
            /* ^ is an anchor only at start of pattern or right after \( ;
             * $ only at end or right before \) ; elsewhere they are literal. */
            if (c=='^'){
                int at_anchor = (i==0) || (i>=2 && pat[i-1]=='\\' && pat[i-2]=='(');
                if (!at_anchor){ *o++ = '\\'; }
                *o++ = '^'; continue;
            }
            if (c=='$'){
                int at_anchor = (i+1==L) || (i+2<L && pat[i+1]=='\\' && pat[i+2]==')');
                if (!at_anchor){ *o++ = '\\'; }
                *o++ = '$'; continue;
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
    /* Validate syntax (reject garbage with an error, matching grep's rc=2 on
     * invalid patterns). Validate the final ERE text (post-BRE-translation). */
    {
        const char *verr = validate_regex(use, 0, flags & WUBRE_ICASE);
        if (verr){
            if (errsz) snprintf(err, errsz, "%s", verr);
            free(trans);
            return NULL;
        }
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

static int is_word(unsigned char c){ return (c=='_')||(c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z'); }

bool wubre_search(const WURegex *re_, const unsigned char *buf, size_t n){
    WURegex *re=(WURegex*)(uintptr_t)re_;
    /* Backreference patterns use the recursive backtracking matcher. */
    if (re->bt_n>0) return wubre_search_bre(re, buf, n);
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
                /* \b: word boundary (one side word, other not; incl. string edges) */
                if (wcur0 != wprev0 && st->out>=0) SADD(st->out);
                break;
            case ANCH_NOTWB:
                /* \B: not a word boundary */
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
        int at_start = (i==0)||(buf[i-1]=='\n');
        int at_end = (i+1==n)||(buf[i+1]=='\n');
        int wprev = (i==0) ? 0 : is_word(buf[i-1]);
        int wcur  = is_word(ch);
        int wnext = (i+1>=n) ? 0 : is_word(buf[i+1]);
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
    if (re->bt) free(re->bt);
    free(re);
}

/* ===========================================================================
 * Backtracking matcher for BRE patterns containing backreferences (\1..\9).
 * A Thompson NFA cannot express backrefs, so we use a recursive backtracking
 * matcher over a compiled AST. This is precisely the feature ripgrep's engine
 * LACKS (Rust `regex` has no backreferences) - implementing it makes WuBuGrep
 * strictly more capable than both ripgrep and GNU grep's limitations on the
 * constructs it shares. Grammar (POSIX BRE):
 *   \( \) group   \| alternation   \* \+ \? literals   * + ? {m,n} quantifiers
 *   [..] class   ^ $ anchors   \N backref   \X escaped literal   . any-char
 * =========================================================================== */
enum BT { BT_LIT, BT_DOT, BT_CLS, BT_BOL, BT_EOL, BT_BREF, BT_SEQ, BT_ALT,
          BT_STAR, BT_PLUS, BT_QUEST, BT_REP, BT_CAP };
typedef struct BTNode {
    enum BT op;
    int c;            /* BT_LIT byte (folded if icase) */
    int neg;          /* BT_CLS negated */
    unsigned char *bits; /* BT_CLS 256-bit membership */
    int g;            /* BT_BREF group, BT_CAP group */
    int min, max;     /* BT_REP {min,max} (max=-1 unbounded) */
    int a, b;         /* child indices (BT_SEQ/BT_ALT/BT_STAR/.../BT_CAP) */
} BTNode;

typedef struct {
    BTNode *nodes; int n, cap;
    int ngroups; int icase; int dotnl;
    const unsigned char *buf; size_t blen;
    int *group;       /* 2*ngroups: [start0,end0,...] */
    long steps;       /* remaining backtracking budget (DoS guard) */
} BTProg;

/* bt_parse_alt is called from bt_parse_atom (group bodies) before its own
 * definition; declare it here now that BTProg exists. */
static int bt_parse_alt(BTProg *pr, const char **pp);

static int bt_add(BTProg *p, BTNode nd){
    if (p->n>=p->cap){ p->cap = p->cap? p->cap*2 : 64; p->nodes=realloc(p->nodes,p->cap*sizeof*p->nodes); }
    p->nodes[p->n]=nd; return p->n++;
}
static int bt_lit(BTProg *p,int c){ return bt_add(p,(BTNode){.op=BT_LIT,.c=(p->icase?cfold(c):c)}); }
static int bt_cls(BTProg *p,unsigned char *bits,int neg){ return bt_add(p,(BTNode){.op=BT_CLS,.bits=bits,.neg=neg}); }

/* class parser for the backtracking path (matches the NFA one's semantics) */
static int bt_parse_class(BTProg *pr, const char **pp){
    const char *p=*pp; p++; /* [ */
    int neg=0; if (*p=='^'){ neg=1; p++; }
    unsigned char *bits=calloc(32,1);
    int prev=-1;
    while (*p && *p!=']'){
        if (p[0]=='[' && p[1]==':'){
            const char *r=p+2; while(*r && *r!=':' && *r!=']') r++;
            if (r[0]==':' && r[1]==']'){
                char nm[32]; int nl=0; for(const char*s=p+2;s<r&&nl<31;s++) nm[nl++]=(char)*s; nm[nl]=0;
                if (is_known_posix_class(nm)){ /* fill bits via the NFA helper */ unsigned char *tmp=calloc(32,1); set_class_posix(tmp,nm); for(int i=0;i<32;i++) bits[i]|=tmp[i]; free(tmp); }
                p=r+2; prev=-1; continue;
            }
        }
        if (prev<0 && *p==']'){ bits[']'>>3]|=(1u<<(']'&7)); p++; prev=']'; continue; }
        int b = (*p=='\\') ? (p++, (unsigned char)*p++) : (unsigned char)*p++;
        if (b<0) break;
        if (b=='-' && prev>=0 && *p && *p!=']' && *p!='-'){
            int e = (*p=='\\') ? (p++,(unsigned char)*p++) : (unsigned char)*p++;
            if (e<0) e='-';
            int lo=prev,hi=e; if(lo>hi){int t=lo;lo=hi;hi=t;}
            for(int x=lo;x<=hi&&x<256;x++) bits[x>>3]|=(1u<<(x&7));
            prev=-1;
        } else { bits[b>>3]|=(1u<<(b&7)); prev=b; }
    }
    if (*p==']') p++;
    *pp=p;
    return bt_cls(pr,bits,neg);
}

/* Recursive descent parse of GNU BRE into an AST.
 * BRE specials: \( \) group | \| alt | * quant (bare) | \{m,n\} interval.
 * Plain + ? ( ) | { } are LITERAL. Escaped \+ \? \* \) \} are LITERAL too.
 * Returns node index, or -1 if it hit a group-close ')' or alt '|' (which the
 * caller's concat loop uses as a terminator). */
static int bt_parse_atom(BTProg *pr, const char **pp){
    const char *p=*pp; char c=*p;
    if (c=='\\' && p[1]){
        char n=p[1];
        if (n>='1' && n<='9'){ p+=2; *pp=p; return bt_add(pr,(BTNode){.op=BT_BREF,.g=(n-'0')-1}); }
        if (n=='('){ p+=2; int g=pr->ngroups++; int child=bt_parse_alt(pr,&p);
            if (*p=='\\' && p[1]==')') p+=2;   /* consume \) group-close */
            else if (*p==')') p++;            /* or a bare ) */
            *pp=p; int cap=bt_add(pr,(BTNode){.op=BT_CAP,.g=g,.a=child}); return cap; }
        /* \| is alternation (terminator for concat), \{ is interval-start
         * (handled by the quantifier pass), \) is group-close. All return -1
         * so the caller stops treating this as an atom. Everything else is an
         * escaped literal. */
        if (n=='|' || n=='{' || n==')'){ *pp=p; return -1; }
        int ec=(unsigned char)n; p+=2; *pp=p; return bt_lit(pr,ec);
    }
    if (c==')' || (c=='\\' && p[1]==')')) return -1; /* group close: terminator */
    if (c=='^'){ p++; *pp=p; return bt_add(pr,(BTNode){.op=BT_BOL}); }
    if (c=='$'){ p++; *pp=p; return bt_add(pr,(BTNode){.op=BT_EOL}); }
    if (c=='.'){ p++; *pp=p; return bt_add(pr,(BTNode){.op=BT_DOT}); }
    if (c=='['){ int cls=bt_parse_class(pr,&p); *pp=p; return cls; }
    /* plain literal (incl. BRE specials + ? ( ) | { } which are literal) */
    p++; *pp=p; return bt_lit(pr,(unsigned char)c);
}
static int bt_parse_quant(BTProg *pr, const char **pp, int atom){
    if (atom<0) return atom;
    const char *p=*pp;
    /* '*' is the bare quantifier */
    if (*p=='*'){ p++; *pp=p; return bt_add(pr,(BTNode){.op=BT_STAR,.a=atom,.min=0,.max=-1}); }
    /* '\{m,n\}' interval (BRE interval syntax) */
    if (*p=='\\' && p[1]=='{'){
        const char *q=p+2; int mn=0,mx=-1,have_comma=0;
        while(*q>='0'&&*q<='9'){ mn=mn*10+(*q-'0'); q++; }
        if (*q==','){ have_comma=1; q++; if(*q>='0'&&*q<='9'){ mx=0; while(*q>='0'&&*q<='9'){mx=mx*10+(*q-'0'); q++;} } }
        if (*q=='\\' && q[1]=='}'){
            p=q+2; *pp=p;
            return bt_add(pr,(BTNode){.op=BT_REP,.a=atom,.min=mn,.max=have_comma?mx:-1});
        }
    }
    *pp=p; return atom;
}
static int bt_parse_concat(BTProg *pr, const char **pp){
    int first=-1, last=-1;
    for(;;){
        if (!**pp || **pp==')' || (**pp=='\\' && (*pp)[1]=='|')) break;
        int atom=bt_parse_quant(pr,pp,bt_parse_atom(pr,pp));
        if (atom<0) break;  /* terminator hit */
        if (first<0) first=atom;
        else { int s=bt_add(pr,(BTNode){.op=BT_SEQ,.a=last,.b=atom}); last=s; }
        if (first>=0 && last<0) last=atom;
    }
    if (first<0) return bt_add(pr,(BTNode){.op=BT_SEQ}); /* empty */
    return (last<0)? first : last;
}
static int bt_parse_alt(BTProg *pr, const char **pp){
    int left=bt_parse_concat(pr,pp);
    if (**pp=='\\' && (*pp)[1]=='|'){ (*pp)+=2; int right=bt_parse_alt(pr,pp);
        return bt_add(pr,(BTNode){.op=BT_ALT,.a=left,.b=right}); }
    return left;
}

/* Recursive backtracking matcher. Returns the new position after matching the
 * subtree at ni, or -1 if it cannot match. pos is the current offset. Captures
 * are recorded in pr->group[2*g .. 2*g+1]. Greedy with backtracking. */
static int bt_match(BTProg *pr, int ni, int pos){
    if (pr->steps-- <= 0) return -1;   /* backtracking budget exhausted */
    if (ni<0) return -1;
    BTNode *nd=&pr->nodes[ni];
    const unsigned char *buf=pr->buf; size_t n=pr->blen;
    int icase=pr->icase, dotnl=pr->dotnl;
    switch(nd->op){
        case BT_LIT: {
            if (pos>=n) return -1;
            int got=buf[pos], want=nd->c;
            if (icase){ if(cfold(got)!=cfold(want)) return -1; } else if(got!=want) return -1;
            return pos+1;   /* SEQ wrapper continues */
        }
        case BT_DOT: {
            if (pos>=n) return -1;
            if (!dotnl && buf[pos]=='\n') return -1;
            return pos+1;
        }
        case BT_CLS: {
            if (pos>=n) return -1;
            int ch=buf[pos]; int in=((nd->bits[ch>>3]>>(ch&7))&1);
            if (nd->neg) in=!in;
            if (!in) return -1;
            return pos+1;
        }
        case BT_BOL:  return (pos==0)      ? pos        : -1;
        case BT_EOL:  return (pos==(int)n) ? pos        : -1;
        case BT_BREF: {
            if (nd->g>=pr->ngroups) return -1;
            int s=pr->group[2*nd->g], e=pr->group[2*nd->g+1];
            if (s<0||e<s) return -1;
            int len=e-s;
            if (pos+len>n) return -1;
            for(int i=0;i<len;i++){
                int a=buf[s+i], b=buf[pos+i];
                if (icase){ if(cfold(a)!=cfold(b)) return -1; } else if(a!=b) return -1;
            }
            return pos+len;
        }
        case BT_SEQ: {
            int p2=bt_match(pr, nd->a, pos);
            if (p2<0) return -1;
            return bt_match(pr, nd->b, p2);
        }
        case BT_ALT: {
            int r=bt_match(pr, nd->a, pos);
            if (r>=0) return r;
            return bt_match(pr, nd->b, pos);
        }
        case BT_STAR: case BT_PLUS: case BT_QUEST: case BT_REP: {
            int min = (nd->op==BT_PLUS)  ? 1 : (nd->op==BT_QUEST ? 0 : nd->min);
            int max = (nd->op==BT_QUEST) ? 1 : (nd->max<0 ? 1000000 : nd->max);
            /* greedy: try max repetitions down to min. After the repetition we
             * return the new position; the enclosing SEQ node continues. */
            for (int k=max; k>=min; k--){
                int pp=pos, ok=1;
                for(int j=0;j<k;j++){
                    int nx=bt_match(pr, nd->a, pp);
                    if (nx<0){ ok=0; break; }
                    if (nx==pp){ ok=0; break; } /* zero-width child: no progress */
                    pp=nx;
                }
                if (!ok) continue;
                return pp;
            }
            return -1;
        }
        case BT_CAP: {
            int save_s=pr->group[2*nd->g], save_e=pr->group[2*nd->g+1];
            pr->group[2*nd->g]=pos;
            int r=bt_match(pr, nd->a, pos);
            if (r>=0) pr->group[2*nd->g+1]=r;
            else { pr->group[2*nd->g]=save_s; pr->group[2*nd->g+1]=save_e; }
            return r;
        }
        default: return -1;
    }
}

WURegex *wubre_compile_bre(const char *pat, int flags, char *err, size_t errsz){
    BTProg pr={0}; pr.icase=(flags&WUBRE_ICASE)?1:0; pr.dotnl=(flags&WUBRE_DOTNL)?1:0;
    const char *p=pat;
    int root=bt_parse_alt(&pr,&p);
    if (pr.n==0){ if(errsz)snprintf(err,errsz,"empty"); free(pr.nodes); return NULL; }
    WURegex *re=calloc(1,sizeof *re);
    if(!re){ if(errsz)snprintf(err,errsz,"oom"); free(pr.nodes); return NULL; }
    re->flags=flags; re->bt=pr.nodes; re->bt_n=pr.n; re->bt_ngroups=pr.ngroups; re->bt_root=root;
    return re;
}

/* Drive the backtracking engine over a single line buffer. Unanchored: try
 * every start position. Returns true on the first match. */
static bool wubre_search_bre(const WURegex *re, const unsigned char *buf, size_t n){
    BTProg pr={0};
    pr.nodes=re->bt; pr.n=re->bt_n; pr.ngroups=re->bt_ngroups;
    pr.icase=(re->flags&WUBRE_ICASE)?1:0; pr.dotnl=(re->flags&WUBRE_DOTNL)?1:0;
    pr.buf=buf; pr.blen=n;
    pr.steps = 20000000;   /* backtracking budget (DoS guard; shared across starts) */
    pr.group=calloc(2*re->bt_ngroups+2, sizeof(int));
    for(int g=0; g<re->bt_ngroups; g++){ pr.group[2*g]=-1; pr.group[2*g+1]=-1; }
    int root = re->bt_root; /* top-level node from bt_parse_alt */
    bool found=false;
    for (size_t start=0; start<=n && !found; start++){
        for(int g=0; g<re->bt_ngroups; g++){ pr.group[2*g]=-1; pr.group[2*g+1]=-1; }
        int r=bt_match(&pr, root, (int)start);
        if (r >= 0) found=true;
    }
    free(pr.group);
    return found;
}

