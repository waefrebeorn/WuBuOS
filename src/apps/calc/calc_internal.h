/* calc_internal.h -- Internal helpers shared by calc sub-modules.
 * Public API + types in calc.h. Math eval in calc_math.c.
 */

#ifndef CALC_INTERNAL_H
#define CALC_INTERNAL_H

#include "calc.h"

double calc_apply_op(int op, double a, double b, bool *err);
double calc_apply_func(int func, double x, bool *err);

#endif /* CALC_INTERNAL_H */
