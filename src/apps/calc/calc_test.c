/*
 * calc_test.c  --  WuBuOS Calculator behavioral tests
 *
 * Exercises the real calculation engine (no GUI required): digit entry,
 * operator chaining, unary functions, error handling, base clamping.
 */

#include "calc.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int pass = 0, fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fail++; printf("  FAIL: %s\n", msg); return; } } while (0)
#define OK() do { pass++; printf("  ok\n"); } while (0)

/* Helper: feed a sequence of decimal digits, then an op. */
static void enter(CalcState *c, const char *digits) {
    for (const char *p = digits; *p; p++)
        calc_input_digit(c, *p - '0');
}
static void run(CalcState *c, const char *a, int op, const char *b) {
    enter(c, a);
    calc_input_op(c, op);
    enter(c, b);
    calc_input_op(c, CALC_OP_EQ);
}

static void test_add(void) {
    printf("calc add 2+3");
    CalcState *c = calc_create();
    run(c, "2", CALC_OP_ADD, "3");
    CHECK(calc_get_display(c) == 5.0, "2+3 should be 5");
    calc_destroy(c);
    OK();
}

static void test_sub_chain(void) {
    printf("calc subtract chain 10-3-2");
    CalcState *c = calc_create();
    enter(c, "10");
    calc_input_op(c, CALC_OP_SUB);
    enter(c, "3");
    calc_input_op(c, CALC_OP_SUB);   /* chain: (10-3)=7 pending, then -2 */
    enter(c, "2");
    calc_input_op(c, CALC_OP_EQ);
    CHECK(calc_get_display(c) == 5.0, "10-3-2 should be 5");
    calc_destroy(c);
    OK();
}

static void test_mul_div(void) {
    printf("calc mul/div 6*7 then /2");
    CalcState *c = calc_create();
    run(c, "6", CALC_OP_MUL, "7");
    CHECK(calc_get_display(c) == 42.0, "6*7 should be 42");
    calc_input_op(c, CALC_OP_DIV);
    enter(c, "2");
    calc_input_op(c, CALC_OP_EQ);
    CHECK(calc_get_display(c) == 21.0, "42/2 should be 21");
    calc_destroy(c);
    OK();
}

static void test_func_sqrt(void) {
    printf("calc sqrt(144)");
    CalcState *c = calc_create();
    enter(c, "144");
    calc_input_func(c, CALC_FUNC_SQRT);
    CHECK(calc_get_display(c) == 12.0, "sqrt(144) should be 12");
    calc_destroy(c);
    OK();
}

static void test_func_neg(void) {
    printf("calc negate 7");
    CalcState *c = calc_create();
    enter(c, "7");
    calc_input_func(c, CALC_FUNC_NEG);
    CHECK(calc_get_display(c) == -7.0, "neg(7) should be -7");
    calc_destroy(c);
    OK();
}

static void test_func_sin(void) {
    printf("calc sin(0) ~ 0");
    CalcState *c = calc_create();
    enter(c, "0");
    calc_input_func(c, CALC_FUNC_SIN);
    CHECK(calc_get_display(c) == 0.0, "sin(0) should be 0");
    calc_destroy(c);
    OK();
}

static void test_div_zero_error(void) {
    printf("calc divide-by-zero errors");
    CalcState *c = calc_create();
    run(c, "5", CALC_OP_DIV, "0");
    CHECK(calc_in_error(c) == true, "5/0 should set error state");
    calc_destroy(c);
    OK();
}

static void test_clear(void) {
    printf("calc clear resets state");
    CalcState *c = calc_create();
    enter(c, "99");
    calc_input_func(c, CALC_FUNC_CLEAR);
    CHECK(calc_get_display(c) == 0.0, "clear should zero display");
    calc_destroy(c);
    OK();
}

static void test_base_clamp(void) {
    printf("calc base-2 rejects digit 2");
    CalcState *c = calc_create();
    calc_set_base(c, 2);
    calc_input_digit(c, 2);   /* out of range for base 2 */
    CHECK(calc_get_display(c) == 0.0, "digit 2 invalid in base 2 -> ignored");
    calc_input_digit(c, 1);   /* valid */
    CHECK(calc_get_display(c) == 1.0, "digit 1 valid in base 2");
    calc_destroy(c);
    OK();
}

int main(void) {
    printf("=== WuBuOS Calculator Engine Tests ===\n\n");
    test_add();
    test_sub_chain();
    test_mul_div();
    test_func_sqrt();
    test_func_neg();
    test_func_sin();
    test_div_zero_error();
    test_clear();
    test_base_clamp();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
