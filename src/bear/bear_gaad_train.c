/*
 * bear_gaad_train.c  --  GAAD Integration Implementation
 *
 * Bridges per-layer neural net weights to flat GAAD optimizer arrays.
 * Matches the existing PPO training pattern: only WEIGHT tensors are
 * updated (bias gradients are not computed/accumulated).
 */
#include "bear_gaad_train.h"
#include "bear_arena.h"
#include <string.h>
#include <stdio.h>

/* ===================================================================
 * Parameter Counting (weights only  --  biases handled separately)
 * =================================================================== */

int bear_gaad_count_policy_params(const BearPolicyNet* policy) {
    if (!policy || !policy->layers) return 0;
    int total = 0;
    for (int i = 0; i < policy->num_layers; ++i) {
        BearParam* p = policy->layers[i].param;
        if (!p) continue;
        total += (int)bear_tensor_numel(&p->weight);
    }
    return total;
}

int bear_gaad_count_critic_params(const BearValueNet* critic) {
    if (!critic || !critic->layers) return 0;
    int total = 0;
    for (int i = 0; i < critic->num_layers; ++i) {
        BearParam* p = critic->layers[i].param;
        if (!p) continue;
        total += (int)bear_tensor_numel(&p->weight);
    }
    return total;
}

/* ===================================================================
 * Flat Buffer Packing (weights only  --  matches existing Adam optimizer)
 * =================================================================== */

static int pack_weights(const BearPolicyNet* net, float* flat_w, int count) {
    int offset = 0;
    /* Check if count matches expected total */
    for (int i = 0; i < net->num_layers && offset < count; ++i) {
        BearParam* p = net->layers[i].param;
        if (!p) continue;
        int nw = (int)bear_tensor_numel(&p->weight);
        if (offset + nw > count) break;
        memcpy(flat_w + offset, p->weight.data, nw * sizeof(float));
        offset += nw;
    }
    return offset;
}

static int pack_grads(const BearPolicyNet* net, float* flat_g, int count) {
    int offset = 0;
    for (int i = 0; i < net->num_layers && offset < count; ++i) {
        BearParam* p = net->layers[i].param;
        if (!p || !p->grad.data) continue;
        int nw = (int)bear_tensor_numel(&p->weight);
        if (offset + nw > count) break;
        memcpy(flat_g + offset, p->grad.data, nw * sizeof(float));
        offset += nw;
    }
    return offset;
}

static int scatter_weights(BearPolicyNet* net, const float* flat_w, int count) {
    int offset = 0;
    for (int i = 0; i < net->num_layers && offset < count; ++i) {
        BearParam* p = net->layers[i].param;
        if (!p) continue;
        int nw = (int)bear_tensor_numel(&p->weight);
        if (offset + nw > count) break;
        memcpy(p->weight.data, flat_w + offset, nw * sizeof(float));
        offset += nw;
    }
    return offset;
}

static int pack_value_weights(const BearValueNet* net, float* flat_w, int count) {
    int offset = 0;
    for (int i = 0; i < net->num_layers && offset < count; ++i) {
        BearParam* p = net->layers[i].param;
        if (!p) continue;
        int nw = (int)bear_tensor_numel(&p->weight);
        if (offset + nw > count) break;
        memcpy(flat_w + offset, p->weight.data, nw * sizeof(float));
        offset += nw;
    }
    return offset;
}

static int pack_value_grads(const BearValueNet* net, float* flat_g, int count) {
    int offset = 0;
    for (int i = 0; i < net->num_layers && offset < count; ++i) {
        BearParam* p = net->layers[i].param;
        if (!p || !p->grad.data) continue;
        int nw = (int)bear_tensor_numel(&p->weight);
        if (offset + nw > count) break;
        memcpy(flat_g + offset, p->grad.data, nw * sizeof(float));
        offset += nw;
    }
    return offset;
}

static int scatter_value_weights(BearValueNet* net, const float* flat_w, int count) {
    int offset = 0;
    for (int i = 0; i < net->num_layers && offset < count; ++i) {
        BearParam* p = net->layers[i].param;
        if (!p) continue;
        int nw = (int)bear_tensor_numel(&p->weight);
        if (offset + nw > count) break;
        memcpy(p->weight.data, flat_w + offset, nw * sizeof(float));
        offset += nw;
    }
    return offset;
}

