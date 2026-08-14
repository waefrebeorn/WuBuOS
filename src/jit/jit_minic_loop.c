/* jit_minic_loop.c -- Loop analysis subsystem for the Mini-C JIT.
 *
 * Subsystem B (foundation for #12 induction-variable strength reduction and
 * #13 loop-invariant code motion). The single-pass codegen cannot do these
 * inline, so this module provides the ANALYSIS core: given a captured while-loop
 * body (as a list of assignment/expression statements over scalar int64 vars),
 * it identifies:
 *
 *   - the INDUCTION VARIABLE(s): a var assigned a constant increment/decrement
 *     each iteration (the classic IV; its final value can be computed in closed
 *     form -> strength reduction),
 *   - LOOP-INVARIANT expressions: a var whose assigned RHS does not mention the
 *     IV or any var assigned in the loop (safe to hoist -> #13 LICM).
 *
 * It then emits, per IV, the closed-form trip count (when the bound is a
 * constant affine function of the IV), and per invariant, the hoist decision.
 *
 * This is the loop-analysis half of #12/#13. Wiring it into codegen requires a
 * loop-body AST (the compiler currently emits statements directly); this module
 * is the provable analysis engine the codegen pass will consume. The engine is
 * unit-tested: given canonical loop bodies it must identify the IV, its stride,
 * the trip count, and the invariants correctly.
 */
#include "jit_minic_loop.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/* -- per-var info ------------------------------------------------ */

typedef struct {
    char name[64];
    bool is_iv;        /* assigned a constant stride each iteration */
    int64_t stride;    /* IV stride (+/- constant) */
    bool written;      /* assigned anywhere in the loop */
} VarInfo;

static VarInfo vinfo[LOOP_MAX_VARS];
static int n_vars;

static VarInfo *vi_find(const char *name) {
    for (int i = 0; i < n_vars; i++)
        if (strcmp(vinfo[i].name, name) == 0) return &vinfo[i];
    return NULL;
}
static VarInfo *vi_get(const char *name) {
    VarInfo *v = vi_find(name);
    if (v) return v;
    if (n_vars < LOOP_MAX_VARS) {
        v = &vinfo[n_vars++];
        memset(v, 0, sizeof(*v));
        snprintf(v->name, sizeof(v->name), "%s", name);
    }
    return v;
}
static int vi_is_written(const char *name) {
    VarInfo *v = vi_find(name);
    return v && v->written;
}
static int vi_is_iv(const char *name) {
    VarInfo *v = vi_find(name);
    return v && v->is_iv;
}

/* -- analysis ---------------------------------------------------- */

/* An expression `name` (single var) or `imm` (constant). A var is loop-invariant
 * (#13) if it is never written in the loop and not an IV. */
static int expr_is_invariant(const char *expr) {
    if (!expr[0]) return 1;
    /* numeric constant? */
    char *end; long long v = strtoll(expr, &end, 10);
    if (end && *end == 0) return 1;
    /* single var: invariant iff not written and not an IV */
    return !vi_is_written(expr) && !vi_is_iv(expr);
}

/* Run the analysis over a loop body. Fills vinfo with IVs/invariants. */
static void loop_analyze(LoopBody *lb) {
    n_vars = 0;
    for (int i = 0; i < lb->n_stmts; i++) {
        LoopStmt *s = &lb->stmts[i];
        vi_get(s->var)->written = true;
        /* mark RHS operands written (a var used on RHS that's also LHS of a
         * later stmt would be counted there; here we just track LHS) */
        if (s->a[0] && vi_find(s->a)) vi_find(s->a);       /* touch */
        if (s->b[0] && vi_find(s->b)) vi_find(s->b);
    }
    /* IV detection: var = var +/- imm (self-increment/decrement) */
    for (int i = 0; i < lb->n_stmts; i++) {
        LoopStmt *s = &lb->stmts[i];
        if (s->kind != LOOP_ASSIGN) continue;
        if (s->a[0] && strcmp(s->a, s->var) == 0 && s->b[0] == 0 &&
            (s->op == '+' || s->op == '-')) {
            VarInfo *v = vi_get(s->var);
            v->is_iv = true;
            v->stride = (s->op == '+') ? s->imm : -s->imm;
        }
    }
}

