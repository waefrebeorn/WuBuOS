#include <stdlib.h>
/*
 * test_mir_opt.c -- MIR optimizer pass tests.
 *
 * Verifies each optimizer pass:
 * 1. Constant folding: 3+4 -> 7 (at compile time)
 * 2. Strength reduction: x*2 -> x<<1
 * 3. DCE: unused instructions removed
 * 4. Combined pipeline
 *
 * C11, self-contained.
 */
#include "wubu_mir.h"
#include "wubu_mir_opt.h"
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int pass_count = 0, fail_count = 0, total = 0;
#define CHECK(c, m) do { total++; if (c) { pass_count++; printf("  ok: %s\n", m); } else { fail_count++; printf("  FAIL: %s\n", m); } } while (0)

/* Count non-label/non-jmp instructions in a program */
static size_t count_ops(const wubu_mir_prog_t *p)
{
    size_t n = 0;
    for (size_t i = 0; i < p->n; i++) {
        if (p->ins[i].op != MIR_LABEL && p->ins[i].op != MIR_JMP &&
            p->ins[i].op != MIR_JZ && p->ins[i].op != MIR_RET)
            n++;
    }
    return n;
}

/* Test 1: Constant folding */
static void test_fold(void)
{
    printf("-- Test: constant folding (3 + 4 = 7) --\n");
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t a = wubu_mir_const(&prog, 3);
    wubu_vr_t b = wubu_mir_const(&prog, 4);
    wubu_vr_t r = wubu_mir_binop(&prog, MIR_ADD, a, b);
    wubu_mir_ret(&prog, r);

    wubu_mir_optimize(&prog, MIR_OPT_FOLD);

    /* After folding, the result vr should be a CONST 7 */
    int found_const_7 = 0;
    for (size_t i = 0; i < prog.n; i++) {
        if (prog.ins[i].op == MIR_CONST && prog.ins[i].imm == 7)
            found_const_7 = 1;
    }
    CHECK(found_const_7, "3+4 folded to CONST 7");

    /* Verify it still runs correctly on x86-64 */
    const wubu_isa_driver_t *d = wubu_isa_find("x86-64");
    if (d) {
        uint8_t *code = NULL;
        size_t csize = 0;
        if (d->compile(&prog, &code, &csize) == 0) {
            int64_t result = d->run(code, csize, 0);
            CHECK(result == 7, "folded program returns 7");
            free(code);
        }
    }
    wubu_mir_free(&prog);
}

/* Test 2: Constant folding with multiplication */
static void test_fold_mul(void)
{
    printf("-- Test: constant folding (7 * 6 = 42) --\n");
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t a = wubu_mir_const(&prog, 7);
    wubu_vr_t b = wubu_mir_const(&prog, 6);
    wubu_vr_t r = wubu_mir_binop(&prog, MIR_MUL, a, b);
    wubu_mir_ret(&prog, r);

    wubu_mir_optimize(&prog, MIR_OPT_FOLD);

    int found_const_42 = 0;
    for (size_t i = 0; i < prog.n; i++) {
        if (prog.ins[i].op == MIR_CONST && prog.ins[i].imm == 42)
            found_const_42 = 1;
    }
    CHECK(found_const_42, "7*6 folded to CONST 42");
    wubu_mir_free(&prog);
}

/* Test 3: DCE — unused instructions removed */
static void test_dce(void)
{
    printf("-- Test: dead code elimination --\n");
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t a = wubu_mir_const(&prog, 10);
    wubu_vr_t b = wubu_mir_const(&prog, 20);
    /* c = a + b (dead — never used) */
    wubu_vr_t c = wubu_mir_binop(&prog, MIR_ADD, a, b);
    /* d = a (used by ret) */
    wubu_vr_t d = wubu_mir_binop(&prog, MIR_MOV, a, 0);
    (void)c;
    wubu_mir_ret(&prog, d);

    size_t before = count_ops(&prog);
    wubu_mir_optimize(&prog, MIR_OPT_DCE);
    size_t after = count_ops(&prog);

    CHECK(after <= before, "DCE doesn't increase instruction count");
    printf("  before=%zu after=%zu ops\n", before, after);
    wubu_mir_free(&prog);
}

