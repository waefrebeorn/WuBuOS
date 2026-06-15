/* bear_holo_opt.h  --  Holographic Geodesic Optimizer (optax-compatible)
 * 
 * ByTropix Holographic Architecture:
 *   Gradient g = q.2π + r where q ∈ Z, r ∈ [-π, π]
 *   Soul (stored_topology) = Σq  --  exact integer accumulation (int32)
 *   Echo (stored_residue) = Σr  --  float64 precision accumulation
 *   Body = standard Adam updates on remainders
 *   Total gradient recovered: total = Soul.2π + Echo (bit-perfect)
 */

#ifndef BEAR_HOLO_OPT_H
#define BEAR_HOLO_OPT_H

#include "bear_arena.h"
#include <stdint.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Per-parameter state */
typedef struct {
    /* Soul: exact integer windings (never truncated) */
    int32_t soul;
    /* Echo: fractional remainder (float64 precision) */
    double echo;
    /* Momentum (Adam-style on remainders) */
    double m1;
    double m2;
} HoloParamState;

/* Configuration */
typedef struct {
    double base_lr;           /* Base learning rate */
    double boundary;          /* Decomposition boundary (default 2π) */
    double beta1;             /* Momentum beta1 (default 0.9) */
    double beta2;             /* Momentum beta2 (default 0.999) */
    double eps;               /* Adam epsilon (default 1e-8) */
    int use_holographic;      /* 1 = use soul/echo, 0 = standard Adam */
} BearHoloConfig;

static inline BearHoloConfig bear_holo_default_config(void) {
    return (BearHoloConfig){
        .base_lr = 1e-4,
        .boundary = 2.0 * M_PI,
        .beta1 = 0.9,
        .beta2 = 0.999,
        .eps = 1e-8,
        .use_holographic = 1,
    };
}

/* Holographic optimizer state for flat parameter array */
typedef struct {
    HoloParamState* states;   /* Array of per-parameter states */
    int n_params;             /* Number of parameters */
    BearHoloConfig cfg;       /* Configuration */
    uint64_t step_count;      /* Global step counter */
} BearHoloOptimizer;

/* Create optimizer */
BearHoloOptimizer* bear_holo_create(BearArena* arena, const BearHoloConfig* cfg, int n_params);

/* Reset optimizer state */
void bear_holo_reset(BearHoloOptimizer* opt);

/* Optimizer step: update weights using holographic decomposition
 * params: flat array of current weights
 * grads:  flat array of raw gradients
 * n:      number of elements
 * 
 * For each parameter i:
 *   (q, r) = decompose(grads[i]) where boundary = 2π
 *   states[i].soul   += q
 *   states[i].echo   += r  
 *   states[i].m1 = beta1 * m1 + (1-beta1) * r
 *   states[i].m2 = beta2 * m2 + (1-beta2) * r^2
 *   m1_hat = m1 / (1 - beta1^t), m2_hat = m2 / (1 - beta2^t)
 *   update = lr * m1_hat / (sqrt(m2_hat) + eps)
 *   params[i] -= update
 */
void bear_holo_step(BearHoloOptimizer* opt, double* params, const double* grads, int n);

/* Float-compatible wrapper for training code that uses float arrays */
void bear_holo_step_float(BearHoloOptimizer* opt, float* params, const float* grads, int n);

/* Debug: print optimizer state summary */
void bear_holo_debug(BearHoloOptimizer* opt);

/* Exact gradient recovery: total = soul * boundary + echo */
double bear_holo_recover_total(const HoloParamState* state, double boundary);

#endif