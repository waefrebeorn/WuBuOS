/*
 * jit_deep_opt_test.c -- Deep optimization verification:
 * 1. Stack frame sizing (no longer fixed 256)
 * 2. Two-pass loop compilation (body analyzed before emission)
 * 3. Loop analysis engine integration (IV detection, trip count, invariants)
 * 4. #14 layout swap (single-return then-body)
 * 5. Combined A+B+C pipeline with structs + loops + conditionals
 */
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int pass, fail;
#define CHECK(c,m) do{if(c)pass++;else{fail++;printf("FAIL: %s\n",m);}}while(0)

int main(void) {
    /* Test 1: Simple function correctness after stack frame change */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        r = jit_compile(ctx, "long f(long x){ return x+1; }", JIT_LANG_C, "f", &fn);
        CHECK(r==0,"T1 compile");
        if(r==0){ int64_t v=jit_call1(&fn,41); CHECK(v==42,"T1: 41+1=42"); }
        jit_free(ctx);
    }

    /* Test 2: While loop with IV (two-pass compilation) */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        r = jit_compile(ctx,
            "long f(long n){"
            "  long i=0; long s=0;"
            "  while(i<n){ s=s+i; i=i+1; }"
            "  return s;"
            "}", JIT_LANG_C, "f", &fn);
        CHECK(r==0,"T2 compile");
        if(r==0){
            int64_t v=jit_call1(&fn,5); /* 0+1+2+3+4=10 */
            CHECK(v==10,"T2: sum(0..4)=10");
        }
        jit_free(ctx);
    }

    /* Test 3: Nested if inside while (two-pass + layout) */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        r = jit_compile(ctx,
            "long f(long n){"
            "  long s=0;"
            "  while(n>0){"
            "    if(n>1){ s=s+n; }"
            "    n=n-1;"
            "  }"
            "  return s;"
            "}", JIT_LANG_C, "f", &fn);
        CHECK(r==0,"T3 compile");
        if(r==0){
            int64_t v=jit_call1(&fn,4); /* 4+3+2=9 */
            CHECK(v==9,"T3: 4+3+2=9");
        }
        jit_free(ctx);
    }

    /* Test 4: #14 layout swap — single-return then-body */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        r = jit_compile(ctx,
            "long f(long x){"
            "  if(x<0){ return -1; }"
            "  else{ return x*2; }"
            "}", JIT_LANG_C, "f", &fn);
        CHECK(r==0,"T4 compile");
        if(r==0){
            CHECK(jit_call1(&fn,-5)==-1,"T4: f(-5)=-1");
            CHECK(jit_call1(&fn,7)==14,"T4: f(7)=14");
        }
        jit_free(ctx);
    }

    /* Test 5: Struct access + loop (A+B integration) */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        r = jit_compile(ctx,
            "struct S{ long x; U8 c; };"
            "long f(struct S* p, long n){"
            "  long s=0;"
            "  while(n>0){ s=s+p->x+p->c; n=n-1; }"
            "  return s;"
            "}", JIT_LANG_C, "f", &fn);
        CHECK(r==0,"T5 compile");
        if(r==0){
            unsigned char buf[16]; memset(buf,0,16);
            *(int64_t*)&buf[0]=100; buf[8]=3;
            int64_t v=jit_call2(&fn,(int64_t)(uintptr_t)buf,5); /* 5*(100+3)=515 */
            CHECK(v==515,"T5: 5*(100+3)=515");
        }
        jit_free(ctx);
    }

    /* Test 6: PGO + loop (C+B integration) */
    {
        setenv("WUBU_JIT_PGO", "1", 1);
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        r = jit_compile(ctx,
            "long f(long n){"
            "  long s=0;"
            "  while(n>0){ s=s+1; n=n-1; }"
            "  return s;"
            "}", JIT_LANG_C, "f", &fn);
        CHECK(r==0,"T6 compile (PGO)");
        if(r==0){
            int64_t v=jit_call1(&fn,10);
            CHECK(v==10,"T6: f(10)=10 (PGO correct)");
        }
        jit_free(ctx);
        unsetenv("WUBU_JIT_PGO");
    }

    /* Test 7: Multiple locals (stack frame sizing) */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        r = jit_compile(ctx,
            "long f(long a, long b){"
            "  long c=a+b;"
            "  long d=c*2;"
            "  long e=d-1;"
            "  return e;"
            "}", JIT_LANG_C, "f", &fn);
        CHECK(r==0,"T7 compile");
        if(r==0){
            int64_t v=jit_call2(&fn,3,4); /* c=7, d=14, e=13 */
            CHECK(v==13,"T7: (3+4)*2-1=13");
        }
        jit_free(ctx);
    }

    /* Test 8: Loop with invariant (LICM candidate) */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        r = jit_compile(ctx,
            "long f(long n){"
            "  long c=n*10;"
            "  long s=0;"
            "  while(s<c){ s=s+1; }"
            "  return s;"
            "}", JIT_LANG_C, "f", &fn);
        CHECK(r==0,"T8 compile");
        if(r==0){
            int64_t v=jit_call1(&fn,5); /* c=50, s counts to 50 */
            CHECK(v==50,"T8: s counts to n*10 = 50");
        }
        jit_free(ctx);
    }

    printf("=== jit_deep_opt_test: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
