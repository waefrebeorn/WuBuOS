/*
 * wubre_compile.c - WuBu regex compiler: BRE/ERE parser -> Thompson NFA.
 * ---------------------------------------------------------------------------
 * Self-contained module (C11, no third-party). See wubre_internal.h for the
 * shared struct layouts and the module map.
 * License: WaefreBeorn Umbrella License v3.0
 * ---------------------------------------------------------------------------
 */
#include "wubre_internal.h"
#include <stdio.h>

/* internal forward declarations (parser mutual recursion, TU-local) */
static Frag   parse_alt(P *ps);
static Frag   parse_concat(P *ps);
static Frag   parse_quant(P *ps);
static Frag   parse_atom(P *ps);
static Frag   parse_class(P *ps);
static int    parse_escaped_byte(P *ps);
static const char *validate_regex(const char *p, int bre, int icase);
int    is_known_posix_class(const char *name);
void   set_class_posix(unsigned char *bits, const char *name);

/* ----- NFA construction helpers ----- */

static State *add_state(WURegex *re, Stype t){
    if (re->nst >= MAX_STATES) return NULL;
    State *s = &re->st[re->nst++];
    memset(s,0,sizeof *s);
    s->out = s->out1 = -1;
    s->type = t;
    return s;
}

static void patch_to(Ctx *c, Dangle *d, int target){
    while (d){
        Dangle *n = d->next;
        if (d->field==0) c->re->st[d->s].out = target;
        else             c->re->st[d->s].out1 = target;
        Dangle **pp = &c->all;
        while (*pp){ if (*pp==d){ *pp = d->regnext; break; } pp = &(*pp)->regnext; }
        free(d);
        d = n;
    }
}
static Dangle *dangle_one(Ctx *c, int s, int field){
    Dangle *d = malloc(sizeof *d);
    d->s=s; d->field=field; d->next=NULL;
    d->regnext = c->all; c->all = d;
    return d;
}
static Frag empty_frag(Ctx *c){
    WURegex *re=c->re;
    State *sp=add_state(re,SPLIT);
    int si=(int)(sp-re->st);
    Dangle *d0=dangle_one(c,si,0);
    Dangle *d1=dangle_one(c,si,1);
    d0->next=d1;
    return (Frag){ .start=si, .out=d0, .src=NULL, .src_end=NULL, .empty=1 };
}

/* ----- class helpers ----- */

int is_known_posix_class(const char *name){
    static const char *tbl[] = {
        "alpha","lower","upper","digit","xdigit","alnum","blank","space",
        "punct","print","graph","cntrl","word","ascii", NULL };
    for (int i=0; tbl[i]; i++) if (strcmp(name,tbl[i])==0) return 1;
    return 0;
}
void set_class_posix(unsigned char *bits, const char *name){
    memset(bits,0,32);
    int add=0;
    if      (strcmp(name,"alpha")==0||strcmp(name,"alnum")==0) add=1;
    else if (strcmp(name,"lower")==0){ for(int c='a';c<='z';c++) bits[c>>3]|=(1u<<(c&7)); return; }
    else if (strcmp(name,"upper")==0){ for(int c='A';c<='Z';c++) bits[c>>3]|=(1u<<(c&7)); return; }
    else if (strcmp(name,"digit")==0||strcmp(name,"xdigit")==0){ for(int c='0';c<='9';c++) bits[c>>3]|=(1u<<(c&7)); if(strcmp(name,"digit")==0) return; }
    else if (strcmp(name,"xdigit")==0){ for(int c='a';c<='f';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='A';c<='F';c++) bits[c>>3]|=(1u<<(c&7)); return; }
    else if (strcmp(name,"alnum")==0){ return; } /* already added alpha+digit */
    else if (strcmp(name,"space")==0){ for(int c=0;c<256;c++) if(isspace(c)) bits[c>>3]|=(1u<<(c&7)); return; }
    else if (strcmp(name,"blank")==0){ bits[' '>>3]|=(1u<<(' '&7)); bits['\t'>>3]|=(1u<<('\t'&7)); return; }
    else if (strcmp(name,"punct")==0){ for(int c=0;c<256;c++) if(ispunct(c)) bits[c>>3]|=(1u<<(c&7)); return; }
    else if (strcmp(name,"print")==0){ for(int c=32;c<127;c++) bits[c>>3]|=(1u<<(c&7)); return; }
    else if (strcmp(name,"graph")==0){ for(int c=33;c<127;c++) bits[c>>3]|=(1u<<(c&7)); return; }
    else if (strcmp(name,"cntrl")==0){ for(int c=0;c<32;c++) bits[c>>3]|=(1u<<(c&7)); bits[127>>3]|=(1u<<(127&7)); return; }
    else if (strcmp(name,"word")==0){ for(int c='a';c<='z';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='A';c<='Z';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='0';c<='9';c++) bits[c>>3]|=(1u<<(c&7)); bits['_'>>3]|=(1u<<('_'&7)); return; }
    else if (strcmp(name,"ascii")==0){ for(int c=0;c<128;c++) bits[c>>3]|=(1u<<(c&7)); return; }
    if (add){ for(int c='a';c<='z';c++) bits[c>>3]|=(1u<<(c&7)); for(int c='A';c<='Z';c++) bits[c>>3]|=(1u<<(c&7)); }
}

