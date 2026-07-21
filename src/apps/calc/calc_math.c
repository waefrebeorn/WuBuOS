/* calc_math.c -- Calculator math evaluation (self-contained).
 *
 * calc_apply_op / calc_apply_func: pure double->double ops. Types in calc.h.
 * Minimal includes.
 */

#include "calc.h"
#include <math.h>

double calc_apply_op(int op, double a, double b, bool *err) {
    switch (op) {
        case CALC_OP_ADD: return a + b;
        case CALC_OP_SUB: return a - b;
        case CALC_OP_MUL: return a * b;
        case CALC_OP_DIV:
            if (b == 0.0) { *err = true; return 0.0; }
            return a / b;
        case CALC_OP_POW: return pow(a, b);
        default:          return b;
    }
}

double calc_apply_func(int func, double x, bool *err) {
    switch (func) {
        case CALC_FUNC_SQRT:
            if (x < 0.0) { *err = true; return 0.0; }
            return sqrt(x);
        case CALC_FUNC_NEG:   return -x;
        case CALC_FUNC_RECIP:
            if (x == 0.0) { *err = true; return 0.0; }
            return 1.0 / x;
        case CALC_FUNC_SIN: return sin(x);
        case CALC_FUNC_COS: return cos(x);
        case CALC_FUNC_TAN: return tan(x);
        case CALC_FUNC_LN:
            if (x <= 0.0) { *err = true; return 0.0; }
            return log(x);
        case CALC_FUNC_LOG:
            if (x <= 0.0) { *err = true; return 0.0; }
            return log10(x);
        case CALC_FUNC_EXP: return exp(x);
        case CALC_FUNC_PERCENT: return x / 100.0;
        case CALC_FUNC_CLEAR: return 0.0;
        default: return x;
    }
}
