/* wubu_expr.c -- see wubu_expr.h. One parser, one evaluator, no allocation. */
#include "wubu_expr.h"
#include <math.h>
#include <stdlib.h>   /* strtod */
#include <string.h>
#include <ctype.h>

#define MAX_DEPTH 64

typedef struct {
    const char    *p;
    const WubuExprVar *vars;
    size_t         nvars;
    int            err;
} P;

static double expr(P *s);

static void skip(P *s) { while (*s->p == ' ' || *s->p == '\t') s->p++; }

static int is_name_start(int c) { return isalpha(c) || c == '_'; }
static int is_name_char(int c)  { return isalnum(c) || c == '_'; }

static int lookup_var(P *s, const char *name, size_t len, double *out) {
    if (len == 2 && !strncmp(name, "pi", 2)) { *out = 3.14159265358979323846; return 1; }
    if (len == 1 && name[0] == 'e')          { *out = 2.71828182845904523536; return 1; }
    for (size_t i = 0; i < s->nvars; i++) {
        const char *v = s->vars[i].name;
        if (v && strlen(v) == len && !strncmp(v, name, len)) { *out = s->vars[i].value; return 1; }
    }
    return 0;
}

static int arity(const char *f, size_t n) {
    struct { const char *n; int a; } t[] = {
        {"sin",1},{"cos",1},{"tan",1},{"asin",1},{"acos",1},{"atan",1},
        {"sqrt",1},{"abs",1},{"floor",1},{"ceil",1},{"round",1},
        {"exp",1},{"log",1},{"log2",1},{"log10",1},{"sinh",1},{"cosh",1},{"tanh",1},
        {"pow",2},{"atan2",2},{"min",2},{"max",2},{"hypot",2},{"clamp",3},
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++)
        if (strlen(t[i].n) == n && !strncmp(t[i].n, f, n)) return t[i].a;
    return -1;
}

static double call(const char *f, size_t n, double a, double b, double c, int *err) {
    if (n == 3 && !strncmp(f, "sin", 3)) return sin(a);
    if (n == 3 && !strncmp(f, "cos", 3)) return cos(a);
    if (n == 3 && !strncmp(f, "tan", 3)) return tan(a);
    if (n == 3 && !strncmp(f, "abs", 3)) return fabs(a);
    if (n == 3 && !strncmp(f, "exp", 3)) return exp(a);
    if (n == 3 && !strncmp(f, "log", 3)) { if (a <= 0) { *err = WUBU_EXPR_DOMAIN; return 0; } return log(a); }
    if (n == 3 && !strncmp(f, "pow", 3)) return pow(a, b);
    if (n == 3 && !strncmp(f, "min", 3)) return a < b ? a : b;
    if (n == 3 && !strncmp(f, "max", 3)) return a > b ? a : b;
    if (n == 4 && !strncmp(f, "sqrt", 4)) { if (a < 0) { *err = WUBU_EXPR_DOMAIN; return 0; } return sqrt(a); }
    if (n == 4 && !strncmp(f, "asin", 4)) { if (a < -1 || a > 1) { *err = WUBU_EXPR_DOMAIN; return 0; } return asin(a); }
    if (n == 4 && !strncmp(f, "acos", 4)) { if (a < -1 || a > 1) { *err = WUBU_EXPR_DOMAIN; return 0; } return acos(a); }
    if (n == 4 && !strncmp(f, "atan", 4)) return atan(a);
    if (n == 4 && !strncmp(f, "log2", 4)) { if (a <= 0) { *err = WUBU_EXPR_DOMAIN; return 0; } return log2(a); }
    if (n == 4 && !strncmp(f, "sinh", 4)) return sinh(a);
    if (n == 4 && !strncmp(f, "cosh", 4)) return cosh(a);
    if (n == 4 && !strncmp(f, "tanh", 4)) return tanh(a);
    if (n == 5 && !strncmp(f, "floor", 5)) return floor(a);
    if (n == 5 && !strncmp(f, "round", 5)) return round(a);
    if (n == 5 && !strncmp(f, "atan2", 5)) return atan2(a, b);
    if (n == 5 && !strncmp(f, "hypot", 5)) return hypot(a, b);
    if (n == 5 && !strncmp(f, "clamp", 5)) return a < b ? b : (a > c ? c : a);
    if (n == 5 && !strncmp(f, "log10", 5)) { if (a <= 0) { *err = WUBU_EXPR_DOMAIN; return 0; } return log10(a); }
    if (n == 4 && !strncmp(f, "ceil", 4)) return ceil(a);
    *err = WUBU_EXPR_NAME;
    return 0;
}

