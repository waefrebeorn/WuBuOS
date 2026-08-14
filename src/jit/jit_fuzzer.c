/*
 * jit_fuzzer.c -- Exhaustive edge-case sweep of the Mini-C JIT compiler.
 * Tests every code path, boundary condition, and interaction between
 * subsystems to find bugs and missed optimizations.
 */
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

static int pass, fail, total;
#define CHECK(c,m) do{total++;if(c)pass++;else{fail++;printf("FAIL: %s\n",m);}}while(0)

static int64_t run1(JITContext *ctx, const char *src, int64_t a) {
    JITFunc fn; JITResult r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
    if (r != 0) return INT64_MIN;
    int64_t v = jit_call1(&fn, a);
    return v;
}
static int64_t run2(JITContext *ctx, const char *src, int64_t a, int64_t b) {
    JITFunc fn; JITResult r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
    if (r != 0) return INT64_MIN;
    return jit_call2(&fn, a, b);
}
static int run0(JITContext *ctx, const char *src) {
    JITFunc fn; JITResult r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
    if (r != 0) return INT64_MIN;
    return jit_call0(&fn);
}

int main(void) {
    JITContext *ctx = jit_init();
    int64_t v;

    printf("=== EXPRESSION EDGE CASES ===\n");

    /* Constant folding opportunities */
    v = run1(ctx, "long f(long x){ return x+0; }", 42);
    CHECK(v==42, "x+0 == x (identity add)");

    v = run1(ctx, "long f(long x){ return x*1; }", 42);
    CHECK(v==42, "x*1 == x (identity mul)");

    v = run1(ctx, "long f(long x){ return x*0; }", 42);
    CHECK(v==0, "x*0 == 0 (zero mul)");

    v = run1(ctx, "long f(long x){ return x-0; }", 42);
    CHECK(v==42, "x-0 == x (identity sub)");

    v = run1(ctx, "long f(long x){ return 0-x; }", 5);
    CHECK(v==-5, "0-x == -x (negation)");

    /* Division edge cases */
    v = run1(ctx, "long f(long x){ return x/1; }", 42);
    CHECK(v==42, "x/1 == x");

    v = run1(ctx, "long f(long x){ return x/-1; }", 42);
    CHECK(v==-42, "x/-1 == -x");

    /* Operator precedence */
    v = run1(ctx, "long f(long x){ return x+2*3; }", 5);
    CHECK(v==11, "x+2*3 == x+6 (precedence)");

    v = run1(ctx, "long f(long x){ return (x+2)*3; }", 5);
    CHECK(v==21, "(x+2)*3 == 21 (parens)");

    /* Nested unary */
    v = run1(ctx, "long f(long x){ return -(-x); }", 5);
    CHECK(v==5, "-(-x) == x");

    /* Multiple operations */
    v = run1(ctx, "long f(long x){ return x+x+x; }", 7);
    CHECK(v==21, "x+x+x == 3x");

    printf("\n=== INTEGER OVERFLOW BEHAVIOR ===\n");

    v = run1(ctx, "long f(long x){ return x+1; }", INT64_MAX);
    CHECK(v==INT64_MIN, "INT64_MAX+1 wraps to INT64_MIN");

    v = run1(ctx, "long f(long x){ return x-1; }", INT64_MIN);
    CHECK(v==INT64_MAX, "INT64_MIN-1 wraps to INT64_MAX");

    v = run1(ctx, "long f(long x){ return x*2; }", INT64_MAX);
    CHECK(v==-2, "INT64_MAX*2 wraps to -2");

    printf("\n=== VARIABLE LIFETIME ===\n");

    /* Many locals */
    v = run1(ctx, "long f(long x){ long a=x+1; long b=a+1; long c=b+1; long d=c+1; return d; }", 10);
    CHECK(v==14, "chain of locals: 10+1+1+1+1=14");

    /* Unused variable */
    v = run1(ctx, "long f(long x){ long unused=999; return x+1; }", 5);
    CHECK(v==6, "unused local doesn't affect result");

    printf("\n=== CONTROL FLOW EDGE CASES ===\n");

    /* Empty-ish bodies */
    v = run1(ctx, "long f(long x){ if(x>0){ x=x+1; } return x; }", 5);
    CHECK(v==6, "if with single stmt body");

    v = run1(ctx, "long f(long x){ if(x>0){ x=x+1; } return x; }", -5);
    CHECK(v==-5, "if false, no change");

    /* Deeply nested if */
    v = run1(ctx, "long f(long x){ if(x>0){ if(x>1){ if(x>2){ return 3; } } } return 0; }", 5);
    CHECK(v==3, "deeply nested if (all true)");

    v = run1(ctx, "long f(long x){ if(x>0){ if(x>1){ if(x>2){ return 3; } } } return 0; }", 1);
    CHECK(v==0, "deeply nested if (one false)");

    /* While loop edge cases */
    v = run1(ctx, "long f(long n){ while(n>0){ n=n-1; } return n; }", 0);
    CHECK(v==0, "while with false condition (no iterations)");

    v = run1(ctx, "long f(long n){ while(n>0){ n=n-1; } return n; }", 1);
    CHECK(v==0, "while with 1 iteration");

    v = run1(ctx, "long f(long n){ long s=0; while(n>0){ s=s+n; n=n-1; } return s; }", 100);
    CHECK(v==5050, "sum(1..100)=5050");

    printf("\n=== MULTIPLE ARGUMENTS ===\n");

    v = run2(ctx, "long f(long a, long b){ return a+b; }", 3, 4);
    CHECK(v==7, "two args: a+b");

    v = run2(ctx, "long f(long a, long b){ return a*b; }", 3, 4);
    CHECK(v==12, "two args: a*b");

    v = run2(ctx, "long f(long a, long b){ long c=a+1; long d=b+2; return c+d; }", 10, 20);
    CHECK(v==33, "two args with locals");

    printf("\n=== COMPARISON OPERATORS ===\n");

    v = run1(ctx, "long f(long x){ return x==0; }", 0);
    CHECK(v==1, "x==0 true");

    v = run1(ctx, "long f(long x){ return x==0; }", 5);
    CHECK(v==0, "x==0 false");

    v = run1(ctx, "long f(long x){ return x!=0; }", 5);
    CHECK(v==1, "x!=0 true");

    v = run1(ctx, "long f(long x){ return x<10; }", 5);
    CHECK(v==1, "x<10 true");

    v = run1(ctx, "long f(long x){ return x<=10; }", 10);
    CHECK(v==1, "x<=10 true (equal)");

    v = run1(ctx, "long f(long x){ return x>10; }", 15);
    CHECK(v==1, "x>10 true");

    v = run1(ctx, "long f(long x){ return x>=10; }", 10);
    CHECK(v==1, "x>=10 true (equal)");

    printf("\n=== UNARY OPERATORS ===\n");

    v = run1(ctx, "long f(long x){ return -x; }", 5);
    CHECK(v==-5, "unary neg");

    v = run1(ctx, "long f(long x){ return -x; }", 0);
    CHECK(v==0, "neg of zero");

    v = run1(ctx, "long f(long x){ return -x; }", INT64_MIN);
    CHECK(v==INT64_MIN, "neg of INT64_MIN (undefined but should not crash)");

    printf("\n=== STRUCT EDGE CASES ===\n");

    {
        unsigned char buf[32]; memset(buf,0,32);
        *(int64_t*)&buf[0] = 100; buf[8] = 5; buf[9] = 7;  /* reordered: x@0,c1@8,c2@9 */
        JITFunc fn; JITResult r;
        r = jit_compile(ctx, "struct S{ long x; U8 c1; U8 c2; } long f(struct S* p){ return p->x+p->c1+p->c2; }", JIT_LANG_C, "f", &fn);
        CHECK(r==0, "struct compile");
        if(r==0){ int64_t v=jit_call1(&fn,(int64_t)(uintptr_t)buf); CHECK(v==112, "struct: 100+5+7=112"); }
    }

    printf("\n=== DIVISION BY CONSTANTS ===\n");

    /* These test the magic-multiply path (requires XRA) */
    v = run1(ctx, "long f(long x){ return x/3; }", 21);
    CHECK(v==7, "21/3=7");

    v = run1(ctx, "long f(long x){ return x/7; }", 49);
    CHECK(v==7, "49/7=7");

    v = run1(ctx, "long f(long x){ return x/10; }", 100);
    CHECK(v==10, "100/10=10");

    v = run1(ctx, "long f(long x){ return x/3; }", -21);
    CHECK(v==-7, "-21/3=-7 (negative dividend)");

    printf("\n=== MULTIPLICATION BY CONSTANTS ===\n");

    v = run1(ctx, "long f(long x){ return x*3; }", 7);
    CHECK(v==21, "7*3=21");

    v = run1(ctx, "long f(long x){ return x*5; }", 7);
    CHECK(v==35, "7*5=35");

    v = run1(ctx, "long f(long x){ return x*9; }", 7);
    CHECK(v==63, "7*9=63");

    v = run1(ctx, "long f(long x){ return x*10; }", 7);
    CHECK(v==70, "7*10=70 (3-op imul)");

    printf("\n=== STRESS: MANY ITERATIONS ===\n");

    v = run1(ctx, "long f(long n){ long s=0; while(n>0){ s=s+1; n=n-1; } return s; }", 10000);
    CHECK(v==10000, "count to 10000");

    printf("\n=== CONSTANT FOLDING (non-XRA + XRA) ===\n");

    /* These test the constant folding we just added */
    v = run1(ctx, "long f(long x){ return x*0; }", 42);
    CHECK(v==0, "x*0 == 0 (folded)");

    v = run1(ctx, "long f(long x){ return x*1; }", 42);
    CHECK(v==42, "x*1 == x (folded)");

    v = run1(ctx, "long f(long x){ return x+0; }", 42);
    CHECK(v==42, "x+0 == x (folded)");

    v = run1(ctx, "long f(long x){ return x-0; }", 42);
    CHECK(v==42, "x-0 == x (folded)");

    v = run1(ctx, "long f(long x){ return x/1; }", 42);
    CHECK(v==42, "x/1 == x (folded)");

    /* Combined: x*0+5 == 5 */
    v = run1(ctx, "long f(long x){ return x*0+5; }", 99);
    CHECK(v==5, "x*0+5 == 5");

    /* Combined: x+0-0 == x */
    v = run1(ctx, "long f(long x){ return x+0-0; }", 77);
    CHECK(v==77, "x+0-0 == x");

    printf("\n=== SUMMARY ===\n");
    printf("=== jit_fuzzer: %d/%d passed, %d failed ===\n", pass, total, fail);
    jit_free(ctx);
    return fail ? 1 : 0;
}
