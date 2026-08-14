/*
 * wubu_mir_opt.h -- MIR optimizer passes.
 *
 * Five classical passes over the hourglass-neck IR:
 *   FOLD    -- constant folding (compile-time eval of binops on constants)
 *   STRENGTH -- strength reduction (mul/div -> shift, *1/+0/-0/*0 elim)
 *   DCE     -- dead code elimination (remove unused result vrs)
 *   LICM    -- loop-invariant code motion (hoist pure computations)
 *   UNROLL  -- loop unrolling (small constant trip counts)
 *
 * Each pass is a bit in mir_opt_flags_t; wubu_mir_optimize runs the
 * requested passes in a fixed order (fold, strength, dce, licm, unroll)
 * regardless of flag order, so later passes clean up after earlier ones.
 *
 * C11, self-contained.
 */
#ifndef WUBU_MIR_OPT_H
#define WUBU_MIR_OPT_H

#include "wubu_mir.h"

typedef enum {
    MIR_OPT_FOLD    = 1,   /* constant folding */
    MIR_OPT_STRENGTH = 2,  /* strength reduction */
    MIR_OPT_DCE     = 4,   /* dead code elimination */
    MIR_OPT_LICM    = 8,   /* loop-invariant code motion */
    MIR_OPT_UNROLL  = 16,  /* loop unrolling */
} mir_opt_flags_t;

/* Run all requested optimization passes on program p.
 * Passes execute in a canonical order so later passes consume the
 * results of earlier ones:
 *   1. FOLD (if set)
 *   2. STRENGTH (if set)
 *   3. DCE (if set)
 *   4. LICM (if set)
 *   5. UNROLL (if set)
 * Multiple passes of the same kind are NOT re-run; call again if you
 * want to iterate to a fixpoint. */
void wubu_mir_optimize(wubu_mir_prog_t *p, mir_opt_flags_t flags);

#endif /* WUBU_MIR_OPT_H */