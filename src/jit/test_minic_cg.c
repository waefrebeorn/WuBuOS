/*
 * test_minic_cg.c — Test multi-target minic compilation (full grammar).
 */
#include "jit_codegen.h"
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

int jit_minic_compile_cg(CodeGen *cg, const char *src);
const uint8_t *jit_minic_get_code(CodeGen *cg, size_t *size);

static int pass, fail, total;
static void check(int cond, const char *msg) {
    total++;
    if (cond) pass++;
    else { fail++; printf("  FAIL: %s\n", msg); }
}

int main(void) {
    printf("=== MULTI-TARGET FULL MINIC TEST ===\n\n");

    /* Test 1: Simple constant */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "return 42;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: return 42");
        cg_destroy(cg);
    }

    /* Test 2: Constant on ARM64 */
    {
        CodeGen *cg = cg_create_arm64();
        jit_minic_compile_cg(cg, "return 42;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "arm64: return 42");
        cg_destroy(cg);
    }

    /* Test 3: Expression a + b */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "return a + b;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: return a + b");
        cg_destroy(cg);
    }

    /* Test 4: Expression on ARM64 */
    {
        CodeGen *cg = cg_create_arm64();
        jit_minic_compile_cg(cg, "return a + b;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "arm64: return a + b");
        cg_destroy(cg);
    }

    /* Test 5: Complex expression */
    {
        CodeGen *cg_x86 = cg_create_x86();
        CodeGen *cg_arm = cg_create_arm64();
        jit_minic_compile_cg(cg_x86, "return 1 + 2 * 3;");
        jit_minic_compile_cg(cg_arm, "return 1 + 2 * 3;");
        size_t sz1, sz2;
        jit_minic_get_code(cg_x86, &sz1);
        jit_minic_get_code(cg_arm, &sz2);
        check(sz1 > 0, "x86: return 1 + 2 * 3");
        check(sz2 > 0, "arm64: return 1 + 2 * 3");
        cg_destroy(cg_x86);
        cg_destroy(cg_arm);
    }

    /* Test 6: Variable declaration */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "long x = 42; return x;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: long x = 42; return x");
        cg_destroy(cg);
    }

    /* Test 7: If statement */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "if(a > b) { return a; } else { return b; }");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: if/else");
        cg_destroy(cg);
    }

    /* Test 8: If on ARM64 */
    {
        CodeGen *cg = cg_create_arm64();
        jit_minic_compile_cg(cg, "if(a > b) { return a; } else { return b; }");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "arm64: if/else");
        cg_destroy(cg);
    }

    /* Test 9: While loop */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "long s = 0; while(s < 10) { s = s + 1; } return s;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: while loop");
        cg_destroy(cg);
    }

    /* Test 10: While on ARM64 */
    {
        CodeGen *cg = cg_create_arm64();
        jit_minic_compile_cg(cg, "long s = 0; while(s < 10) { s = s + 1; } return s;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "arm64: while loop");
        cg_destroy(cg);
    }

    /* Test 11: Bitwise ops */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "return a & b | c;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: a & b | c");
        cg_destroy(cg);
    }

    /* Test 12: Hex literal */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "return 0xFF & 0x0F;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: 0xFF & 0x0F");
        cg_destroy(cg);
    }

    /* Test 13: Comparison */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "return a < b;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: a < b");
        cg_destroy(cg);
    }

    /* Test 14: Modulo */
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "return a % 7;");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "x86: a % 7");
        cg_destroy(cg);
    }

    /* Test 15: Nested parentheses */
    {
        CodeGen *cg = cg_create_arm64();
        jit_minic_compile_cg(cg, "return (a + b) * (c - d);");
        size_t sz; jit_minic_get_code(cg, &sz);
        check(sz > 0, "arm64: (a + b) * (c - d)");
        cg_destroy(cg);
    }

    printf("\n=== SUMMARY ===\n");
    printf("=== test_minic_cg: %d/%d passed, %d failed ===\n", pass, total, fail);
    return fail ? 1 : 0;
}