/* ----- escaped byte (\\n \\t \\r \\0 \\xHH \\c) ----- */
static int parse_escaped_byte(P *ps){
    const char *p = ps->p; /* points at '\\' */
    if (p+1>=ps->end) return -1;
    char e = p[1];
    int val=-1;
    switch(e){
        case 'n': val='\n'; break;
        case 't': val='\t'; break;
        case 'r': val='\r'; break;
        case 'f': val='\f'; break;
        case 'v': val='\v'; break;
        case '0': val=0; break;
        case 'a': val='\a'; break;
        case 'b': val='\b'; break;
        case 'e': val=27; break;
        case 'x': {
            if (p+3<ps->end && isxdigit((unsigned char)p[2]) && isxdigit((unsigned char)p[3])){
                val = (isdigit((unsigned char)p[2])?p[2]-'0': (tolower((unsigned char)p[2])-'a'+10))*16
                    + (isdigit((unsigned char)p[3])?p[3]-'0': (tolower((unsigned char)p[3])-'a'+10));
                ps->p += 4; return val;
            }
            val='x'; ps->p+=2; return val;
        }
        default:
            if (e>='0'&&e<='7'){ /* octal */
                int o=0,k=0; const char *q=p+1;
                while(q<ps->end && *q>='0'&&*q<='7' && k<3){ o=o*8+(*q-'0'); q++; k++; }
                val=o; ps->p = q; return val;
            }
            val=(unsigned char)e; ps->p+=2; return val;
    }
    ps->p += 2;
    return val;
}

/* ----- parser ----- */
static Frag parse_alt(P *ps);  /* forward */

