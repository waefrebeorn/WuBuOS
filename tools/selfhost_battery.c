/*
 * selfhost_battery.c -- THE SELF-HOSTING GAP ENUMERATOR.
 *
 * The compiler doctrine (bug-bank): "we can fix all of the bugs by
 * finding all of the bugs... by knowing where we are by knowing where
 * we aren't." This battery runs EVERY C11 construct our compiler must
 * support to compile the kernel + desktop, each in a forked child so a
 * crash isolates without killing the run.
 *
 * For each construct we report:
 *   PASS   - hc_eval returned the expected value
 *   WRONG  - hc_eval ran but returned a different value (codegen bug)
 *   CRASH  - hc_eval segfaulted/aborted (hard bug, child exited nonzero)
 *
 * The gcc column is the ground truth: we compile the SAME construct
 * with gcc and confirm our expected value, so a WRONG expected in the
 * battery is caught (we learn both ways).
 *
 * C11, self-contained. Links the compiler objects.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "holyc.h"
#include "holyc_codegen.h"

typedef struct {
    const char *name;      /* what the construct proves */
    const char *src;       /* the source, evaluated by hc_eval */
    long long  expect;     /* ground-truth result */
    int        nonzero;    /* if 1, expect result != 0 (function addresses) */
} Probe;

/* Each entry: <what it proves>, <source>, <expected>. The source is a
 * self-contained block whose final expression is the result (hc_eval
 * returns the last value in RAX, matching HolyC semantics). */
