/*
 * jit_cycle_profiler.c -- Measure actual cycle costs of generated code.
 * Uses rdtsc for wall-clock measurement and counts instructions by type
 * to identify the top cycle-wasters in the JIT output.
 */
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

static inline uint64_t rdtsc(void) {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static int comp_total, comp_pass, comp_fail;
#define CHECK(c,m) do{comp_total++;if(c)comp_pass++;else{comp_fail++;printf("  FAIL: %s\n",m);}}while(0)

/* Measure a function's execution time over many iterations */
static uint64_t measure(JITFunc *fn, int64_t a, int64_t n_iters) {
    typedef int64_t (*fp)(int64_t);
    fp f = (fp)fn->code;
    /* Warmup */
    for (int i = 0; i < 100; i++) f(a);
    /* Measure */
    uint64_t start = rdtsc();
    volatile int64_t sink = 0;
    for (int64_t i = 0; i < n_iters; i++) {
        sink = f(a);
    }
    uint64_t end = rdtsc();
    (void)sink;
    return (end - start) / n_iters;
}

static int comp_total, comp_pass, comp_fail;
#undef CHECK
#define CHECK(c,m) do{comp_total++;if(c)comp_pass++;else{comp_fail++;printf("  FAIL: %s\n",m);}}while(0)

int main(void) {
    JITContext *ctx = jit_init();
    JITFunc fn;

    printf("=== JIT CYCLE PROFILER ===\n\n");

    /* Baseline: simplest possible function */
    uint64_t base_cycles = 0;

    /* Test 1: Identity function (baseline overhead) */
    jit_compile(ctx, "long f(long x){ return x; }", JIT_LANG_C, "f", &fn);
    uint64_t c_identity = measure(&fn, 42, 1000000);
    printf("Identity 'return x': %lu cyc/call, code=%zuB\n", c_identity, fn.code_size);
    base_cycles = c_identity;

    /* Test 2: Add constant */
    jit_compile(ctx, "long f(long x){ return x+1; }", JIT_LANG_C, "f", &fn);
    uint64_t c_add1 = measure(&fn, 42, 1000000);
    printf("Add const 'x+1': %lu cyc/call (+%lu vs identity)\n", c_add1, c_add1 - base_cycles);

    /* Test 3: Multiply by 5 (lea) */
    jit_compile(ctx, "long f(long x){ return x*5; }", JIT_LANG_C, "f", &fn);
    uint64_t c_mul5 = measure(&fn, 7, 1000000);
    printf("Mul by 5 'x*5': %lu cyc/call (+%lu vs identity)\n", c_mul5, c_mul5 - base_cycles);

    /* Test 4: Divide by 7 (magic multiply) */
    jit_compile(ctx, "long f(long x){ return x/7; }", JIT_LANG_C, "f", &fn);
    uint64_t c_div7 = measure(&fn, 49, 1000000);
    printf("Div by 7 'x/7': %lu cyc/call (+%lu vs identity)\n", c_div7, c_div7 - base_cycles);

    /* Test 5: Loop 100 iterations */
    jit_compile(ctx, "long f(long n){ long s=0; while(n>0){ s=s+1; n=n-1; } return s; }", JIT_LANG_C, "f", &fn);
    uint64_t c_loop100 = measure(&fn, 100, 100000);
    printf("Loop 100 iters: %lu cyc/call (%lu cyc/iter)\n", c_loop100, c_loop100/100);

    /* Test 6: Loop 1000 iterations */
    uint64_t c_loop1000 = measure(&fn, 1000, 10000);
    printf("Loop 1000 iters: %lu cyc/call (%lu cyc/iter)\n", c_loop1000, c_loop1000/1000);

    /* Test 7: if/else branch */
    jit_compile(ctx, "long f(long x){ if(x>0){ return x; } else{ return -x; } }", JIT_LANG_C, "f", &fn);
    uint64_t c_ifelse = measure(&fn, 5, 1000000);
    printf("if/else branch: %lu cyc/call (+%lu vs identity)\n", c_ifelse, c_ifelse - base_cycles);

    /* Test 8: Struct access */
    jit_compile(ctx, "struct S{ long x; U8 c; }; long f(struct S* p){ return p->x+p->c; }", JIT_LANG_C, "f", &fn);
    unsigned char buf[16]; memset(buf,0,16); *(int64_t*)&buf[0]=100; buf[8]=3;
    /* Warmup */
    typedef int64_t (*fp)(int64_t); fp fs = (fp)fn.code;
    for(int i=0;i<100;i++) fs((int64_t)(uintptr_t)buf);
    uint64_t s = rdtsc();
    volatile int64_t sink=0;
    for(int i=0;i<1000000;i++) sink=fs((int64_t)(uintptr_t)buf);
    uint64_t e = rdtsc();
    uint64_t c_struct = (e-s)/1000000;
    printf("Struct access: %lu cyc/call (+%lu vs identity)\n", c_struct, c_struct - base_cycles);
    (void)sink;

    printf("\n=== ANALYSIS ===\n");
    printf("Base overhead (prologue+epilogue+ret): ~%lu cyc\n", base_cycles);
    printf("Per-iteration loop overhead: ~%lu cyc\n", (c_loop1000-c_loop100)/900);

    /* Correctness checks */
    printf("\n=== CORRECTNESS ===\n");
    jit_compile(ctx, "long f(long x){ return x*5; }", JIT_LANG_C, "f", &fn);
    CHECK(jit_call1(&fn,7)==35, "7*5=35");
    jit_compile(ctx, "long f(long x){ return x/7; }", JIT_LANG_C, "f", &fn);
    CHECK(jit_call1(&fn,49)==7, "49/7=7");
    jit_compile(ctx, "long f(long n){ long s=0; while(n>0){ s=s+1; n=n-1; } return s; }", JIT_LANG_C, "f", &fn);
    CHECK(jit_call1(&fn,100)==100, "loop 100");

    printf("\n=== cycle_profiler: %d/%d correctness checks passed ===\n", comp_pass, comp_total);
    jit_free(ctx);
    return comp_fail ? 1 : 0;
}
