/*
 * bear_opt.h — PufferC/BearRL Optimizers (Adam + Muon)
 *
 * Pure C11, SIMD-accelerated updates.
 * Adam: standard adaptive moment estimation.
 * Muon: momentum with orthogonal updates (PufferLib).
 */

#ifndef BEAR_OPT_H
#define BEAR_OPT_H

#include "bear_arena.h"
#include "bear_nn.h"
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════
 * Optimizer Types
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    BEAR_OPT_SGD      = 0,
    BEAR_OPT_ADAM     = 1,
    BEAR_OPT_MUON     = 2,  /* MomentUm Orthogonal Normalized (PufferLib) */
    BEAR_OPT_ADAMW    = 3,
} BearOptType;

/* Adam state */
typedef struct {
    BearTensor m;    /* 1st moment */
    BearTensor v;    /* 2nd moment */
    int step;
} BearAdamState;

/* Muon state */
typedef struct {
    BearTensor momentum;
    float lr;
} BearMuonState;

/* Optimizer handle */
typedef struct {
    BearOptType type;
    float lr;
    float beta1;       /* Adam: 0.9 */
    float beta2;       /* Adam: 0.999 */
    float eps;         /* Adam: 1e-8 */
    float weight_decay;/* AdamW */
    float momentum;    /* SGD/Muon */
    float nesterov;    /* SGD */
    
    /* Per-parameter state arrays */
    BearAdamState* adam_states;
    BearMuonState* muon_states;
    int num_params;
    
    /* Global step counter */
    int step;
} BearOptimizer;

/* ═══════════════════════════════════════════════════════════════════
 * Optimizer Lifecycle
 * ═══════════════════════════════════════════════════════════════════ */

/* Create optimizer for a set of parameters */
BearOptimizer* bear_optimizer_create(BearArena* arena, BearOptType type, float lr);

/* Register parameter with optimizer (weight + grad pair) */
int bear_optimizer_register(BearOptimizer* opt, BearParam* param);

/* Step: update all registered parameters using their gradients */
void bear_optimizer_step(BearOptimizer* opt);

/* Zero all gradients of registered parameters */
void bear_optimizer_zero_grad(BearOptimizer* opt);

/* Learning rate scheduling */
void bear_optimizer_set_lr(BearOptimizer* opt, float lr);
float bear_optimizer_get_lr(const BearOptimizer* opt);

/* ═══════════════════════════════════════════════════════════════════
 * Adam / AdamW Update (SIMD where possible)
 * ═══════════════════════════════════════════════════════════════════ */

void bear_adam_update_param(BearParam* param, BearAdamState* state,
                             float lr, float beta1, float beta2, float eps,
                             float weight_decay, int step);

void bear_adamw_update_param(BearParam* param, BearAdamState* state,
                              float lr, float beta1, float beta2, float eps,
                              float weight_decay, int step);

/* ═══════════════════════════════════════════════════════════════════
 * Muon Update (PufferLib style)
 * 
 * Muon: Normalized momentum with orthogonalization
 *   m = beta * m + (1 - beta) * g
 *   m = m / ||m||_2  (per-row normalization for weight matrices)
 *   w = w - lr * m
 * ═══════════════════════════════════════════════════════════════════ */

void bear_muon_update_param(BearParam* param, BearMuonState* state,
                             float lr, float beta, float weight_decay);

#endif /* BEAR_OPT_H */