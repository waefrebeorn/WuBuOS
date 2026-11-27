/* wubu_expr_test.c -- the slerm gate for SLERM-2012.
 *
 * Oracles are computed in Python at double precision and written as literals, so a
 * disagreement means the parser is wrong, not that the expected value drifted.
 * Every refusal path is asserted too: an evaluator that returns 0.0 for "1/0"
 * instead of naming the error is the fail-open shape this gate exists to stop.
 */
#include "wubu_expr.h"
#include <stdio.h>
#include <math.h>

static int pass, fail;

static void near(const char *expr, double want, double tol) {
    int err = 0;
    double got = wubu_expr_eval(expr, NULL, 0, &err);
    if (err == 0 && fabs(got - want) <= tol) { pass++; return; }
    printf("  [FAIL] %-22s want %.12g got %.12g err %d (%s)\n", expr, want, got, err, wubu_expr_reason(err));
    fail++;
}

static void with_vars(const char *expr, const WubuExprVar *v, size_t n, double want) {
    int err = 0;
    double got = wubu_expr_eval(expr, v, n, &err);
    if (err == 0 && fabs(got - want) <= 1e-12) { pass++; return; }
    printf("  [FAIL] %-22s want %.12g got %.12g err %d\n", expr, want, got, err);
    fail++;
}

static void refuses(const char *expr, int want_code) {
    int err = 0;
    (void)wubu_expr_eval(expr, NULL, 0, &err);
    if (err == want_code) { pass++; return; }
    printf("  [FAIL] %-22s expected refusal %d, got %d\n", expr, want_code, err);
    fail++;
}

int main(void) {
    printf("=== wubu_expr (slerm gate, SLERM-2012) ===\n");
    near("1+2*3", 7, 1e-9);
    near("2^3^2", 512, 1e-9);
    near("-2^2", -4, 1e-9);
    near("(1+2)*3", 9, 1e-9);
    near("10/4", 2.5, 1e-9);
    near("7%4", 3, 1e-9);
    near("sqrt(2)", 1.4142135623730951, 1e-9);
    near("sin(pi/2)", 1, 1e-9);
    near("max(3,7)", 7, 1e-9);
    near("clamp(12,0,10)", 10, 1e-9);
    near("abs(-3.5)", 3.5, 1e-9);
    near("2*(3+4)^2", 98, 1e-9);
    near("log(e)", 1, 1e-9);
    near("hypot(3,4)", 5, 1e-9);
    near("atan2(1,1)", 0.78539816339744828, 1e-9);
    near("1e3/2", 500, 1e-9);
    { WubuExprVar v[] = {{"x", 3}, {"y", 4}}; with_vars("x*y+1", v, 2, 13); }
    { WubuExprVar v[] = {{"x", 3}, {"y", 4}}; with_vars("x^2+y^2", v, 2, 25); }
    refuses("1/0", WUBU_EXPR_DIV0);
    refuses("5%0", WUBU_EXPR_DIV0);
    refuses("sqrt(-1)", WUBU_EXPR_DOMAIN);
    refuses("log(-1)", WUBU_EXPR_DOMAIN);
    refuses("nope(1)", WUBU_EXPR_NAME);
    refuses("1+", WUBU_EXPR_SYNTAX);
    refuses("1 2", WUBU_EXPR_SYNTAX);
    refuses("(1+2", WUBU_EXPR_SYNTAX);
    refuses("", WUBU_EXPR_SYNTAX);
    refuses("sin(1,2)", WUBU_EXPR_ARGS);

    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
