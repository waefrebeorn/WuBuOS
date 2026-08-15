/*
 * bench_regalloc.c — Benchmark: register-allocated x86-64 JIT.
 *
 * Computes: (1+2)*(3+4) + (5+6)*(7+8) = 3*7 + 11*15 = 21 + 165 = 186
 */
#include "wubu_mir.h"
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void) {
    /* Build MIR for the computation */
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);

    wubu_vr_t a = wubu_mir_const(&prog, 1);
    wubu_vr_t b = wubu_mir_const(&prog, 2);
    wubu_vr_t c = wubu_mir_const(&prog, 3);
    wubu_vr_t d = wubu_mir_const(&prog, 4);
    wubu_vr_t e = wubu_mir_const(&prog, 5);
    wubu_vr_t f = wubu_mir_const(&prog, 6);
    wubu_vr_t g = wubu_mir_const(&prog, 7);
    wubu_vr_t h = wubu_mir_const(&prog, 8);

    /* Chain of additions to exercise register pressure */
    wubu_vr_t s1 = wubu_mir_binop(&prog, MIR_ADD, a, b);
    wubu_vr_t s2 = wubu_mir_binop(&prog, MIR_ADD, c, d);
    wubu_vr_t s3 = wubu_mir_binop(&prog, MIR_ADD, e, f);
    wubu_vr_t s4 = wubu_mir_binop(&prog, MIR_ADD, g, h);
    wubu_vr_t m1 = wubu_mir_binop(&prog, MIR_MUL, s1, s2);
    wubu_vr_t m2 = wubu_mir_binop(&prog, MIR_MUL, s3, s4);
    wubu_vr_t result = wubu_mir_binop(&prog, MIR_ADD, m1, m2);

    wubu_mir_ret(&prog, result);

    /* Expected: (1+2)*(3+4) + (5+6)*(7+8) = 3*7 + 11*15 = 21 + 165 = 186 */
    int64_t expected = (1+2)*(3+4) + (5+6)*(7+8);
    printf("Expected: %lld\n", (long long)expected);

    /* Compile with x86-64 (regalloc) */
    const wubu_isa_driver_t *dx = wubu_isa_find("x86-64");
    uint8_t *code = NULL; size_t csize = 0;
    if (dx->compile(&prog, &code, &csize) != 0) {
        printf("Compile failed\n");
        return 1;
    }

    /* Verify correctness */
    int64_t result_x = dx->run(code, csize, 0);
    printf("x86-64 JIT (regalloc): %lld %s\n", (long long)result_x,
           result_x == expected ? "OK" : "WRONG");

    /* Benchmark */
    struct timespec t0, t1;
    int iterations = 1000000;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int iter = 0; iter < iterations; iter++) {
        dx->run(code, csize, 0);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("x86-64 JIT (%d iters): %.3f ms (%.1f ns/iter)\n",
           iterations, elapsed * 1000, elapsed * 1e9 / iterations);

    /* Compare with 8086 interpreter */
    const wubu_isa_driver_t *d8086 = wubu_isa_find("8086");
    uint8_t *code_8086 = NULL; size_t csize_8086 = 0;
    if (d8086->compile(&prog, &code_8086, &csize_8086) == 0) {
        int64_t result_8086 = d8086->run(code_8086, csize_8086, 0);
        printf("8086 interpreter: %lld %s\n", (long long)result_8086,
               result_8086 == expected ? "OK" : "WRONG");

        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int iter = 0; iter < iterations / 10; iter++) {
            d8086->run(code_8086, csize_8086, 0);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);

        double elapsed_8086 = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
        printf("8086 interpreter (%d iters): %.3f ms (%.1f ns/iter, 10x fewer iters)\n",
               iterations / 10, elapsed_8086 * 1000, elapsed_8086 * 1e9 / (iterations / 10));
        free(code_8086);
    }

    free(code);
    wubu_mir_free(&prog);
    return 0;
}
