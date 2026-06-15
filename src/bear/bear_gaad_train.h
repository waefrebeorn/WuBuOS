/*
 * bear_gaad_train.h  --  GAAD Integration into BearRL PPO Training
 *
 * Replaces standard Adam with GAAD (TGT + anisotropic + resonant + Poincare)
 * in the PPO gradient step. Uses flat buffer bridge between per-layer params
 * and GAAD's contiguous arrays.
 */
#ifndef BEAR_GAAD_TRAIN_H
#define BEAR_GAAD_TRAIN_H

#include "bear_arena.h"
#include "bear_nn.h"
#include "bear_ppo.h"
#include "bear_gaad.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * GAAD-Aware Trainer Extension
 * =================================================================== */

typedef struct {
    /* GAAD optimizers (one for all policy weights, one for all critic) */
    BearGAADOptimizer* gaad_policy;
    BearGAADOptimizer* gaad_critic;
    
    /* Flat buffer for policy weights and gradients */
    float* flat_w_policy;       /* all policy weights concatenated [n_policy] */
    float* flat_g_policy;       /* all policy gradients concatenated [n_policy] */
    int    n_policy_params;     /* total number of policy weight scalars */
    
    /* Flat buffer for critic */
    float* flat_w_critic;
    float* flat_g_critic;
    int    n_critic_params;
    
    /* Staging arena for flat buffers */
    BearArena flat_arena;
} BearGAADTrainExt;

/* Initialize GAAD extension for a trainer */
int bear_gaad_train_init(BearGAADTrainExt* ext,
                          BearPolicyNet* policy, BearValueNet* critic,
                          const BearGAADConfig* cfg_policy,
                          const BearGAADConfig* cfg_critic,
                          BearArena* arena);

/* Apply gradients using GAAD (replaces bear_ppo_apply_gradients) */
void bear_gaad_apply_gradients(BearGAADTrainExt* ext,
                                BearPolicyNet* policy,
                                BearValueNet* critic);

/* Destroy GAAD extension (frees malloced anis/res buffers inside GAAD) */
void bear_gaad_train_destroy(BearGAADTrainExt* ext);

/* Count total parameters in a policy/value network (weights + biases) */
int bear_gaad_count_policy_params(const BearPolicyNet* policy);
int bear_gaad_count_critic_params(const BearValueNet* critic);

#ifdef __cplusplus
}
#endif

#endif /* BEAR_GAAD_TRAIN_H */
