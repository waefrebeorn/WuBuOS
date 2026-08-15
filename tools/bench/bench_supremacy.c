/*
 * bench_supremacy.c — WuBu MIR JIT vs GCC -O2 head-to-head.
 *
 * Uses volatile accumulator to prevent GCC from optimizing away the loop.
 */
#include "wubu_mir.h"
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile int64_t g_a = 12345;
static volatile int64_t g_b = 6789;

static double time_gcc(const char *expr, int64_t *result) {
    FILE *f = fopen("/tmp/bench_gcc_loop.c", "w");
    if (!f) return -1;
    fprintf(f, "#include <stdint.h>\n#include <stdio.h>\n#include <time.h>\n");
    fprintf(f, "static volatile int64_t g_a = %lld, g_b = %lld;\n", (long long)g_a, (long long)g_b);
    fprintf(f, "int main(){\n");
    fprintf(f, "  int64_t a=g_a,b=g_b;\n");
    fprintf(f, "  struct timespec t0,t1;\n");
    fprintf(f, "  int N=1000000;\n");
    fprintf(f, "  clock_gettime(CLOCK_MONOTONIC,&t0);\n");
    fprintf(f, "  volatile int64_t r=0;\n");
    fprintf(f, "  for(int i=0;i<N;i++){r=r+(%s);}\n", expr);
    fprintf(f, "  clock_gettime(CLOCK_MONOTONIC,&t1);\n");
    fprintf(f, "  double ms=(t1.tv_sec-t0.tv_sec)*1000.0+(t1.tv_nsec-t0.tv_nsec)/1e6;\n");
    fprintf(f, "  printf(\"%%f %%lld\\n\",ms,(long long)(r/N));\n");
    fprintf(f, "  return 0;\n}\n");
    fclose(f);

    int rc = system("gcc -O2 -o /tmp/bench_gcc_loop /tmp/bench_gcc_loop.c -lrt 2>/dev/null");
    if (rc != 0) { fprintf(stderr, "GCC compile failed\n"); return -1; }

    FILE *p = popen("/tmp/bench_gcc_loop", "r");
    if (!p) return -1;
    double ms = 0;
    int64_t r = 0;
    fscanf(p, "%lf %lld", &ms, (long long *)&r);
    pclose(p);
    *result = r;
    return ms;
}