static Frag parse_class(P *ps){
    const char *s0 = ps->p; ps->p++;  /* consume [ */
    int neg=0;
    if (ps->p<ps->end && *ps->p=='^'){ neg=1; ps->p++; }
    unsigned char *bits = calloc(32,1);
    if (ps->p<ps->end && *ps->p==']'){ /* a ] right after [ or [^ is a literal ] */
        int x=(unsigned char)']'; bits[x>>3]|=(1u<<(x&7)); ps->p++;
    }
    while (ps->p<ps->end && *ps->p!=']'){
        if (ps->p[0]=='\\' && ps->p+1<ps->end){
            int x=parse_escaped_byte(ps);
            if (x<0) x=0;
            if (x>255) x=255;
            bits[x>>3]|=(1u<<(x&7)); continue;
        }
        /* collating/equivalence element [[.x.]] [[=x=]] : single char only */
        if (ps->p[0]=='[' && (ps->p[1]=='.'||ps->p[1]=='=')){
            char close = (ps->p[1]=='.') ? '.' : '=';
            const char *q = ps->p+2;   /* element start, after [[. or [[= */
            const char *ce = NULL;
            while (q+1 < ps->end){
                if (q[0]==close && q[1]==']'){ ce=q; break; }
                q++;
            }
            if (ce){
                int elen=(int)(ce-(ps->p+2));
                const char *el = ps->p+2;
                if (elen==1){
                    int x=(unsigned char)el[0];
                    bits[x>>3]|=(1u<<(x&7));
                    ps->p = ce+2;        /* past .] or =] */
                    continue;
                }
                /* multi-char / unknown element: GNU -> error */
                if (ps->err) snprintf(ps->err, ps->errsz, "ERR_COLLATE");
                free(bits);
                ps->p = ps->end;
                return empty_frag(ps->cx);
            }
            /* malformed: no close -> treat '[' as literal and continue */
            int x=(unsigned char)'['; bits[x>>3]|=(1u<<(x&7)); ps->p++; continue;
        }
        /* POSIX class [:name:] */
        if (ps->p[0]=='[' && ps->p[1]==':'){
            const char *r=ps->p+2; while (r<ps->end && *r && *r!=':' && *r!=']') r++;
            if (r<ps->end && r[0]==':' && r+1<ps->end && r[1]==']'){
                char nm[32]; int nl=0; const char *s=ps->p+2;
                for(; s<r && nl<31; s++){ nm[nl++]=(char)*s; } nm[nl]=0;
                set_class_posix(bits, nm);
                ps->p = r+2; continue;
            }
            /* malformed [: -> treat '[' as literal */
            int x=(unsigned char)'['; bits[x>>3]|=(1u<<(x&7)); ps->p++; continue;
        }
        /* range a-b */
        if (ps->p+2<ps->end && ps->p[1]=='-' && ps->p[2]!=']' && ps->p[2]!='\\'
            && !(ps->p[0]=='[')){
            int lo=(unsigned char)ps->p[0], hi=(unsigned char)ps->p[2];
            if (lo<=hi){ for(int c=lo;c<=hi;c++) bits[c>>3]|=(1u<<(c&7)); }
            ps->p+=3; continue;
        }
        {
            int x=(unsigned char)ps->p[0];
            bits[x>>3]|=(1u<<(x&7)); ps->p++;
        }
    }
    if (ps->p<ps->end && *ps->p==']') ps->p++;
    /* GNU grep semantics: bracket expressions never match '\n' (grep works
     * line-by-line, so a negated class can never span a line). Our
     * whole-buffer engine CAN see '\n', so exclude it from negated classes
     * explicitly or patterns like a[^]b]c match across lines. */
    if (neg){
        int x=(unsigned char)'\n';
        bits[x>>3]|=(1u<<(x&7));
    }
    State *s = add_state(ps->cx->re, CHR);
    s->cl=2; s->bits=bits; s->neg=neg?1:0;
    Frag f = { .start=(int)(s-ps->cx->re->st),
               .out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0),
               .src=s0, .src_end=ps->p, .empty=0 };
    return f;
}