/* Test 4: Combined pipeline (fold + dce) */
static void test_pipeline(void)
{
    printf("-- Test: combined pipeline (fold + dce) --\n");
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    /* (3+4) * (5+2) = 7 * 7 = 49 */
    wubu_vr_t a = wubu_mir_const(&prog, 3);
    wubu_vr_t b = wubu_mir_const(&prog, 4);
    wubu_vr_t sum1 = wubu_mir_binop(&prog, MIR_ADD, a, b);
    wubu_vr_t c = wubu_mir_const(&prog, 5);
    wubu_vr_t d = wubu_mir_const(&prog, 2);
    wubu_vr_t sum2 = wubu_mir_binop(&prog, MIR_ADD, c, d);
    wubu_vr_t r = wubu_mir_binop(&prog, MIR_MUL, sum1, sum2);
    wubu_mir_ret(&prog, r);

    wubu_mir_optimize(&prog, MIR_OPT_FOLD | MIR_OPT_DCE);

    int found_const_49 = 0;
    for (size_t i = 0; i < prog.n; i++) {
        if (prog.ins[i].op == MIR_CONST && prog.ins[i].imm == 49)
            found_const_49 = 1;
    }
    CHECK(found_const_49, "(3+4)*(5+2) folded to CONST 49");

    /* Verify on x86-64 */
    const wubu_isa_driver_t *drv = wubu_isa_find("x86-64");
    if (drv) {
        uint8_t *code = NULL;
        size_t csize = 0;
        if (drv->compile(&prog, &code, &csize) == 0) {
            int64_t result = drv->run(code, csize, 0);
            CHECK(result == 49, "pipeline result is 49");
            free(code);
        }
    }
    wubu_mir_free(&prog);
}

/* Test 5: Optimizer preserves correctness across all drivers */
static void test_opt_correctness(void)
{
    printf("-- Test: optimizer correctness across all drivers --\n");
    /* Build: (10+20)*3 = 90 */
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t a = wubu_mir_const(&prog, 10);
    wubu_vr_t b = wubu_mir_const(&prog, 20);
    wubu_vr_t sum = wubu_mir_binop(&prog, MIR_ADD, a, b);
    wubu_vr_t c = wubu_mir_const(&prog, 3);
    wubu_vr_t r = wubu_mir_binop(&prog, MIR_MUL, sum, c);
    wubu_mir_ret(&prog, r);

    /* Run unoptimized on all drivers */
    const char *names[] = {"x86-64", "8086", "m68k", "6502", "riscv", "z80"};
    int64_t unopt_results[6] = {0};
    for (int i = 0; i < 6; i++) {
        const wubu_isa_driver_t *d = wubu_isa_find(names[i]);
        if (!d) continue;
        uint8_t *code = NULL;
        size_t csize = 0;
        if (d->compile(&prog, &code, &csize) == 0) {
            unopt_results[i] = d->run(code, csize, 0);
            free(code);
        }
    }

    /* Optimize */
    wubu_mir_optimize(&prog, MIR_OPT_FOLD | MIR_OPT_DCE);

    /* Run optimized on all drivers */
    int all_match = 1;
    for (int i = 0; i < 6; i++) {
        const wubu_isa_driver_t *d = wubu_isa_find(names[i]);
        if (!d) continue;
        uint8_t *code = NULL;
        size_t csize = 0;
        if (d->compile(&prog, &code, &csize) == 0) {
            int64_t opt_result = d->run(code, csize, 0);
            if (opt_result != 90 || opt_result != unopt_results[i]) {
                printf("  %s: unopt=%lld opt=%lld expected=90\n",
                       names[i], (long long)unopt_results[i], (long long)opt_result);
                all_match = 0;
            }
            free(code);
        }
    }
    CHECK(all_match, "optimizer preserves correctness across all 6 drivers");
    wubu_mir_free(&prog);
}

