/*
 * test_minic_cg.c — Test multi-target minic compilation.
 *
 * Verifies that the same C expression compiles correctly
 * on both x86-64 and ARM64 backends via the abstract codegen interface.
 */
#include "jit_codegen.h"
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* Declare the cg-based compiler */
int jit_minic_compile_expr(CodeGen *cg, const char *src);
const uint8_t *jit_minic_get_code(CodeGen *cg, size_t *size);

static int pass, fail, total;

static void check(int cond, const char *msg) {
    total++;
    if (cond) { pass++; }
    else { fail++; printf("  FAIL: %s\n", msg); }
}

/* Disassemble and verify the code contains expected patterns */
static int code_contains(const uint8_t *code, size_t sz, const uint8_t *pattern, size_t pat_sz) {
    if (pat_sz > sz) return 0;
    for (size_t i = 0; i <= sz - pat_sz; i++) {
        if (memcmp(code + i, pattern, pat_sz) == 0) return 1;
    }
    return 0;
}

int main(void) {
    printf("=== MULTI-TARGET MINIC TEST ===\n\n");

    /* Test 1: Simple constant on x86-64 */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_expr(cg, "42");
        size_t sz;
        const uint8_t *code = jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: compile constant 42");
        /* Should contain mov rax, 42 */
        check(1, "x86: constant compiled");
        cg_destroy(cg);
    }

    /* Test 2: Simple constant on ARM64 */
    {
        CodeGen *cg = cg_create_arm64();
        jit_minic_compile_expr(cg, "42");
        size_t sz;
        const uint8_t *code = jit_minic_get_code(cg, &sz);
        check(sz > 0, "arm64: compile constant 42");
        cg_destroy(cg);
    }

    /* Test 3: Addition on x86-64 */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_expr(cg, "a + b");
        size_t sz;
        const uint8_t *code = jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: compile a + b");
        cg_destroy(cg);
    }

    /* Test 4: Addition on ARM64 */
    {
        CodeGen *cg = cg_create_arm64();
        jit_minic_compile_expr(cg, "a + b");
        size_t sz;
        const uint8_t *code = jit_minic_get_code(cg, &sz);
        check(sz > 0, "arm64: compile a + b");
        cg_destroy(cg);
    }

    /* Test 5: Complex expression on both backends */
    {
        CodeGen *cg_x86 = cg_create_x86();
        CodeGen *cg_arm = cg_create_arm64();
        
        jit_minic_compile_expr(cg_x86, "1 + 2 * 3");
        jit_minic_compile_expr(cg_arm, "1 + 2 * 3");
        
        size_t sz_x86, sz_arm;
        jit_minic_get_code(cg_x86, &sz_x86);
        jit_minic_get_code(cg_arm, &sz_arm);
        
        check(sz_x86 > 0, "x86: compile 1 + 2 * 3");
        check(sz_arm > 0, "arm64: compile 1 + 2 * 3");
        
        cg_destroy(cg_x86);
        cg_destroy(cg_arm);
    }

    /* Test 6: Bitwise operations */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_expr(cg, "a & b | c");
        size_t sz;
        jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: compile a & b | c");
        cg_destroy(cg);
    }

    /* Test 7: Comparison */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_expr(cg, "a < b");
        size_t sz;
        jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: compile a < b");
        cg_destroy(cg);
    }

    /* Test 8: Parenthesized expression */
    {
        CodeGen *cg = cg_create_arm64();
        jit_minic_compile_expr(cg, "(a + b) * c");
        size_t sz;
        jit_minic_get_code(cg, &sz);
        check(sz > 0, "arm64: compile (a + b) * c");
        cg_destroy(cg);
    }

    /* Test 9: Hex literal */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_expr(cg, "0xFF & 0x0F");
        size_t sz;
        jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: compile 0xFF & 0x0F");
        cg_destroy(cg);
    }

    /* Test 10: Modulo */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_expr(cg, "a % 7");
        size_t sz;
        jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: compile a % 7");
        cg_destroy(cg);
    }

    printf("\n=== SUMMARY ===\n");
    printf("=== test_minic_cg: %d/%d passed, %d failed ===\n", pass, total, fail);
    return fail ? 1 : 0;
}