static Frag parse_atom(P *ps){ /* identical to current engine */
    const char *s0 = ps->p;
    if (ps->p>=ps->end){ return empty_frag(ps->cx); }
    char ch=*ps->p;
    if (ch=='*' || ch=='+' || ch=='?'){ ps->p++; return empty_frag(ps->cx); }
    if (ch=='('){
        ps->p++;
        ps->gdepth++;
        Frag f=parse_alt(ps);
        ps->gdepth--;
        if (ps->p<ps->end && *ps->p==')') ps->p++;
        f.src=s0; f.src_end=ps->p;
        return f;
    }
    if (ch=='{'){
        const char *q=ps->p+1; int ndig=0,commas=0,bad=0,closed=0;
        while(*q && *q!='}'){ if(*q>='0'&&*q<='9')ndig++; else if(*q==',')commas++; else bad=1; q++; }
        if (*q=='}') closed=1;
        if (closed && !bad && ndig>0 && commas<=1){ return empty_frag(ps->cx); }
        ps->p++; State *s=add_state(ps->cx->re,CHR); s->cl=0; s->c='{';
        Frag f={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)}; f.src=s0; f.src_end=ps->p; return f;
    }
    if (ch=='['){
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
            if (f.empty){ continue; }
            WURegex *re=ps->cx->re;
            if (kind==1 || kind==3){
                State *sp=add_state(re,SPLIT);
                sp->out=f.start;
                patch_to(ps->cx, f.out, (int)(sp-re->st));
                f.start=(int)(sp-re->st);
                f.out=dangle_one(ps->cx,(int)(sp-re->st),1);
            } else {
                State *sp=add_state(re,SPLIT);
                sp->out1=f.start;
                patch_to(ps->cx, f.out, (int)(sp-re->st));
                f.start=f.start;
                f.out=dangle_one(ps->cx,(int)(sp-re->st),0);
            }
            f.src_end = ps->p;
            continue;
        }
        if (q=='{' && !(ps->cx->re->flags & WUBRE_BRE)){
            const char *p=ps->p+1; int n=0,m=-1; int ok=0;
            if (p<ps->end && *p>='0' && *p<='9'){ while (p<ps->end && *p>='0' && *p<='9'){ n=n*10+(*p-'0'); p++; } ok=1; }
            if (p<ps->end && *p==','){
                p++; ok=1;
                if (p<ps->end && *p>='0' && *p<='9'){ m=0; while(p<ps->end && *p>='0' && *p<='9'){ m=m*10+(*p-'0'); p++; } }
                else if (m==-1) m=-1;
            } else if (ok){ m=n; }
            if (p<ps->end && *p=='}'){ p++; }
            else ok=0;
            if (!ok) break;
            const char *after = p;
            int total = (m<0) ? n : m;
            if (total > 1000) total = 1000;
            int has_quant = (f.src && f.src_end > f.src && (f.src_end[-1]=='*'||f.src_end[-1]=='+'||f.src_end[-1]=='?'));
            Frag acc = FRAG_NULL; int have=0;
            if (has_quant){ acc = f; have=1; }
            else {
                for (int i=0;i<total;i++){
                    Frag c;
                    if (f.src && !f.empty){ ps->p = f.src; c = parse_atom(ps); }
                    else { c = empty_frag(ps->cx); }
                    if (i>=n){
                        /* Optional copy i (the {n,m} "up to m" part). A SPLIT
                         * either TAKES the copy (out -> body) or SKIPS it
                         * (out1). After taking the body, OR skipping it, we
                         * continue to the next decision point, so BOTH the body
                         * out and the skip branch must chain FORWARD to the same
                         * next-start/join. We link the skip dangling to the body
                         * out so patch_to(acc.out, next) patches both at once. */
                        State *sp=add_state(ps->cx->re,SPLIT);
                        int sp_i=(int)(sp-ps->cx->re->st);
                        sp->out=c.start;                 /* take -> copy body */
                        Dangle *skip_d=dangle_one(ps->cx, sp_i, 1); /* out1 = skip */
                        skip_d->next = c.out;            /* skip + body-out chain fwd */
                        c.start=sp_i;
                        c.out=skip_d;                    /* patched fwd to next/join */
                        if (!have){ acc=c; have=1; }
                        else { patch_to(ps->cx, acc.out, c.start); acc.out=c.out; }
                    } else {
                        if (!have){ acc=c; have=1; }
                        else { patch_to(ps->cx, acc.out, c.start); acc.out=c.out; }
                    }
                }
            }
            ps->p = after;
            if (!have){ acc = empty_frag(ps->cx); }
            f=acc;
            continue;
        }
        break;
    }
    return f;
}

static Frag parse_concat(P *ps){
    Frag f = FRAG_NULL; int have=0;
    while (ps->p<ps->end){
        char ch=*ps->p;
        if (ch=='|') break;
        if (ch==')'){
            /* GNU grep: an unmatched ')' is a LITERAL ')'. Only stop when it
             * closes a group opened by parse_atom's '(' branch. */
            if (ps->gdepth > 0) break;
            ps->p++;
            State *s=add_state(ps->cx->re,CHR); s->cl=0; s->c=')';
            Frag a={.start=(int)(s-ps->cx->re->st),.out=dangle_one(ps->cx,(int)(s-ps->cx->re->st),0)};
            a.src=ps->p-1; a.src_end=ps->p;
            if (!have){ f=a; have=1; }
            else { patch_to(ps->cx, f.out, a.start); f.out=a.out; }
            continue;
        }
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
        State *join=add_state(ps->cx->re,SPLIT);
        patch_to(ps->cx, f.out, (int)(join-ps->cx->re->st));
        patch_to(ps->cx, g.out, (int)(join-ps->cx->re->st));
        f.start=(int)(sp-ps->cx->re->st);
        f.out=dangle_one(ps->cx,(int)(join-ps->cx->re->st),0);
    }
    return f;
}

