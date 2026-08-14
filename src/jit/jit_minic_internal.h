/* jit_minic_internal.h — internal linkage for Mini-C compiler modules.
 * Include ONLY from jit_minic_*.c files. Not part of public API. */
#ifndef JIT_MINIC_INTERNAL_H
#define JIT_MINIC_INTERNAL_H

#include "jit.h"
#include "jit_internal.h"
#include "wubu_x86.h"
#include "x86_regalloc.h"
#include "jit_branch_profile.h"
#include "jit_minic_loop.h"

/* -- Scope / Variable -------------------------------------------- */
#define MINIC_MAX_VARS 64

typedef struct {
    char    name[64];
    int     slot;
    int     is_arg;
    int     mty;
} MinicVar;

typedef struct {
    MinicVar    vars[MINIC_MAX_VARS];
    int         var_count;
    int         stack_offset;
} MinicScope;

/* -- Compiler state ---------------------------------------------- */
struct MinicCompiler {
    MinicLexer    lex;
    MinicScope    scope;
    Wx86Enc       enc;       /* Uses Wx86Enc from wubu_x86.h */
    int           n_args;
    int           in_func;
    int           error;
    char          error_msg[256];
    /* Linear-scan register allocator state */
    XRARegAlloc    ra;
    int            next_vreg;
    bool           use_xra;  /* true: use x86_regalloc instead of fixed rax+push/pop */
    /* Constant tracking: true when the current RAX holds a known constant.
     * Lets the allocator rematerialize instead of spilling to memory. */
    bool           rax_is_const;
    int64_t        rax_const_val;
    /* Compare-flag fusion (#2/#4): set when the last compiled expression was
     * a comparison (cmp reg,reg; setcc). The flags from the cmp are still live
     * until the next flag-writing instruction, so if/while can branch on the
     * compare's own cc instead of re-deriving via setcc+test. */
    bool           last_was_compare;
    Wx86CC         last_compare_cc;   /* cc of the comparison result */
    bool           last_compare_const0; /* RHS was constant 0 (used test) */
    size_t         cmp_after_pos;     /* encoder pos right after the cmp/test */
    /* Subsystem A: type system (structs/arrays/pointers) with #19 reorder */
    MinicTypeRegistry types;
    int              cur_expr_type;   /* type index of the current expression */
    /* Subsystem C: runtime branch feedback (#14/#23/#24). When WUBU_JIT_PGO=1,
     * the compiler emits a counter increment before each conditional jcc so
     * the runtime can record taken/not-taken for #14 layout decisions. The
     * counters are bump-only int64s the runtime owns; this layer just assigns
     * each branch a unique id (0..N-1) for the counter array. */
    int              n_branches;      /* # of conditional branches emitted */
    bool             profile_enabled; /* set from WUBU_JIT_PGO env at init   */
    /* Fast-path prologue: when true, skip frame setup (no locals, <=6 args).
     * Args stay in registers, return emits plain ret. */
    bool             need_frame;      /* 1 = full frame, 0 = fast path      */
    /* Subsystem B: loop-body capture. When compiling a while-body, record
     * each assignment statement into a LoopBody so the loop analysis engine
     * can detect induction variables and trip counts. */
    bool             capture_loop;    /* true while compiling a while-body */
    LoopBody         loop_body;       /* captured assignments for analysis */
    /* Subsystem B: analysis result for the innermost loop being compiled.
     * Populated after the body is compiled, before the back-edge jmp. */
    int64_t          loop_trip_count; /* closed-form trip count, or -1 */
    char             loop_iv[64];     /* induction variable name, or "" */
    int64_t          loop_iv_stride;  /* IV stride (+1, -1, etc.) */
    int              loop_n_invariants; /* # of loop-invariant expressions */
    /* #14 block-layout: when true, swap if/else block order so the hot path
     * falls through the jcc (taken-fallthrough layout). */
    bool             layout_swap;     /* swap if/else for hot-path fallthrough */
    int64_t          branch_taken_count; /* runtime count for this branch (PGO) */
    int64_t          branch_not_taken_count;
};
typedef struct MinicCompiler MinicCompiler;


/* -- Scope helpers ------------------------------------------------ */
void     scope_init(MinicScope *s);
MinicVar *scope_find(MinicScope *s, const char *name);
MinicVar *scope_add_local(MinicScope *s, const char *name);
MinicVar *scope_add_arg(MinicScope *s, const char *name, int arg_idx);
Wx86Reg  arg_reg(int idx);

/* -- Error helper (defined in jit_minic.c, used by expr.c) ------ */
void mc_error(MinicCompiler *mc, const char *msg);

/* -- Expression compiler chain ------------------------------------ */
void compile_expr(MinicCompiler *mc);
void compile_compare(MinicCompiler *mc);
void compile_bitwise_or(MinicCompiler *mc);
void compile_bitwise_xor(MinicCompiler *mc);
void compile_bitwise_and(MinicCompiler *mc);
void compile_shift(MinicCompiler *mc);
void compile_additive(MinicCompiler *mc);
void compile_multiplicative(MinicCompiler *mc);
void compile_primary(MinicCompiler *mc);



/* -- Emit helper macro (shared across modules) -------------------- */
#ifndef MC_EMIT
#define MC_EMIT(mc, call) do { if (!(mc)->error) { call; } } while(0)
#endif

#endif /* JIT_MINIC_INTERNAL_H */