static double primary(P *s, int depth) {
    if (depth > MAX_DEPTH) { s->err = WUBU_EXPR_DEPTH; return 0; }
    skip(s);
    if (*s->p == '(') {
        s->p++;
        double v = expr(s);
        skip(s);
        if (*s->p != ')') { if (!s->err) s->err = WUBU_EXPR_SYNTAX; return 0; }
        s->p++;
        return v;
    }
    if (isdigit((unsigned char)*s->p) || *s->p == '.') {
        const char *start = s->p;
        while (isdigit((unsigned char)*s->p)) s->p++;
        if (*s->p == '.') { s->p++; while (isdigit((unsigned char)*s->p)) s->p++; }
        if (*s->p == 'e' || *s->p == 'E') {
            const char *save = s->p;
            s->p++;
            if (*s->p == '+' || *s->p == '-') s->p++;
            if (!isdigit((unsigned char)*s->p)) { s->p = save; }
            else while (isdigit((unsigned char)*s->p)) s->p++;
        }
        char buf[64];
        size_t n = (size_t)(s->p - start);
        if (n >= sizeof buf) { s->err = WUBU_EXPR_RANGE; return 0; }
        memcpy(buf, start, n); buf[n] = '\0';
        return strtod(buf, NULL);
    }
    if (is_name_start((unsigned char)*s->p)) {
        const char *start = s->p;
        while (is_name_char((unsigned char)*s->p)) s->p++;
        size_t len = (size_t)(s->p - start);
        skip(s);
        if (*s->p == '(') {
            int want = arity(start, len);
            char fname[32];
            if (want < 0 || len >= sizeof fname) { s->err = WUBU_EXPR_NAME; return 0; }
            memcpy(fname, start, len); fname[len] = '\0';
            s->p++;
            double a = 0, b = 0, c = 0;
            a = expr(s);
            skip(s);
            if (want >= 2) {
                if (*s->p != ',') { if (!s->err) s->err = WUBU_EXPR_ARGS; return 0; }
                s->p++; b = expr(s); skip(s);
            }
            if (want >= 3) {
                if (*s->p != ',') { if (!s->err) s->err = WUBU_EXPR_ARGS; return 0; }
                s->p++; c = expr(s); skip(s);
            }
            if (*s->p != ')') { if (!s->err) s->err = WUBU_EXPR_ARGS; return 0; }
            s->p++;
            if (s->err) return 0;
            return call(fname, len, a, b, c, &s->err);
        }
        double v = 0;
        if (!lookup_var(s, start, len, &v)) { s->err = WUBU_EXPR_NAME; return 0; }
        return v;
    }
    s->err = WUBU_EXPR_SYNTAX;
    return 0;
}

static double power(P *s, int depth) {
    double base = primary(s, depth + 1);
    skip(s);
    if (*s->p == '^') {
        if (s->err) return 0;
        s->p++;
        double e = power(s, depth + 1);      /* right-associative */
        if (s->err) return 0;
        double r = pow(base, e);
        if (isnan(r)) { s->err = WUBU_EXPR_DOMAIN; return 0; }
        return r;
    }
    return base;
}

static double unary(P *s, int depth) {
    skip(s);
    if (*s->p == '-') { s->p++; return -unary(s, depth + 1); }
    if (*s->p == '+') { s->p++; return  unary(s, depth + 1); }
    return power(s, depth + 1);
}

static double term(P *s) {
    double v = unary(s, 0);
    for (;;) {
        skip(s);
        int op = *s->p;
        if (op != '*' && op != '/' && op != '%') return v;
        s->p++;
        double r = unary(s, 0);
        if (s->err) return 0;
        if ((op == '/' || op == '%') && r == 0.0) { s->err = WUBU_EXPR_DIV0; return 0; }
        if (op == '*') v *= r;
        else if (op == '/') v /= r;
        else v = fmod(v, r);
    }
}

static double expr(P *s) {
    double v = term(s);
    for (;;) {
        skip(s);
        int op = *s->p;
        if (op != '+' && op != '-') return v;
        s->p++;
        double r = term(s);
        if (s->err) return 0;
        v = (op == '+') ? v + r : v - r;
    }
}

double wubu_expr_eval(const char *src, const WubuExprVar *vars, size_t nvars, int *err) {
    if (err) *err = WUBU_EXPR_OK;
    if (!src) { if (err) *err = WUBU_EXPR_SYNTAX; return 0; }
    P s = { src, vars, nvars, WUBU_EXPR_OK };
    skip(&s);
    if (!*s.p) { if (err) *err = WUBU_EXPR_SYNTAX; return 0; }
    double v = expr(&s);
    skip(&s);
    if (!s.err && *s.p) s.err = WUBU_EXPR_SYNTAX;   /* trailing garbage */
    if (!s.err && !isfinite(v)) s.err = WUBU_EXPR_RANGE;
    if (s.err) { if (err) *err = s.err; return 0; }
    return v;
}

const char *wubu_expr_reason(int err) {
    switch (err) {
        case WUBU_EXPR_OK:     return "ok";
        case WUBU_EXPR_SYNTAX: return "that expression is not complete";
        case WUBU_EXPR_NAME:   return "unknown name";
        case WUBU_EXPR_DIV0:   return "division by zero";
        case WUBU_EXPR_DOMAIN: return "no real result";
        case WUBU_EXPR_ARGS:   return "wrong number of arguments";
        case WUBU_EXPR_DEPTH:  return "nested too deeply";
        default:               return "invalid";
    }
}