/* ----- validator ----- */
static const char *validate_regex(const char *p, int bre, int icase){
    (void)icase;
    int depth=0;
    while (*p){
        char c=*p;
        switch(c){
            case '\\':
                if (!p[1]) return "EESCAPE";
                if (bre && (p[1]=='(' || p[1]==')' || p[1]=='{' || p[1]=='+' || p[1]=='?' || p[1]=='|')){
                    p+=2; continue;
                }
                p+=2; continue;
            case '(':
                if (bre) { p++; continue; }
                depth++; p++; continue;
            case ')':
                if (bre) { p++; continue; }
                if (depth==0){ p++; continue; }
                depth--; p++; continue;
            case '[': {
                p++;
                if (*p=='^') p++;
                if (*p==']') p++;
                int last_range=0;
                while (*p && *p!=']'){
                    if (*p=='\\'){ if(!p[1]) return "EESCAPE"; p+=2; last_range=0; continue; }
                    if (p[0]=='[' && p[1]==':'){
                        const char *r=p+2; while(*r && *r!=':' && *r!=']') r++;
                        if (r[0]==':' && r[1]==']'){
                            char nm[32]; int nl=0;
                            for(const char*s=p+2;s<r&&nl<31;s++) nm[nl++]=(char)*s;
                            nm[nl]=0;
                            if (nm[0]=='\0' || !is_known_posix_class(nm)) return "ECTYPE";
                            p=r+2; last_range=0; continue;
                        }
                        return "ECTYPE";
                    }
                    if (p[0]=='[' && (p[1]=='.'||p[1]=='=')){
                        const char *q=p+2;
                        while (*q && *q!=']') q++;
                        if (*q && q[1]==']'){
                            if (q - (p+2) == 2 && q[-1]==(p[1]=='.'?'.':'=')){
                                p = q+1; last_range=0; continue;
                            }
                            return "ERR_COLLATE";
                        }
                        while(*p && *p!=']') p++;
                        if(*p) p++;
                        last_range=0; continue;
                    }
                    if (last_range && p[1]=='-' && p[2] && p[2]!=']') return "ERANGE";
                    if (*p!='\\' && p[1]=='-' && p[2] && p[2]!=']'){
                        if (p[2]=='-') return "ERANGE";
                        int lo=(unsigned char)*p, hi=(unsigned char)p[2];
                        if (lo>hi) return "ERANGE";
                        p++; p++;
                        last_range=1; continue;
                    }
                    p++; last_range=0;
                }
                if (!*p) return "EBRACK";
                p++; continue;
            }
            case '*': case '+': case '?':
                if (!bre){
                    p++; continue;
                } else { p++; continue; }
            case '{':
                if (bre) { p++; continue; }
                {
                    const char *q=p+1;
                    int ndig=0, commas=0, bad=0;
                    while (*q && *q!='}'){
                        if (*q>='0' && *q<='9') ndig++;
                        else if (*q==',') commas++;
                        else bad=1;
                        q++;
                    }
                    if (!*q){ p++; continue; }
                    if (bad){ p++; continue; }
                    if (ndig==0 && commas==0) return "BADBR";
                    if (commas>1) return "EBRACE";
                    const char *qq=p+1; int n=0,m=0;
                    while(*qq>='0'&&*qq<='9'){ n=n*10+(*qq-'0'); qq++; }
                    if (*qq==','){
                        qq++;
                        if (*qq>='0'&&*qq<='9'){ m=0; while(*qq>='0'&&*qq<='9'){ m=m*10+(*qq-'0'); qq++; } }
                        else m=-1;
                    } else m=n;
                    if (m>=0 && n>m) return "BADBR";
                    if (n>32767 || m>32767) return "ESIZE";
                    p=q+1; continue;
                }
            case '^': case '$':
                p++; continue;
            case '|':
                if (bre){ p++; continue; }
                p++; continue;
            case '.':
                p++; continue;
            default:
                p++; continue;
        }
    }
    if (depth>0) return "EPAREN";
    return NULL;
}