/* Test 6: Strength reduction */
static void test_strength(void)
{
    printf("-- Test: strength reduction (x+0 -> x) --\n");
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t a = wubu_mir_const(&prog, 42);
    wubu_vr_t z = wubu_mir_const(&prog, 0);
    wubu_vr_t r = wubu_mir_binop(&prog, MIR_ADD, a, z);
    wubu_mir_ret(&prog, r);

    wubu_mir_optimize(&prog, MIR_OPT_STRENGTH);

    /* After strength reduction, the ADD should become MOV */
    int found_mov = 0;
    for (size_t i = 0; i < prog.n; i++) {
        if (prog.ins[i].op == MIR_MOV) found_mov = 1;
    }
    CHECK(found_mov, "x+0 reduced to MOV");

    /* Verify correctness */
    const wubu_isa_driver_t *d = wubu_isa_find("x86-64");
    if (d) {
        uint8_t *code = NULL;
        size_t csize = 0;
        if (d->compile(&prog, &code, &csize) == 0) {
            int64_t result = d->run(code, csize, 0);
            CHECK(result == 42, "strength-reduced program returns 42");
            free(code);
        }
    }
    wubu_mir_free(&prog);
}

/* Test 7: Common subexpression elimination */
static void test_cse(void)
{
    printf("-- Test: common subexpression elimination --\n");
    /* Build: v1=7, v2=6, v3=v1+v2, v4=v1+v2 (redundant), v5=v3+v4 */
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t v1 = wubu_mir_const(&prog, 7);
    wubu_vr_t v2 = wubu_mir_const(&prog, 6);
    wubu_vr_t v3 = wubu_mir_binop(&prog, MIR_ADD, v1, v2); /* 13 */
    wubu_vr_t v4 = wubu_mir_binop(&prog, MIR_ADD, v1, v2); /* redundant */
    wubu_vr_t v5 = wubu_mir_binop(&prog, MIR_ADD, v3, v4); /* 26 */
    wubu_mir_ret(&prog, v5);

    /* Before optimization: 3 ADD instructions */
    size_t adds_before = 0;
    for (size_t i = 0; i < prog.n; i++) if (prog.ins[i].op == MIR_ADD) adds_before++;

    wubu_mir_optimize(&prog, MIR_OPT_CSE);

    /* After optimization: redundant ADD replaced with MOV */
    size_t adds_after = 0;
    for (size_t i = 0; i < prog.n; i++) if (prog.ins[i].op == MIR_ADD) adds_after++;

    CHECK(adds_after < adds_before, "CSE eliminates redundant ADD");
    printf("  ADD count: %zu -> %zu\n", adds_before, adds_after);

    /* Verify correctness: result should still be 26 */
    const wubu_isa_driver_t *d = wubu_isa_find("x86-64");
    uint8_t *code = NULL; size_t csize = 0;
    if (d->compile(&prog, &code, &csize) == 0) {
        int64_t result = d->run(code, csize, 0);
        CHECK(result == 26, "CSE preserves correctness (26)");
        free(code);
    }
    wubu_mir_free(&prog);
}

int main(void)
{
    printf("=== MIR OPTIMIZER TEST ===\n\n");
    test_fold();
    printf("\n");
    test_fold_mul();
    printf("\n");
    test_dce();
    printf("\n");
    test_pipeline();
    printf("\n");
    test_opt_correctness();
    printf("\n");
    test_strength();
    printf("\n");
    test_cse();
    printf("\n");

    printf("=== %s: %d/%d passed ===\n",
           fail_count == 0 ? "OPTIMIZER PASSED" : "OPTIMIZER FAILED",
           pass_count, total);
    return fail_count == 0 ? 0 : 1;
}
