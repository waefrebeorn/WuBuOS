/*
 * wubre_bre.c - BRE backtracking engine (backreferences \1..\9).
 * ---------------------------------------------------------------------------
 * A Thompson NFA cannot express backreferences, so these compile to a recursive
 * backtracking matcher over an AST. This is the one regex feature ripgrep's
 * engine structurally lacks - WuBuGrep has it, byte-identical to GNU grep.
 * License: WaefreBeorn Umbrella License v3.0
 * ---------------------------------------------------------------------------
 */
#include "wubre_internal.h"
#include <stdio.h>

/* BRE-AST node helpers (used by the class parser before their definition) */
static int  bt_add(BTProg *p, BTNode nd);
static int  bt_lit(BTProg *p,int c);
static int  bt_cls(BTProg *p,unsigned char *bits,int neg);
static int  bt_parse_alt(BTProg *pr, const char **pp);

static int bt_add(BTProg *p, BTNode nd){
    if (p->n>=p->cap){ p->cap = p->cap? p->cap*2 : 64; p->nodes=realloc(p->nodes,p->cap*sizeof*p->nodes); }
    nd.next = -1;
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
                if (is_known_posix_class(nm)){ unsigned char *tmp=calloc(32,1); set_class_posix(tmp,nm); for(int i=0;i<32;i++) bits[i]|=tmp[i]; free(tmp); }
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

static int bt_parse_atom(BTProg *pr, const char **pp){
    const char *p=*pp; char c=*p;
    if (c=='\\' && p[1]){
        char n=p[1];
        if (n>='1' && n<='9'){ p+=2; *pp=p; return bt_add(pr,(BTNode){.op=BT_BREF,.g=(n-'0')-1}); }
        if (n=='('){ p+=2; int g=pr->ngroups++; int child=bt_parse_alt(pr,&p);
            if (*p=='\\' && p[1]==')') p+=2;
            else if (*p==')') p++;
            *pp=p; int cap=bt_add(pr,(BTNode){.op=BT_CAP,.g=g,.a=child}); return cap; }
        if (n=='|' || n=='{' || n==')'){ *pp=p; return -1; }
        int ec=(unsigned char)n; p+=2; *pp=p; return bt_lit(pr,ec);
    }
    if (c==')' || (c=='\\' && p[1]==')')) return -1;
    if (c=='^'){ p++; *pp=p; return bt_add(pr,(BTNode){.op=BT_BOL}); }
    if (c=='$'){ p++; *pp=p; return bt_add(pr,(BTNode){.op=BT_EOL}); }
    if (c=='.'){ p++; *pp=p; return bt_add(pr,(BTNode){.op=BT_DOT}); }
    if (c=='['){ int cls=bt_parse_class(pr,&p); *pp=p; return cls; }
    p++; *pp=p; return bt_lit(pr,(unsigned char)c);
}
static int bt_parse_quant(BTProg *pr, const char **pp, int atom){
    if (atom<0) return atom;
    const char *p=*pp;
    if (*p=='*'){
        if (atom>=0 && pr->nodes[atom].op==BT_LIT && pr->nodes[atom].c=='*'){
            int lit=bt_lit(pr,'*'); p++; *pp=p; return lit;
        }
        p++; *pp=p; return bt_add(pr,(BTNode){.op=BT_STAR,.a=atom,.min=0,.max=-1});
    }
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
    int head=-1, prev=-1;
    for(;;){
        if (!**pp || **pp==')' || (**pp=='\\' && (*pp)[1]=='|')) break;
        int atom=bt_parse_atom(pr,pp);
        if (atom<0) break;
        int q=bt_parse_quant(pr,pp,atom);
        if (head<0){ head=q; prev=q; }
        else { pr->nodes[prev].next = q; prev=q; }
    }
    if (head<0) return -1;
    return head;
}
static int bt_parse_alt(BTProg *pr, const char **pp){
    int left=bt_parse_concat(pr,pp);
    if (**pp=='\\' && (*pp)[1]=='|'){ (*pp)+=2; int right=bt_parse_alt(pr,pp);
        int alt=bt_add(pr,(BTNode){.op=BT_ALT,.a=left,.b=right});
        return alt;
    }
    return left;
}

static int bt_match(BTProg *pr, int ni, int pos){
    if (pr->steps-- <= 0) return -1;
    if (ni<0) return -1;
    BTNode *nd=&pr->nodes[ni];
    const unsigned char *buf=pr->buf; size_t n=pr->blen;
    int icase=pr->icase, dotnl=pr->dotnl;
    int r=-1;
    switch(nd->op){
        case BT_LIT: {
            if ((size_t)pos>=n) return -1;
            int got=buf[pos], want=nd->c;
            if (icase){ if(cfold(got)!=cfold(want)) return -1; } else if(got!=want) return -1;
            r = pos+1; break;
        }
        case BT_DOT: {
            if ((size_t)pos>=n) return -1;
            if (!dotnl && buf[pos]=='\n') return -1;
            r = pos+1; break;
        }
        case BT_CLS: {
            if ((size_t)pos>=n) return -1;
            int ch=buf[pos]; int in=((nd->bits[ch>>3]>>(ch&7))&1);
            if (nd->neg) in=!in;
            if (!in) return -1;
            r = pos+1; break;
        }
        case BT_BOL:  r = (pos==0)      ? pos        : -1; break;
        case BT_EOL:  r = (pos==(int)n) ? pos        : -1; break;
        case BT_BREF: {
            if (nd->g>=pr->ngroups) return -1;
            int s=pr->group[2*nd->g], e=pr->group[2*nd->g+1];
            if (s<0||e<=s) return -1;
            int len=e-s;
            if ((size_t)(pos+len) > n) return -1;
            for(int i=0;i<len;i++){
                int a=buf[s+i], b=buf[pos+i];
                if (icase){ if(cfold(a)!=cfold(b)) return -1; } else if(a!=b) return -1;
            }
            r = pos+len; break;
        }
        case BT_ALT: {
            int ra=bt_match(pr, nd->a, pos);
            if (ra>=0){ r=ra; break; }
            r = bt_match(pr, nd->b, pos); break;
        }
        case BT_CAP: {
            int save_s=pr->group[2*nd->g], save_e=pr->group[2*nd->g+1];
            pr->group[2*nd->g]=pos;
            int rr=bt_match(pr, nd->a, pos);
            if (rr>=0 && rr>pos){
                pr->group[2*nd->g+1]=rr;
                r = rr; break;
            }
            pr->group[2*nd->g]=save_s; pr->group[2*nd->g+1]=save_e;
            r = -1; break;
        }
        case BT_STAR: case BT_PLUS: case BT_QUEST: case BT_REP: {
            int min = (nd->op==BT_PLUS)  ? 1 : (nd->op==BT_QUEST ? 0 : nd->min);
            int max = (nd->op==BT_QUEST) ? 1 : (nd->max<0 ? 1000000 : nd->max);
            int room = (int)n - pos + 1;
            if (max > room) max = room;
            for (int k=max; k>=min; k--){
                int pp=pos, ok=1;
                for(int j=0;j<k;j++){
                    int nx=bt_match(pr, nd->a, pp);
                    if (nx<0 || nx==pp){ ok=0; break; }
                    pp=nx;
                }
                if (!ok) continue;
                if (nd->next<0) return pp;
                int cont=bt_match(pr, nd->next, pp);
                if (cont>=0) return cont;
            }
            return -1;
        }
        default: return -1;
    }
    if (r<0) return -1;
    if (nd->next>=0) return bt_match(pr, nd->next, r);
    return r;
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

bool wubre_search_bre(const WURegex *re, const unsigned char *buf, size_t n){
    BTProg pr={0};
    pr.nodes=re->bt; pr.n=re->bt_n; pr.ngroups=re->bt_ngroups;
    pr.icase=(re->flags&WUBRE_ICASE)?1:0; pr.dotnl=(re->flags&WUBRE_DOTNL)?1:0;
    pr.buf=buf; pr.blen=n;
    pr.steps = 20000000;
    pr.group=calloc(2*re->bt_ngroups+2, sizeof(int));
    for(int g=0; g<re->bt_ngroups; g++){ pr.group[2*g]=-1; pr.group[2*g+1]=-1; }
    int root = re->bt_root;
    bool found=false;
    for (size_t start=0; start<=n && !found; start++){
        for(int g=0; g<re->bt_ngroups; g++){ pr.group[2*g]=-1; pr.group[2*g+1]=-1; }
        int r=bt_match(&pr, root, (int)start);
        if (r >= 0) found=true;
    }
    free(pr.group);
    return found;
}