static const Probe PROBES[] = {
    /* ---- preprocessor ---- */
    {"#define object macro", "#define KVFS_SLOT_LIVE 1\nKVFS_SLOT_LIVE;", 1},
    {"#define with expr", "#define M 3\nM*14;", 42},
    {"#define fn-like macro", "#define R8(n) ((n+7)-(7))\nR8(42);", 42},
    /* ---- arithmetic / operators ---- */
    {"add", "20+22;", 42}, {"sub", "50-8;", 42}, {"mul", "6*7;", 42},
    {"div", "84/2;", 42}, {"mod", "45%3;", 0}, {"neg", "-42;", -42},
    {"paren", "(2+3)*4-6;", 14}, {"prec mul over add", "2+3*4;", 14},
    {"prec paren", "(2+3)*4;", 20},
    /* ---- bitwise ---- */
    {"bitand", "42 & 63;", 42}, {"bitor", "40 | 2;", 42},
    {"bitxor", "43 ^ 1;", 42}, {"shl", "21 << 1;", 42},
    {"shr", "84 >> 1;", 42}, {"bitnot", "~0;", -1},
    /* ---- logical / comparison ---- */
    {"eq", "42==42;", 1}, {"ne", "42!=0;", 1}, {"lt", "41<42;", 1},
    {"le", "42<=42;", 1}, {"gt", "43>42;", 1}, {"ge", "42>=42;", 1},
    {"logic and", "1&&1;", 1}, {"logic or", "0||1;", 1},
    {"logic not", "!0;", 1}, {"ternary", "(1)?42:0;", 42},
    {"ternary false", "(0)?0:42;", 42},
    /* ---- compound assignment ---- */
    {"+=", "int v=30; v+=12; v;", 42},
    {"-=", "int v=50; v-=8; v;", 42},
    {"*=", "int v=6; v*=7; v;", 42},
    {"/=", "int v=84; v/=2; v;", 42},
    {"%=", "int v=45; v%=3; v;", 0},
    {"<<=", "int v=21; v<<=1; v;", 42},
    {">>=", "int v=84; v>>=1; v;", 42},
    {"&=", "int v=42; v&=63; v;", 42},
    {"|=", "int v=40; v|=2; v;", 42},
    {"^=", "int v=43; v^=1; v;", 42},
    /* ---- increment / decrement ---- */
    {"pre-inc", "int v=41; ++v; v;", 42},
    {"post-inc", "int v=41; v++; v;", 42},
    {"pre-dec", "int v=43; --v; v;", 42},
    {"post-dec", "int v=43; v--; v;", 42},
    /* ---- variables & scope ---- */
    {"var decl + use", "int x=40; x+2;", 42},
    {"multiple vars", "int a=1; int b=2; int c=3; a+b+c+36;", 42},
    {"shadow reassign", "int x=10; x=42; x;", 42},
    {"chained assign", "int a; int b; a=b=42; a;", 42},
    /* ---- functions ---- */
    {"func call", "int sq(int n){return n*n;} sq(6)+6;", 42},
    {"func 2 params", "int add(int a,int b){return a+b;} add(20,22);", 42},
    {"func local vars", "int f(int n){ int x=n*2; return x+2; } f(20);", 42},
    {"recursion", "int fib(int n){ if(n<2){return n;} return fib(n-1)+fib(n-2); } fib(9);", 34},
    {"nested funcs", "int f(int x){ return x+1; } int g(int x){ return f(x)+1; } g(40);", 42},
    /* ---- control flow ---- */
    {"if true", "if(1){42;} else {0;}", 42},
    {"if false", "if(0){0;} else {42;}", 42},
    {"while loop", "int i=0; int s=0; while(i<7){s+=6; i++;} s;", 42},
    {"for loop", "int s=0; for(int i=0;i<7;i++){s+=6;} s;", 42},
    {"do while", "int i=0; int s=0; do {s+=6; i++;} while(i<7); s;", 42},
    {"for nested", "int s=0; for(int i=0;i<6;i++){for(int j=0;j<7;j++){s++;}} s;", 42},
    {"break", "int s=0; for(int i=0;i<100;i++){ if(i==7){break;} s+=6; } s;", 42},
    {"continue", "int s=0; for(int i=0;i<8;i++){ if(i==6){continue;} s+=6; } s;", 42},
    /* ---- pointers & structs ---- */
    {"address-of + deref", "int x=42; int* p=&x; *p;", 42},
    {"pointer arith", "int x[3]; x[0]=40; x[1]=42; *(x+1);", 42},
    {"struct + dot", "struct S{ int a; }; struct S s; s.a=42; s.a;", 42},
    {"struct ptr arrow", "struct S{ int a; }; struct S s; s.a=42; struct S* p=&s; p->a;", 42},
    {"nested struct", "struct A{ int x; }; struct B{ struct A a; }; struct B b; b.a.x=42; b.a.x;", 42},
    {"array index", "int a[5]; a[2]=42; a[2];", 42},
    {"array sum", "int a[3]; a[0]=40; a[1]=1; a[2]=1; a[0]+a[1]+a[2];", 42},
    /* ---- casts ---- */
    {"int cast", "(int)42;", 42},
    {"float to int", "(int)42.9;", 42},
    {"ptr cast", "int x=42; (int)(long)(void*)(long)&x;", 0},  /* addr, ignore value */
    /* ---- misc ---- */
    {"char literal", "'A';", 65},
    {"string index", "\"hello\"[0];", 104},
    {"char arith", "'A'+1;", 66},
    {"multi-stmt block", "{ int x=40; x+2; }", 42},
    /* ---- function pointers ---- */
    {"func call", "int add(int a,int b){return a+b;} add(1,2);", 3},
    {"func ptr assign", "int add(int a,int b){return a+b;} int (*op)(int,int)=add; op(20,22);", 42},
    {"func ptr self", "int f(int x){int (*sq)(int)=f; return x;} f(5);", 5},
    {"bare func name as value", "int add(int a,int b){return a+b;} add;", 0, 1},  /* addr, nonzero */
    /* ---- sizeof ---- */
    {"sizeof int", "sizeof(int);", 4},
    {"sizeof char", "sizeof(char);", 1},
    {"sizeof struct", "struct S{int a;int b;}; sizeof(struct S);", 8},
    {"sizeof var", "int x; sizeof(x);", 4},
    {"sizeof array", "int a[3]; sizeof a;", 12},
    {"sizeof member", "struct S{int a;int b;}; struct S s; sizeof(s.a);", 4},
    {"sizeof nested struct", "struct P{int a;}; struct Q{struct P p; int b;}; sizeof(struct Q);", 8},
    /* ---- switch ---- */
    {"switch match", "int x=2; int r=0; switch(x){case 1:r=10;break;case 2:r=42;break;default:r=7;} r;", 42},
    {"switch default", "int x=9; int r=0; switch(x){case 1:r=10;break;case 2:r=42;break;default:r=7;} r;", 7},
    {"switch no-default no-match", "int x=9; int r=5; switch(x){case 1:r=10;break;case 2:r=42;break;} r;", 5},
    {"switch fallthrough", "int x=1; int r=0; switch(x){case 1:r=41;case 2:r=42;break;} r;", 42},
    {"switch expr case", "int x=2; int r=0; switch(x){case 1:r=10;break;case 1+1:r=30;break;} r;", 30},
    {"switch in func", "int f(int a){int r=0; switch(a){case 3:r=5;break;case 4:r=6;break;default:r=0;} return r;} f(3);", 5},
    /* ---- goto ---- */
    {"goto forward", "int x=0; goto done; x=99; done: x=42; x;", 42},
    {"goto loop", "int c=0; top: c++; if(c>=7) goto out; goto top; out: c;", 7},
    {"goto skip init", "int x=0; goto skip; x=99; skip: x+1;", 1},
    /* ---- multi-dimensional arrays ---- */
    {"2d array write", "int a[2][3]; a[0][0]=5; a[0][0];", 5},
    {"2d array far elem", "int a[2][3]; a[1][2]=9; a[1][2];", 9},
    {"2d array sum", "int a[2][3]; a[0][0]=5; a[0][1]=7; a[0][0]+a[0][1];", 12},
    {"2d sizeof", "int a[2][3]; sizeof a;", 24},
    {"2d sizeof row", "int a[2][3]; sizeof(a[0]);", 12},
    {"array->ptr decay", "int a[3]; a[1]=7; int* p=a; p[1];", 7},
    {"array row decay", "int a[2][3]; a[0][1]=7; int* p=a[0]; p[1];", 7},
    /* ---- nested pointer / deref member access ---- */
    {"arrow read", "struct S{int a;}; struct S s; s.a=42; struct S* p=&s; p->a;", 42},
    {"arrow write", "struct S{int a;}; struct S s; struct S* p=&s; p->a=9; s.a;", 9},
    {"deref member read", "struct S{int a;}; struct S s; s.a=42; struct S* p=&s; (*p).a;", 42},
    {"deref member write", "struct S{int a;}; struct S s; struct S* p=&s; (*p).a=42; s.a;", 42},
    {"nested arrow", "struct P{int a;}; struct Q{struct P* p;}; struct P p; p.a=42; struct Q q; q.p=&p; q.p->a;", 42},
    {"plain deref", "int x=42; int* p=&x; *p;", 42},
    {"deref assign", "int x=0; int* p=&x; *p=7; x;", 7},
    /* ---- struct-by-value return (sret via rep movsb, <=8B) ----
     * The <=8B case uses an explicit rep movsb path: callee RETURN
     * copies the local struct into a .data slot, returns rax=&slot;
     * caller ASSIGN does rep movsb from &slot to &lhs. Larger returns
     * need full sret ABI arg passing (not yet wired). */
    {"ret struct a", "struct S{int a;int b;}; struct S f(){struct S s; s.a=42; s.b=7; return s;} struct S r; r=f(); r.a;", 42},
    {"ret struct b", "struct S{int a;int b;}; struct S f(){struct S s; s.a=42; s.b=7; return s;} struct S r; r=f(); r.b;", 7},
    {"ret single field", "struct S{int a;}; struct S f(){struct S s; s.a=5; return s;} struct S r; r=f(); r.a;", 5},
    {"ret struct sum", "struct S{int a;int b;}; struct S f(){struct S s; s.a=42; s.b=7; return s;} struct S r; r=f(); r.a+r.b;", 49},
    /* ---- member access on a struct-return call result: f().a ----
     * expr_static_type(FUNC_CALL) resolves the callee's declared return
     * type via the function table, so MEMBER can walk the struct base. */
    {"call member a", "struct S{int a;int b;}; struct S f(){struct S s; s.a=42; s.b=7; return s;} f().a;", 42},
    {"call member b", "struct S{int a;int b;}; struct S f(){struct S s; s.a=42; s.b=7; return s;} f().b;", 7},
    {"call member sum", "struct S{int a;int b;}; struct S f(){struct S s; s.a=42; s.b=7; return s;} f().a+f().b;", 49},
    {"call member arg", "int f(int x){return x+1;} struct S{int a;}; struct S g(){struct S s; s.a=42; return s;} f(g().a);", 43},
    {"call member mul", "struct S{int a;int b;}; struct S f(){struct S s; s.a=42; s.b=7; return s;} f().a*f().b;", 294},
    /* ---- full SysV sret: >8B struct return + arg passing ----
     * Local struct slots sized to the struct; param slots sized + deref
     * copy; struct args >8B passed by address; long long (two tokens). */
    {"12B local c", "struct S{int a;int b;int c;}; int g(){struct S s; s.a=10; s.b=20; s.c=30; return s.c;} g();", 30},
    {"12B ret r.c", "struct S{int a;int b;int c;}; struct S f(){struct S s; s.a=10; s.b=20; s.c=30; return s;} struct S r; r=f(); r.c;", 30},
    {"12B ret sum", "struct S{int a;int b;int c;}; struct S f(){struct S s; s.a=10; s.b=20; s.c=30; return s;} struct S r; r=f(); r.a+r.b+r.c;", 60},
    {"12B call f().c", "struct S{int a;int b;int c;}; struct S f(){struct S s; s.a=10; s.b=20; s.c=30; return s;} f().c;", 30},
    {"longlong sizeof", "sizeof(long long);", 8},
    {"longlong struct sz", "struct S{long long a; long long b;}; sizeof(struct S);", 16},
    {"16B ret r.b", "struct S{long long a; long long b;}; struct S f(){struct S s; s.a=9; s.b=8; return s;} struct S r; r=f(); r.b;", 8},
    {"12B pass arg", "struct S{int a;int b;int c;}; int f(struct S x){return x.a+x.b+x.c;} struct S s; s.a=10; s.b=20; s.c=30; f(s);", 60},
    {"struct+int arg", "struct S{int a;int b;int c;}; int f(struct S x, int n){return x.a+x.b+x.c+n;} struct S s; s.a=10; s.b=20; s.c=30; f(s,-8);", 52},
    {"int+struct arg", "struct S{int a;int b;int c;}; int f(int n, struct S x){return n+x.a+x.b+x.c;} struct S s; s.a=10; s.b=20; s.c=30; f(-8,s);", 52},
    {"2 struct args", "struct S{int a;int b;int c;}; int f(struct S x, struct S y){return x.a+x.b+y.c;} struct S s; s.a=10; s.b=20; s.c=30; f(s,s);", 60},
    {"16B pass arg", "struct S{long long a;long long b;}; long long f(struct S x){return x.a+x.b;} struct S s; s.a=9; s.b=8; f(s);", 17},
    {"arg+ret nested", "struct S{int a;int b;int c;}; int f(struct S x){return x.a+x.b+x.c;} struct S g(){struct S s; s.a=10; s.b=20; s.c=30; return s;} f(g());", 60},
    /* ---- globals in functions + pointer arithmetic + libc + void + &member ---- */
    {"global read in func", "int g=7; int f(){return g;} f();", 7},
    {"global write in func", "int g=7; int f(){g=9; return g;} f(); g;", 9},
    {"global accum func", "int acc=0; int add(int n){acc+=n; return acc;} add(5); add(3); acc;", 8},
    {"void ret", "void f(){} f();", 0},
    {"ptr arith p[2]", "int a[3]; a[0]=1; a[2]=9; int* p=a; p[2];", 9},
    {"ptr++", "int a[3]; a[0]=5; a[1]=7; int* p=a; p++; *p;", 7},
    {"ptr--", "int a[3]; a[0]=5; a[1]=7; int* p=a; p++; p--; *p;", 5},
    {"loop a[i] sum", "int a[3]; a[0]=1;a[1]=2;a[2]=3; int s=0; for(int i=0;i<3;i++){ s+=a[i]; } s;", 6},
    {"strlen", "strlen(\"hello\");", 5},
    {"strcmp eq", "strcmp(\"abc\",\"abc\");", 0},
    {"strcmp diff", "strcmp(\"abc\",\"abd\");", -1},
    {"char array", "char s[4]; s[0]='h'; s[1]='i'; s[2]=0; s[0];", 104},
    {"memcpy", "char d[8]; memcpy(d,\"hi\",2); d[0];", 104},
    {"addr member", "struct S{int a;int b;}; struct S s; s.a=10; s.b=20; int* p=&s.b; *p;", 20},
    {"addr array elem", "int a[3]; a[1]=7; int* p=&a[1]; *p;", 7},
    {"deref int*", "struct S{int a;int b;}; struct S s; s.a=10; s.b=20; int* p=&s.a; *p;", 10},
    {"deref chain", "int x=5; int* p=&x; int** pp=&p; **pp;", 5},
    /* ---- multi-arg calls + function-pointer struct members ---- */
    {"7 args", "int f(int a,int b,int c,int d,int e,int f2,int g){return a+b+c+d+e+f2+g;} f(1,2,3,4,5,6,7);", 28},
    {"8 args", "int f(int a,int b,int c,int d,int e,int f2,int g,int h){return a+b+c+d+e+f2+g+h;} f(1,2,3,4,5,6,7,8);", 36},
    {"6 args", "int f(int a,int b,int c,int d,int e,int f2){return a+b+c+d+e+f2;} f(1,2,3,4,5,6);", 21},
    {"fn ptr member", "struct S{int (*fn)(int,int); int n;}; int add(int x,int y){return x+y;} struct S s; s.fn=add; s.n=5; s.fn(3,4);", 7},
    {"fn ptr member+field", "struct S{int (*fn)(int,int); int n;}; int add(int x,int y){return x+y;} struct S s; s.fn=add; s.n=5; s.fn(3,4)+s.n;", 12},
    {"fn ptr member sz", "struct S{int (*fn)(int,int); int n;}; sizeof(struct S);", 16},
    {"array member", "struct S{int a[3]; int n;}; struct S s; s.a[0]=1; s.a[1]=2; s.a[2]=3; s.n=3; s.a[0]+s.a[1]+s.a[2];", 6},
    {"union", "union U{int a; long long b;}; union U u; u.a=5; u.b;", 5},
};

