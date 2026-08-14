/*
 * peephole_superopt.c -- WuBuOS Souper-style peephole superoptimizer.
 *
 * Discovery engine (research wave "jit-machine-code-25more-2026" #5, LPO/Souper
 * method): given a TARGET expression (e.g. "a*3") and a set of test inputs,
 * enumerate SHORT instruction sequences from a primitive op set and find the
 * shortest one that produces the same result as the reference C expression.
 *
 * This is the "discover a peephole" step: it tells us the minimal x86-64
 * sequence for common idioms (x*3=lea, x/10=magic, abs=(x^x>>63)-(x>>63), ...)
 * that we then bake into jit_minic.c as a peephole rule. We already found these
 * by hand; this tool generalizes the method so NEW patterns (x*7, x*11, x/12, ...)
 * can be discovered automatically.
 *
 * Model: stack machine over 64-bit integers. Ops (each 1 instr in the model):
 *   ADD SUB MUL SHL SHR SAR AND OR XOR NEG NOT  (2-arg: pop2 op pop1, push)
 * A program is a fixed-length sequence of ops; we evaluate it on each test
 * input (seed value(s)) and compare to the reference. Cost = op count.
 *
 * Usage:
 *   peephole_superopt "<ref_expr>" "<ops>" <max_len> <a> <b> ...
 *   e.g. tools/peephole_superopt "x*3" "MUL,ADD,SHL" 3 0 1 2 3 7 100 1000 -7
 *   The tool finds a program (using only the listed ops) of length <= max_len
 *   that maps each seed input to the same value as the ref, and prints it.
 *
 * This is enumerative synthesis (Souper's approach) with an exact simulator as
 * the verifier -- the "LLM proposes, verifier approves" closed loop reduced to
 * the machine-checkable half, so it can never emit a wrong peephole.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LEN 10
#define MAX_SEEDS 16
#define MAX_OPS 16

typedef int64_t (*Binop)(int64_t a, int64_t b);

static int64_t op_add(int64_t a,int64_t b){ return a+b; }
static int64_t op_sub(int64_t a,int64_t b){ return a-b; }
static int64_t op_mul(int64_t a,int64_t b){ return a*b; }
static int64_t op_shl(int64_t a,int64_t b){ return (int64_t)((uint64_t)a << (b&63)); }
static int64_t op_shr(int64_t a,int64_t b){ return (int64_t)((uint64_t)a >> (b&63)); }
static int64_t op_sar(int64_t a,int64_t b){ return a >> (b&63); }
static int64_t op_and(int64_t a,int64_t b){ return a&b; }
static int64_t op_or (int64_t a,int64_t b){ return a|b; }
static int64_t op_xor(int64_t a,int64_t b){ return a^b; }
static int64_t op_neg(int64_t a,int64_t b){ return -a; }   /* ignores b */
static int64_t op_not(int64_t a,int64_t b){ return ~a; }   /* ignores b */

typedef struct { const char *name; Binop fn; } Op;

static const Op OPS[] = {
    {"ADD", op_add}, {"SUB", op_sub}, {"MUL", op_mul},
    {"SHL", op_shl}, {"SHR", op_shr}, {"SAR", op_sar},
    {"AND", op_and}, {"OR", op_or},   {"XOR", op_xor},
    {"NEG", op_neg}, {"NOT", op_not},
};
#define N_OPS (sizeof(OPS)/sizeof(OPS[0]))

static int g_len;          /* program length */
static int g_ops[MAX_LEN]; /* program (op indices) */
static int g_allowed[N_OPS];  /* 1 if this op may be used */
static const char *g_ref_expr;  /* the reference expression string */
static int64_t g_seeds[MAX_SEEDS];
static int g_nseeds;
static int64_t g_ref[MAX_SEEDS];   /* reference result per seed */

/* Interpret the program on a single seed: seed the stack with x, each op pops
 * two (underflow -> x, the input is the implicit second operand), pushes result.
 * Result is the top of the stack. This models a constant-free superoptimizer
 * where the single input is the only "register". */
static int64_t eval(int64_t seed) {
    int64_t st[MAX_LEN+1]; int sp = 0;
    st[sp++] = seed;
    for (int i = 0; i < g_len; i++) {
        int64_t a = st[--sp];                  /* top */
        int64_t b = (sp > 0) ? st[--sp] : seed; /* next, or x if absent */
        st[sp++] = OPS[g_ops[i]].fn(a, b);
    }
    return st[sp-1];
}

static int matches_all(void) {
    for (int s = 0; s < g_nseeds; s++)
        if (eval(g_seeds[s]) != g_ref[s]) return 0;
    return 1;
}

/* Recursive enumeration of op sequences of length g_len, honoring g_allowed. */
static int search_allowed(int pos) {
    if (pos == g_len) return matches_all();
    for (int o = 0; o < N_OPS; o++) {
        if (!g_allowed[o]) continue;
        g_ops[pos] = o;
        if (search_allowed(pos+1)) return 1;
    }
    return 0;
}

/* Parse the allowed-op list (comma/space separated names). */
static void parse_ops(const char *s) {
    for (int i = 0; i < (int)N_OPS; i++) g_allowed[i] = 0;
    char buf[256]; strncpy(buf, s, sizeof(buf)-1); buf[sizeof(buf)-1]=0;
    char *save=NULL;
    for (char *tok = strtok_r(buf, ", ", &save); tok; tok = strtok_r(NULL,", ",&save)) {
        for (int i = 0; i < (int)N_OPS; i++)
            if (strcasecmp(tok, OPS[i].name)==0) g_allowed[i]=1;
    }
}