/* ----- top-level compile ----- */
WURegex *wubre_compile(const char *pat, int flags, char *err, size_t errsz){
    if (err && errsz) err[0]=0;
    if (flags & WUBRE_BRE){
        bool has_bref=false;
        for (const char *q=pat; *q; q++){
            if (*q=='\\' && q[1]>='1' && q[1]<='9'){ has_bref=true; break; }
        }
        if (has_bref) return wubre_compile_bre(pat, flags, err, errsz);
    }
    char *trans = NULL;
    const char *use = pat;
    if (flags & WUBRE_BRE){
        size_t L = strlen(pat);
        trans = malloc(L*2+1);
        char *o = trans;
        int prev_atom = 0;
        int bdepth = 0;
        for (size_t i=0;i<L;i++){
            char c = pat[i];
            if (c=='\\' && i+1<L){
                char n = pat[i+1];
                if (n=='('||n==')'||n=='|'||n=='+'||n=='?'||n=='{'||n=='}'){
                    if (n=='{'){
                        if (pat[i+2]==',' && pat[i+3]=='\\' && pat[i+4]=='}'){
                            *o++='\\'; *o++='\\'; *o++='{'; *o++=','; *o++='\\'; *o++='\\'; *o++='}';
                            i += 4; prev_atom=1; continue;
                        }
                        const char *q = pat + i + 2;
                        int nd=0,commas=0,bad=0; const char *r=q;
                        while (*r && *r!='\\'){ if(*r>='0'&&*r<='9')nd++; else if(*r==',')commas++; else bad=1; r++; }
                        if (*r=='\\' && r[1]=='}' && !bad && commas<=1 && (nd>0 || commas>0)){
                            if (prev_atom){
                                *o++='{'; while(q<r){ *o++=*q; q++; } *o++='}';
                                i = (int)((r+1) - pat);
                                prev_atom = 1; continue;
                            } else {
                                *o++='\\'; *o++='\\'; *o++='{'; while(q<r){ *o++=*q; q++; } *o++='\\'; *o++='\\'; *o++='}';
                                i = (int)((r+1) - pat);
                                prev_atom = 1; continue;
                            }
                        }
                        free(trans); return NULL;
                    }
                    *o++ = n; i++; prev_atom = (n==')');
                    if (n=='(') bdepth++; else if (n==')'){ if (bdepth==0){ free(trans); return NULL; } bdepth--; }
                    continue;
                }
                *o++ = '\\'; *o++ = n; i++; prev_atom = 1; continue;
            }
            if (c=='('||c==')'||c=='|'||c=='+'||c=='?'||c=='{'||c=='}'){
                *o++ = '\\'; *o++ = c; prev_atom = (c==')'); continue;
            }
            if (c=='*'){
                if (!prev_atom){ *o++='\\'; *o++='*'; }
                else *o++='*';
                prev_atom = 1;
                continue;
            }
            if (c=='^'){
                int at_anchor = (i==0) || (i>=2 && pat[i-1]=='(' && pat[i-2]=='\\');
                if (!at_anchor){ *o++ = '\\'; }
                *o++ = '^'; prev_atom = 0; continue;
            }
            if (c=='$'){
                int at_anchor = (i+1==L) || (i+2<L && pat[i+1]=='\\' && pat[i+2]==')');
                if (!at_anchor){ *o++ = '\\'; }
                *o++ = '$'; prev_atom = 0; continue;
            }
            *o++ = c; prev_atom = 1;
        }
        *o = 0;
        use = trans;
        if (bdepth>0 || trans==NULL){ free(trans); return NULL; }
        flags &= ~WUBRE_BRE;
    }
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
    re->prefilter=NULL; re->preflen=0;
    re->pref2a=NULL; re->pref2a_n=0; re->pref2b=NULL; re->pref2b_n=0; re->win_only=0;
    if (!(flags & WUBRE_ICASE)) {
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
            const char *META = ".^$*+?()[]{}|\\";
            while (*p) {
                if (*p=='\\') { p++; if (*p) p++; continue; }
                if (*p=='[') {
                    const char *q=p+1;
                    if (*q=='^') q++;
                    if (*q==']') q++;
                    int depth=1;
                    while (*q && depth>0){
                        if (*q=='['){
                            if (q[1]=='.'||q[1]=='='){
                                q+=2; while(*q && !((q[0]=='.'||q[0]=='=')&&q[1]==']')) q++;
                                if (*q) q+=2; else break;
                                continue;
                            }
                            if (q[1]==':'){
                                q+=2; while(*q && !(q[0]==':'&&q[1]==']')) q++;
                                if (*q) q+=2; else break;
                                continue;
                            }
                            depth++;
                        } else if (*q==']'){ depth--; }
                        q++;
                    }
                    p=q; continue;
                }
                if (*p=='{') {
                    const char *q=p+1;
                    while (*q && *q!='}') q++;
                    if (*q=='}') q++;
                    p=q; continue;
                }
                if (strchr(META, *p)!=NULL) { p++; continue; }
                const char *OPT="?*+({";
                const char *run=p; int rl=0;
                while (*p) {
                    if (*p=='\\') { p++; if (*p) p++; break; }
                    if (*p=='[') break;
                    if (strchr(META, *p)!=NULL) break;
                    const char *nx = p+1;
                    if (*nx && strchr(OPT, *nx)!=NULL) break;
                    p++; rl++;
                }
                if (rl==0) p++;
                if (rl>bestlen){ bestlen=rl; best=run; }
            }
            if (bestlen>=2) {
                char *buf=malloc((size_t)bestlen);
                if (buf){ memcpy(buf,best,(size_t)bestlen); re->prefilter=(unsigned char*)buf; re->preflen=bestlen; }
            }
            /* Two-literal window: detect  RUN1 .* RUN2  (a literal, then an
             * unanchored .* star, then another literal) and record both runs
             * as a bounded window prefilter. This lets the SIMD scanner reject
             * lines that cannot contain both literals in order (Hyperscan
             * "literal acceleration" model) before the NFA runs. */
            {
                const char *META2 = ".^$*+?()[]{}|\\";
                /* find first literal run */
                const char *p1=use; const char *r1=NULL; int r1n=0;
                while (*p1){
                    if (*p1=='\\'){ p1++; if(*p1) p1++; continue; }
                    if (*p1=='['){ const char *q=p1+1; if(*q=='^')q++; if(*q==']')q++; int d=1; while(*q&&d){ if(*q=='['){ if(q[1]=='.'||q[1]=='='){q+=2;while(*q&&!((q[0]=='.'||q[0]=='=')&&q[1]==']'))q++; if(*q)q+=2; else break; continue;} if(q[1]==':'){q+=2;while(*q&&!(q[0]==':'&&q[1]==']'))q++; if(*q)q+=2; else break; continue;} d++; } else if(*q==']')d--; q++; } p1=q; continue; }
                    if (*p1=='{'){ const char *q=p1+1; while(*q&&*q!='}')q++; if(*q)q++; p1=q; continue; }
                    if (strchr(META2,*p1)){ p1++; continue; }
                    const char *OPT2="?*+({"; const char *run=p1; int rl=0;
                    while(*p1){ if(*p1=='\\'){p1++;if(*p1)p1++;break;} if(*p1=='[')break; if(strchr(META2,*p1))break; const char *nx=p1+1; if(*nx&&strchr(OPT2,*nx))break; p1++; rl++; }
                    if (rl==0){ p1++; continue; }
                    if (rl>=1){ r1=run; r1n=rl; }
                    break;
                }
                if (r1 && r1n>=1){
                    const char *mid = r1 + r1n;
                    /* require literal .* right after RUN1 */
                    if (mid[0]=='.' && mid[1]=='*'){
                        const char *p2 = mid+2;
                        /* skip optional quantifier after the star already consumed;
                         * find the next literal run */
                        const char *r2=NULL; int r2n=0;
                        while (*p2){
                            if (*p2=='\\'){ p2++; if(*p2) p2++; continue; }
                            if (*p2=='['){ const char *q=p2+1; if(*q=='^')q++; if(*q==']')q++; int d=1; while(*q&&d){ if(*q=='['){ if(q[1]=='.'||q[1]=='='){q+=2;while(*q&&!((q[0]=='.'||q[0]=='=')&&q[1]==']'))q++; if(*q)q+=2; else break; continue;} if(q[1]==':'){q+=2;while(*q&&!(q[0]==':'&&q[1]==']'))q++; if(*q)q+=2; else break; continue;} d++; } else if(*q==']')d--; q++; } p2=q; continue; }
                            if (*p2=='{'){ const char *q=p2+1; while(*q&&*q!='}')q++; if(*q)q++; p2=q; continue; }
                            if (strchr(META2,*p2)){ p2++; continue; }
                            const char *OPT2b="?*+({"; const char *run=p2; int rl=0;
                            while(*p2){ if(*p2=='\\'){p2++;if(*p2)p2++;break;} if(*p2=='[')break; if(strchr(META2,*p2))break; const char *nx=p2+1; if(*nx&&strchr(OPT2b,*nx))break; p2++; rl++; }
                            if (rl==0){ p2++; continue; }
                            if (rl>=1){ r2=run; r2n=rl; }
                            break;
                        }
                        if (r2 && r2n>=1){
                            char *a=malloc((size_t)r1n), *b=malloc((size_t)r2n);
                            if (a&&b){
                                memcpy(a,r1,(size_t)r1n); memcpy(b,r2,(size_t)r2n);
                                re->pref2a=(const unsigned char*)a; re->pref2a_n=r1n;
                                re->pref2b=(const unsigned char*)b; re->pref2b_n=r2n;
                                /* exact  LIT .* LIT  (no leading/trailing atoms,
                                 * no other structure) -> NFA-free window match */
                                if (r1==use && r2+r2n==(const char*)use+strlen(use))
                                    re->win_only=1;
                            } else { free(a); free(b); }
                        }
                    }
                    }
                    }
                    }
    }
    Ctx cx; cx.re=re; cx.all=NULL;
    P ps; ps.p=use; ps.end=use+strlen(use); ps.cx=&cx; ps.err=err; ps.errsz=errsz; ps.gdepth=0;
    if (ps.p>=ps.end){ State *m=add_state(re,MATCH); re->start=(int)(m-re->st); free(trans); return re; }
    Frag f=parse_alt(&ps);
    if (err && err[0]){ free(trans); return NULL; }
    State *m=add_state(re,MATCH);
    patch_to(&cx, f.out, (int)(m-re->st));
    int start=f.start;
    {
        int anchored = (ps.p>use && use[0]=='^') || (use[0]=='^');
        if (!anchored){
            State *sp=add_state(re,SPLIT);
            sp->out=start;
            State *skip=add_state(re,CHR);
            skip->cl=1;
            skip->out=(int)(sp-re->st);
            sp->out1=(int)(skip-re->st);
            start=(int)(sp-re->st);
        }
    }
    re->start=start;
    re->unanchored = (use[0]!='^') ? 1 : 0;
    /* Pure-literal fast path: if the pattern contains no metacharacters at
     * all (not even ^/$), the entire pattern is a fixed needle. Route the
     * whole-buffer scan through the SIMD literal scanner instead of the
     * byte-wise Pike VM. Case-insensitive builds clear this in wubugrep. */
    {
        /* pure-literal SIMD scan is case-sensitive only; -i needs the NFA.
         * strpbrk==NULL means use contains NO metacharacter at all. */
        re->lit_only = (!re->win_only && !re->bt_n && !(flags & WUBRE_ICASE)
                        && strpbrk(use, ".^$*+?()[]{}|\\") == NULL) ? 1 : 0;
        /* re->lit must OWN a copy: for BRE, 'use' aliases the malloc'd 'trans'
         * buffer which is freed below, so pointing into it would dangle. Use an
         * explicit malloc+memcpy (C11) rather than strdup (POSIX, needs feature
         * macros and otherwise triggers an implicit-declaration UB). */
        { size_t L = strlen(use);
          unsigned char *cp = malloc(L ? L : 1);
          if (cp) { if (L) memcpy(cp, use, L); re->lit = cp; } }
        re->lit_n = (int)strlen(use);
    }
    while (cx.all){ Dangle *n=cx.all->regnext; free(cx.all); cx.all=n; }
    /* Build the literal-set prefilter from the ERE-translated pattern BEFORE
     * freeing 'trans': for BRE, 'use' aliases 'trans', so it must still be
     * valid while wubre_litpref_build parses it. */
    wubre_litpref_build(re, use, flags);
    free(trans);
    /* Eager DFA build (shared read-only across the parallel scan). Skipped for
     * BRE: BRE dispatches to wubre_search_bre before the DFA path, so a DFA
     * would be built and never used. */
    re->dfa_cache = (flags & WUBRE_BRE) ? NULL : wubre_dfa_compile(re);
    return re;
}
