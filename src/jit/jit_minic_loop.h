/* jit_minic_loop.h -- Loop analysis subsystem (Subsystem B).
 * Public types + API for the loop-analysis engine that underpins #12 (IV
 * strength reduction) and #13 (LICM). See jit_minic_loop.c for full docs. */
#ifndef JIT_MINIC_LOOP_H
#define JIT_MINIC_LOOP_H

#include <stdint.h>
#include <stdbool.h>

#define LOOP_MAX_STMTS 64
#define LOOP_MAX_VARS  32

typedef enum { LOOP_ASSIGN, LOOP_EXPR } LoopStmtKind;

typedef struct {
    LoopStmtKind kind;
    char  var[64];     /* LHS variable */
    char  a[64], b[64];/* RHS operands ("" if absent) */
    char  op;          /* '+','-','*','/','=', 0 if none */
    int64_t imm;       /* constant operand, if any */
} LoopStmt;

typedef struct {
    LoopStmt stmts[LOOP_MAX_STMTS];
    int n_stmts;
    int64_t bound;
    bool    has_bound;
} LoopBody;

void minic_loop_body_init(LoopBody *lb);
int  minic_loop_add_assign(LoopBody *lb, const char *var, char op,
                           const char *a, const char *b, int64_t imm);
int  minic_loop_analyze(LoopBody *lb, int64_t iv_init, char cmp_op,
                        int64_t bound, int64_t *trip_count,
                        char *iv_name, int64_t *stride);
int  minic_loop_invariant_count(LoopBody *lb);
int  minic_loop_strength_candidate(LoopBody *lb, const char *acc_var,
                                   const char *iv_name, int64_t k,
                                   int64_t *per_iter_add);

#endif /* JIT_MINIC_LOOP_H */