static double time_jit(void *code, size_t csize, int64_t *result) {
    const wubu_isa_driver_t *d = wubu_isa_find("x86-64");
    int64_t r = d->run(code, csize, 0);
    *result = r;

    struct timespec t0, t1;
    int N = 1000000;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile int64_t rr = 0;
    for (int i = 0; i < N; i++) {
        rr = d->run(code, csize, 0);
        rr += rr; /* prevent loop hoisting */
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    (void)rr;

    return (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
}

int main(void) {
    printf("=== WuBu MIR JIT vs GCC -O2 SUPREMACY BENCHMARK ===\n");
    printf("Inputs: a=%lld, b=%lld\n\n", (long long)g_a, (long long)g_b);

    /* Test 1: a*b + a/b */
    {
        int64_t gcc_r = 0, jit_r = 0;
        double gcc_ms = time_gcc("a*b + a/b", &gcc_r);

        wubu_mir_prog_t prog;
        wubu_mir_init(&prog);
        wubu_vr_t va = wubu_mir_const(&prog, g_a);
        wubu_vr_t vb = wubu_mir_const(&prog, g_b);
        wubu_vr_t mul = wubu_mir_binop(&prog, MIR_MUL, va, vb);
        wubu_vr_t div = wubu_mir_binop(&prog, MIR_DIV, va, vb);
        wubu_vr_t sum = wubu_mir_binop(&prog, MIR_ADD, mul, div);
        wubu_mir_ret(&prog, sum);

        uint8_t *code = NULL; size_t csize = 0;
        wubu_isa_find("x86-64")->compile(&prog, &code, &csize);
        double jit_ms = time_jit(code, csize, &jit_r);

        printf("Test 1: a*b + a/b\n");
        printf("  GCC -O2:  %lld (%.2f ms)\n", (long long)gcc_r, gcc_ms);
        printf("  WuBu JIT: %lld (%.2f ms)\n", (long long)jit_r, jit_ms);
        printf("  Ratio: %.2fx (GCC/WuBu) %s\n\n", jit_ms / gcc_ms,
               jit_r == gcc_r ? "CORRECT" : "WRONG");

        free(code);
        wubu_mir_free(&prog);
    }

    /* Test 2: (a+b)*(a-b) */
    {
        int64_t gcc_r = 0, jit_r = 0;
        double gcc_ms = time_gcc("(a+b)*(a-b)", &gcc_r);

        wubu_mir_prog_t prog;
        wubu_mir_init(&prog);
        wubu_vr_t va = wubu_mir_const(&prog, g_a);
        wubu_vr_t vb = wubu_mir_const(&prog, g_b);
        wubu_vr_t apb = wubu_mir_binop(&prog, MIR_ADD, va, vb);
        wubu_vr_t amb = wubu_mir_binop(&prog, MIR_SUB, va, vb);
        wubu_vr_t result = wubu_mir_binop(&prog, MIR_MUL, apb, amb);
        wubu_mir_ret(&prog, result);

        uint8_t *code = NULL; size_t csize = 0;
        wubu_isa_find("x86-64")->compile(&prog, &code, &csize);
        double jit_ms = time_jit(code, csize, &jit_r);

        printf("Test 2: (a+b)*(a-b)\n");
        printf("  GCC -O2:  %lld (%.2f ms)\n", (long long)gcc_r, gcc_ms);
        printf("  WuBu JIT: %lld (%.2f ms)\n", (long long)jit_r, jit_ms);
        printf("  Ratio: %.2fx (GCC/WuBu) %s\n\n", jit_ms / gcc_ms,
               jit_r == gcc_r ? "CORRECT" : "WRONG");

        free(code);
        wubu_mir_free(&prog);
    }

    /* Test 3: (a&b)|(a^b) */
    {
        int64_t gcc_r = 0, jit_r = 0;
        double gcc_ms = time_gcc("(a&b)|(a^b)", &gcc_r);

        wubu_mir_prog_t prog;
        wubu_mir_init(&prog);
        wubu_vr_t va = wubu_mir_const(&prog, g_a);
        wubu_vr_t vb = wubu_mir_const(&prog, g_b);
        wubu_vr_t anb = wubu_mir_binop(&prog, MIR_AND, va, vb);
        wubu_vr_t axb = wubu_mir_binop(&prog, MIR_XOR, va, vb);
        wubu_vr_t result = wubu_mir_binop(&prog, MIR_OR, anb, axb);
        wubu_mir_ret(&prog, result);

        uint8_t *code = NULL; size_t csize = 0;
        wubu_isa_find("x86-64")->compile(&prog, &code, &csize);
        double jit_ms = time_jit(code, csize, &jit_r);

        printf("Test 3: (a&b)|(a^b)\n");
        printf("  GCC -O2:  0x%016llx (%.2f ms)\n", (unsigned long long)gcc_r, gcc_ms);
        printf("  WuBu JIT: 0x%016llx (%.2f ms)\n", (unsigned long long)jit_r, jit_ms);
        printf("  Ratio: %.2fx (GCC/WuBu) %s\n\n", jit_ms / gcc_ms,
               jit_r == gcc_r ? "CORRECT" : "WRONG");

        free(code);
        wubu_mir_free(&prog);
    }

    printf("=== Benchmark complete ===\n");
    printf("Note: Ratio >1 means GCC is faster; <1 means WuBu is faster.\n");
    printf("      WuBu JIT uses register allocation but no peephole yet.\n");
    return 0;
}
