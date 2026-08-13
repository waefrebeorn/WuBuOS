/*
 * jit_regalloc_test.c — prove the JIT's register-allocation path survives
 * clobber-prone nested expressions.
 *
 * The mini-C JIT in jit_minic.c funnels all operands through WREG_RAX via a
 * hand-rolled push/pop dance (compile_multiplicative/ Additive). Deeply
 * nested expressions like (1+2)*(3+4) or (1+2)*(3+4)+(5+6) stress the
 * register survival path: the RHS evaluation clobbers rax, so the saved LHS
 * must be preserved. These probe EXACTLY that -- the same bug class the
 * wubuos-holyc-compiler skill documents as "the rdi-CLOBBER BINOP FAMILY".
 *
 * If the push/pop or rax-funnel is broken, the LHS is destroyed and the
 * result is wrong. This is the allocator-correctness gate for the JIT tier.
 */
#include "jit.h"
#include <stdio.h>

static int failures = 0;
static int passed = 0;
#define CHECK(c, m) do { if (c) { passed++; } else { printf("FAIL: %s\n", m); failures++; } } while(0)

int main(void) {
    JITContext *ctx = jit_init();
    if (!ctx) { printf("jit_init failed\n"); return 1; }

    /* 1. Nested multiplication with parenthesized operands.
     *    (1+2)*(3+4) = 3*7 = 21 -- exercises LHS push before RHS clobbers rax */
    JITFunc fn_mul_nested;
    JITResult r = jit_compile(ctx, "(1+2)*(3+4)", JIT_LANG_C, "mul_nested", &fn_mul_nested);
    CHECK(r == JIT_OK, "compile '(1+2)*(3+4)'");
    if (r == JIT_OK) {
        int64_t res = jit_call0(&fn_mul_nested);
        CHECK(res == 21, "(1+2)*(3+4) == 21");
    }

    /* 2. Chained add+mul: 1+2*3+4 = 1+6+4 = 11 -- precedence + nested clobbers */
    JITFunc fn_prec;
    r = jit_compile(ctx, "a+b*c+d", JIT_LANG_C, "prec", &fn_prec);
    CHECK(r == JIT_OK, "compile 'a+b*c+d'");
    if (r == JIT_OK) CHECK((int64_t)(intptr_t)jit_callv(&fn_prec, 1, 2, 3, 4) == 11, "1+2*3+4==11");

    /* 3. Deeply nested subtract: (10-(3*2))-(1+1) = (10-6)-(1+1) = 2
     *    Multiplies and subtracts chained; LHS saved across RHS */
    JITFunc fn_deep;
    r = jit_compile(ctx, "(a-(b*c))-(d+e)", JIT_LANG_C, "deep", &fn_deep);
    CHECK(r == JIT_OK, "compile '(a-(b*c))-(d+e)'");
    if (r == JIT_OK) CHECK((int64_t)(intptr_t)jit_callv(&fn_deep, 10, 3, 2, 1, 1) == 2, "(10-(3*2))-(1+1)==2");

    /* 4. Left-associative subtraction (non-commutative -- operand ORDER matters):
     *    a-b-c == (a-b)-c. 10-3-2 = 5. If LHS/RHS reversed, get 2-3-10 = -11. */
    JITFunc fn_leftassoc;
    r = jit_compile(ctx, "a-b-c", JIT_LANG_C, "leftassoc", &fn_leftassoc);
    CHECK(r == JIT_OK, "compile 'a-b-c'");
    if (r == JIT_OK) CHECK((int64_t)(intptr_t)jit_callv(&fn_leftassoc, 10, 3, 2) == 5, "10-3-2==5 (operand order)");

    /* 5. Constant nested: ((1+2)*(3+4))+(5*6) = 21+30 = 51 */
    JITFunc fn_const;
    r = jit_compile(ctx, "((1+2)*(3+4))+(5*6)", JIT_LANG_C, "const_nested", &fn_const);
    CHECK(r == JIT_OK, "compile deeply constant-nested");
    if (r == JIT_OK) CHECK(jit_call0(&fn_const) == 51, "((1+2)*(3+4))+(5*6)==51");

    printf("\n=== jit_regalloc_test: %d passed, %d failed ===\n", passed, failures);
    jit_free(ctx);
    return failures ? 1 : 0;
}