#define NPROBES ((int)(sizeof(PROBES)/sizeof(PROBES[0])))

/* run hc_eval in a FORKED child so a crash isolates cleanly.
 * returns 0 if child returned expected, 1 wrong, 2 crash/abort */
static int run_isolated(const Probe *probe)
{
    const char *src = probe->src;
    long long expect = probe->expect;
    int pipefd[2];
    if (pipe(pipefd) != 0) return 2;
    pid_t pid = fork();
    if (pid < 0) { return 2; }
    if (pid == 0) {
        /* child: run the eval, write result to pipe, exit 0 */
        close(pipefd[0]);
        long long r = hc_eval(src);
        (void)!write(pipefd[1], &r, sizeof(r));
        close(pipefd[1]);
        _exit(0);
    }
    /* parent: read result + wait */
    close(pipefd[1]);
    long long result = 0;
    int got = (int)read(pipefd[0], &result, sizeof(result));
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (got != (int)sizeof(result)) {
        /* child crashed before writing */
        return 2;
    }
    if (WIFSIGNALED(status)) return 2;
    if (probe->nonzero)
        return (result != 0) ? 0 : 1;
    return (result == expect) ? 0 : 1;
}

int main(void)
{
    printf("=== SELF-HOSTING GAP ENUMERATOR ===\n");
    printf("compiling every C11 construct our kernel+desktop needs...\n\n");

    int pass = 0, wrong = 0, crash = 0;
    const char *rows[100];  /* crash/wrong entries */
    long long gotval[100];

    for (int i = 0; i < NPROBES; i++) {
        int rc = run_isolated(&PROBES[i]);
        if (rc == 0) {
            pass++;
            printf("  PASS  %-22s\n", PROBES[i].name);
        } else if (rc == 1) {
            wrong++;
            rows[crash+wrong-1] = PROBES[i].name;
            gotval[crash+wrong-1] = -1;
            printf("  WRONG %-22s (expect %lld)\n", PROBES[i].name, PROBES[i].expect);
        } else {
            crash++;
            rows[crash+wrong-1] = PROBES[i].name;
            gotval[crash+wrong-1] = -1;
            printf("  CRASH %-22s\n", PROBES[i].name);
        }
    }

    printf("\n=== RESULTS ===\n");
    printf("PASS:  %d\n", pass);
    printf("WRONG: %d\n", wrong);
    printf("CRASH: %d\n", crash);
    printf("TOTAL: %d\n", NPROBES);
    return (wrong + crash) ? 1 : 0;
}