/* ===================================================================
 * Public API
 * =================================================================== */

int bear_gaad_train_init(BearGAADTrainExt* ext,
                          BearPolicyNet* policy, BearValueNet* critic,
                          const BearGAADConfig* cfg_policy,
                          const BearGAADConfig* cfg_critic,
                          BearArena* arena) {
    if (!ext || !policy || !critic || !cfg_policy || !cfg_critic || !arena) return -1;
    memset(ext, 0, sizeof(BearGAADTrainExt));
    
    ext->n_policy_params = bear_gaad_count_policy_params(policy);
    ext->n_critic_params = bear_gaad_count_critic_params(critic);
    
    if (ext->n_policy_params <= 0 || ext->n_critic_params <= 0) return -1;
    
    /* Allocate flat buffer arena */
    size_t flat_sz = (size_t)(ext->n_policy_params + ext->n_critic_params) * 2 * sizeof(float) + 65536;
    if (bear_arena_create(&ext->flat_arena, flat_sz) != 0) return -1;
    
    ext->flat_w_policy = (float*)BEAR_ARENA_ALLOC(&ext->flat_arena, float, ext->n_policy_params);
    ext->flat_g_policy = (float*)BEAR_ARENA_ALLOC(&ext->flat_arena, float, ext->n_policy_params);
    ext->flat_w_critic = (float*)BEAR_ARENA_ALLOC(&ext->flat_arena, float, ext->n_critic_params);
    ext->flat_g_critic = (float*)BEAR_ARENA_ALLOC(&ext->flat_arena, float, ext->n_critic_params);
    
    if (!ext->flat_w_policy || !ext->flat_g_policy || 
        !ext->flat_w_critic || !ext->flat_g_critic) return -1;
    
    /* Pack initial weights */
    pack_weights(policy, ext->flat_w_policy, ext->n_policy_params);
    pack_value_weights(critic, ext->flat_w_critic, ext->n_critic_params);
    
    /* Create GAAD optimizers (flat array version) */
    ext->gaad_policy = bear_gaad_create(&ext->flat_arena, cfg_policy, ext->n_policy_params);
    ext->gaad_critic = bear_gaad_create(&ext->flat_arena, cfg_critic, ext->n_critic_params);
    
    if (!ext->gaad_policy || !ext->gaad_critic) return -1;
    
    printf("[GAAD] Policy params: %d, Critic params: %d\n", 
           ext->n_policy_params, ext->n_critic_params);
    fflush(stdout);
    
    return 0;
}

void bear_gaad_apply_gradients(BearGAADTrainExt* ext,
                                BearPolicyNet* policy,
                                BearValueNet* critic) {
    if (!ext || !ext->gaad_policy || !ext->gaad_critic) return;
    
    /* Pack policy gradients into flat buffer */
    pack_grads(policy, ext->flat_g_policy, ext->n_policy_params);
    
    /* GAAD step on policy weights (does TGT + anisotropic φ-tiling + resonant + Adam) */
    bear_gaad_step(ext->gaad_policy, 
                    ext->flat_w_policy, 
                    ext->flat_g_policy, 
                    ext->n_policy_params, 
                    NULL);
    
    /* Scatter updated policy weights back to per-layer */
    scatter_weights(policy, ext->flat_w_policy, ext->n_policy_params);
    
    /* Pack critic gradients */
    pack_value_grads(critic, ext->flat_g_critic, ext->n_critic_params);
    
    /* GAAD step on critic weights */
    bear_gaad_step(ext->gaad_critic,
                    ext->flat_w_critic,
                    ext->flat_g_critic,
                    ext->n_critic_params,
                    NULL);
    
    /* Scatter updated critic weights */
    scatter_value_weights(critic, ext->flat_w_critic, ext->n_critic_params);
}

void bear_gaad_train_destroy(BearGAADTrainExt* ext) {
    if (!ext) return;
    if (ext->gaad_policy) bear_gaad_destroy(ext->gaad_policy);
    if (ext->gaad_critic) bear_gaad_destroy(ext->gaad_critic);
    bear_arena_destroy(&ext->flat_arena);
    memset(ext, 0, sizeof(BearGAADTrainExt));
}