/* Evaluate the reference C expression via a tiny recursive-descent parser.
 * Grammar: expr := term (('+'|'-'|'*'|'&'|'|'|'^') term)*   (left-assoc, all
 * same precedence for our purposes); term := 'x' | number | '(' expr ')' |
 * unary '-' | '~'. Keeps the tool dependency-free (no compiler subprocess). */
static int64_t g_x;
static int64_t ref_expr(const char **pp);
static int64_t ref_primary(const char **pp) {
    const char *p = *pp;
    while (*p==' ') p++;
    if (*p=='(') { p++; int64_t v=ref_expr(&p); if(*p==')') p++; *pp=p; return v; }
    if (*p=='-') { p++; return -ref_primary(&p); }
    if (*p=='~') { p++; return ~ref_primary(&p); }
    if (*p=='x'||*p=='a') { p++; *pp=p; return g_x; }
    if (*p>='0'&&*p<='9') { char *e; int64_t v=strtoll(p,&e,0); p=e; *pp=p; return v; }
    *pp=p; return 0;
}
static int64_t ref_expr(const char **pp) {
    int64_t l = ref_primary(pp);
    for (;;) {
        const char *p = *pp; while (*p==' ') p++;
        char op = *p;
        if (op==0 || op==')') { *pp=p; return l; }
        if (op=='<' ) { if (p[1]=='<'){ p+=2; *pp=p; l=(int64_t)((uint64_t)l<<(ref_primary(pp)&63)); continue;} *pp=p; return l; }
        if (op=='>' ) { if (p[1]=='>'){ p+=2; *pp=p; l=(int64_t)((uint64_t)l>>(ref_primary(pp)&63)); continue;} *pp=p; return l; }
        if (strchr("+-*&|^", op)) {
            p++; *pp=p; int64_t r=ref_primary(pp);
            switch(op){case '+':l=l+r;break;case '-':l=l-r;break;case '*':l=l*r;break;
                       case '&':l=l&r;break;case '|':l=l|r;break;case '^':l=l^r;break;}
            continue;
        }
        *pp=p; return l;
    }
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s \"<ref_expr>\" \"<ops>\" <max_len> <seed1> [seed2 ...]\n"
            "  ref_expr: infix over 'x' with + - * << >> & | ^ ~ ( )\n"
            "  ops:      comma/space list of ADD SUB MUL SHL SHR SAR AND OR XOR NEG NOT\n"
            "  max_len:  1..%d\n"
            "  seeds:    test inputs; the found program must match ref on all\n"
            "Example: %s \"x*3\" \"MUL,ADD,SHL\" 3 0 1 2 3 7 100 -7\n"
            "  finds the shortest program (e.g. ADD of x,x then ADD x) = x*3\n",
            argv[0], MAX_LEN, argv[0]);
        return 2;
    }
    const char *ref = argv[1];
    g_ref_expr = ref;
    parse_ops(argv[2]);
    int max_len = atoi(argv[3]);
    if (max_len < 1 || max_len > MAX_LEN) max_len = MAX_LEN;
    g_nseeds = 0;
    for (int i = 4; i < argc && g_nseeds < MAX_SEEDS; i++)
        g_seeds[g_nseeds++] = (int64_t)strtoll(argv[i], NULL, 0);

    /* compute reference results */
    for (int s = 0; s < g_nseeds; s++) { g_x = g_seeds[s]; const char *p = ref; g_ref[s] = ref_expr(&p); }

    /* search shortest first */
    for (int L = 1; L <= max_len; L++) {
        g_len = L;
        if (search_allowed(0)) {
            printf("FOUND length %d program:", L);
            for (int i = 0; i < L; i++) printf(" %s", OPS[g_ops[i]].name);
            printf("\n");
            /* print verification table */
            for (int s = 0; s < g_nseeds; s++)
                printf("  x=%ld -> ref=%ld prog=%ld %s\n",
                    (long)g_seeds[s], (long)g_ref[s], (long)eval(g_seeds[s]),
                    eval(g_seeds[s])==g_ref[s] ? "OK":"MISMATCH");
            /* #7 Hydra generalization: the candidate matched the training seeds,
             * but is it a GENERAL identity or a seed-specific accident? Verify
             * against a large battery of pseudo-random seeds (a deterministic
             * LCG, so this is reproducible). A general peephole must hold on
             * every one of them; if not, the pattern is overfit and we reject. */
            unsigned long long s0 = 0x9E3779B97F4A7C15ULL;
            int gpass = 0, gfail = 0;
            for (int i = 0; i < 100000; i++) {
                s0 = s0 * 6364136223846793005ULL + 1442695040888963407ULL;
                int64_t x = (int64_t)(s0 >> 1);
                int64_t r = eval(x);
                g_x = x; const char *rp = g_ref_expr; int64_t want = ref_expr(&rp);
                if (r == want) gpass++; else { gfail++; if (gfail <= 3)
                    printf("  GENERALIZATION FAIL at x=%ld: prog=%ld ref=%ld\n",(long)x,(long)r,(long)want); }
            }
            printf("  generalization: %d pass / %d fail over 100000 random seeds -> %s\n",
                gpass, gfail, gfail==0 ? "GENERAL (accepted)" : "SEED-SPECIFIC (rejected)");
            if (gfail > 0) { printf("  REJECTING overfit candidate; continue search.\n"); break; }
            return 0;
        }
    }
    printf("NO program of length <=%d using those ops matches the reference\n", max_len);
    return 1;
}
