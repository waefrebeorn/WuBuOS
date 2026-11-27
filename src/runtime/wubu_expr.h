/* wubu_expr.h -- arithmetic expressions, parsed and evaluated in-process.
 *
 * Replaces a vendored expression library (see docs/GOAL-SLERM.json, SLERM-2012):
 * a calculator field, a formula column, a .wet binding and an agent's arithmetic
 * all need this, and none of them need a dependency.
 *
 * Grammar (recursive descent, right-associative power):
 *   expr    := term (('+'|'-') term)*
 *   term    := unary (('*'|'/'|'%') unary)*
 *   unary   := ('-'|'+') unary | power
 *   power   := primary ('^' unary)?
 *   primary := number | name | name '(' args ')' | '(' expr ')'
 */
#ifndef WUBU_EXPR_H
#define WUBU_EXPR_H

#include <stddef.h>

typedef struct {
    const char *name;
    double      value;
} WubuExprVar;

enum {
    WUBU_EXPR_OK     =  0,
    WUBU_EXPR_SYNTAX = -1,   /* malformed or unbalanced input */
    WUBU_EXPR_NAME   = -2,   /* unknown variable or function */
    WUBU_EXPR_DIV0   = -3,   /* division or modulo by zero */
    WUBU_EXPR_DOMAIN = -4,   /* result is not a real number (sqrt(-1), log(-1)) */
    WUBU_EXPR_ARGS   = -5,   /* wrong argument count or bad argument */
    WUBU_EXPR_DEPTH  = -6,   /* nesting deeper than the parser allows */
    WUBU_EXPR_RANGE  = -7    /* non-finite result (overflow) */
};

/* Evaluate `src`. On failure returns 0.0 and writes the reason to *err (may be NULL).
 * Variables are looked up by exact name; `pi` and `e` are built in. */
double wubu_expr_eval(const char *src, const WubuExprVar *vars, size_t nvars, int *err);

/* The failure reason as a stable, user-ready phrase. */
const char *wubu_expr_reason(int err);

#endif