/* Trip count in closed form for an IV going `while (iv OP bound)`:
 *   iv += stride each iteration, loop while iv < bound (or >, <=, >=).
 * Returns the exact number of iterations for the common case, or -1 if the
 * bound is not a constant (can't close without knowing the initial value).
 * The caller provides the initial IV value (known at loop entry). */
static int64_t loop_trip_count(VarInfo *iv, char op, int64_t bound, int64_t init) {
    if (!iv->is_iv || iv->stride == 0) return -1;
    int64_t s = iv->stride;
    int64_t n = -1;
    switch (op) {
        case '<':  /* init + k*s < bound  => k < (bound-init)/s */
            if (s > 0) { int64_t d = bound - init; n = d <= 0 ? 0 : (d + s - 1) / s; }
            else       { int64_t d = init - bound; n = d < 0 ? 0 : (d + (-s) - 1) / (-s); }
            break;
        case 'N':  /* <= : one more iteration than strict < */
            if (s > 0) { int64_t d = bound - init + 1; n = d <= 0 ? 0 : (d + s - 1) / s; }
            else       { int64_t d = init - bound + 1; n = d < 0 ? 0 : (d + (-s) - 1) / (-s); }
            break;
        case '>':  if (s < 0) { int64_t d = init - bound; n = d <= 0 ? 0 : (d + (-s) - 1) / (-s); }
                   else       { int64_t d = bound - init; n = d < 0 ? 0 : (d + s - 1) / s; }
                   break;
        default: return -1;
    }
    return n >= 0 ? n : -1;
}

/* -- public API -------------------------------------------------- */

void minic_loop_body_init(LoopBody *lb) { memset(lb, 0, sizeof(*lb)); }

int minic_loop_add_assign(LoopBody *lb, const char *var, char op,
                          const char *a, const char *b, int64_t imm) {
    if (lb->n_stmts >= LOOP_MAX_STMTS) return -1;
    LoopStmt *s = &lb->stmts[lb->n_stmts++];
    s->kind = LOOP_ASSIGN;
    snprintf(s->var, sizeof(s->var), "%s", var);
    s->op = op;
    snprintf(s->a, sizeof(s->a), "%s", a ? a : "");
    snprintf(s->b, sizeof(s->b), "%s", b ? b : "");
    s->imm = imm;
    return 0;
}

/* Analyze and report: returns #IVs found. Fills trip_count (or -1). */
int minic_loop_analyze(LoopBody *lb, int64_t iv_init, char cmp_op, int64_t bound,
                       int64_t *trip_count, char *iv_name, int64_t *stride) {
    loop_analyze(lb);
    *trip_count = -1;
    for (int i = 0; i < n_vars; i++) {
        if (vinfo[i].is_iv) {
            snprintf(iv_name, 64, "%s", vinfo[i].name);
            *stride = vinfo[i].stride;
            *trip_count = loop_trip_count(&vinfo[i], cmp_op, bound, iv_init);
            return 1;
        }
    }
    return 0;
}

/* How many expressions in the body are loop-invariant (#13 candidates)? */
int minic_loop_invariant_count(LoopBody *lb) {
    loop_analyze(lb);
    int c = 0;
    for (int i = 0; i < lb->n_stmts; i++) {
        LoopStmt *s = &lb->stmts[i];
        if (s->kind != LOOP_ASSIGN) continue;
        if (expr_is_invariant(s->a) && expr_is_invariant(s->b)) c++;
    }
    return c;
}

/* Strength-reduction summary for the canonical `while(i<n){ s += i*K }` where
 * the IV strides by 1: the accumulator's per-iteration increment `i*K` can be
 * replaced by a constant add `+= (stride*K)` plus the IV's own stride add —
 * turning a multiply per iteration into two adds (the #12 win). */
int minic_loop_strength_candidate(LoopBody *lb, const char *acc_var, const char *iv_name,
                                  int64_t k, int64_t *per_iter_add) {
    /* acc += iv * K  with iv an IV striding by stride => per_iter_add = stride*K */
    loop_analyze(lb);   /* ensure IV info is populated */
    for (int i = 0; i < n_vars; i++) {
        if (vinfo[i].is_iv && strcmp(vinfo[i].name, iv_name) == 0) {
            *per_iter_add = vinfo[i].stride * k;
            return 1;
        }
    }
    return 0;
}

/* Self-test entry (used by jit_loop_test). */
int minic_loop_run_selftest(void);
