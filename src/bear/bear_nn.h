/*
 * bear_nn.h — PufferC/BearRL Neural Network: PolicyNet MLP + MinGRU
 *
 * Pure C11: MLP forward, recurrent (MinGRU), action sampling.
 * SIMD-accelerated via bear_simd.h.
 */

#ifndef BEAR_NN_H
#define BEAR_NN_H

#include "bear_arena.h"
#include "bear_simd.h"

/* ═══════════════════════════════════════════════════════════════════
 * Policy Network: Actor-Critic with optional recurrence
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    BEAR_NET_MLP   = 0,  /* Feedforward only */
    BEAR_NET_MINGU = 1,  /* MinGRU recurrent */
} BearNetType;

/* Layer configuration */
typedef struct {
    int in_features;
    int out_features;
    BearAct act;
    BearParam* param;  /* weight + bias + optimizer state */
} BearLayer;

/* Policy Network */
typedef struct {
    BearNetType type;
    int num_layers;
    BearLayer* layers;       /* array of num_layers */
    BearMinGRU* gru;         /* optional recurrent core */
    int obs_dim;
    int act_dim;
    int act_discrete;
    int hid_size;
    BearArena* param_arena;  /* where params live */
} BearPolicyNet;

/* Create MLP policy network */
int bear_policy_create_mlp(BearPolicyNet* net, BearArena* param_arena,
                            int obs_dim, int act_dim, int act_discrete,
                            const int* hid_sizes, int num_hid);

/* Create MinGRU policy network */
int bear_policy_create_mingru(BearPolicyNet* net, BearArena* param_arena,
                               int obs_dim, int act_dim, int act_discrete,
                               int hid_size);

/* Forward pass: obs -> (actions, logprobs, values, next_hidden) */
/* obs: [batch, obs_dim] or [batch, seq_len, obs_dim] for recurrent */
void bear_policy_forward(const BearPolicyNet* net,
                          const BearTensor* obs,        /* [batch, obs_dim] */
                          const BearTensor* h_in,       /* [batch, hid] (optional) */
                          BearTensor* actions,          /* [batch, act_dim] */
                          BearTensor* logprobs,         /* [batch] */
                          BearTensor* values,           /* [batch] */
                          BearTensor* h_out,            /* [batch, hid] (optional) */
                          BearArena* temp_arena);

/* Sample action from policy output (in-place) */
void bear_policy_sample(BearPolicyNet* net, BearTensor* actions, BearTensor* logprobs,
                         uint64_t rng_state[2]);

/* Get deterministic action (argmax for discrete, mean for continuous) */
void bear_policy_deterministic(BearPolicyNet* net, BearTensor* actions);

/* Get all parameters as flat array (for checkpointing) */
int bear_policy_get_params(const BearPolicyNet* net, float* out, int max_params);
int bear_policy_set_params(BearPolicyNet* net, const float* in, int num_params);

/* ═══════════════════════════════════════════════════════════════════
 * Value Network (separate critic optionally)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    int num_layers;
    BearLayer* layers;
    BearArena* param_arena;
} BearValueNet;

int bear_value_create(BearValueNet* vnet, BearArena* param_arena,
                       int obs_dim, const int* hid_sizes, int num_hid);

void bear_value_forward(const BearValueNet* vnet,
                         const BearTensor* obs,  /* [batch, obs_dim] */
                         BearTensor* values,     /* [batch] */
                         BearArena* temp_arena);

/* ══════════════════════════════════════════════════════════════════
 * Utility: Xavier/Orthogonal Init, Checkpointing
 * ═══════════════════════════════════════════════════════════════════ */

void bear_orthogonal_init_params(BearPolicyNet* net, float gain);
void bear_value_orthogonal_init(BearValueNet* vnet, float gain);

int bear_checkpoint_save(const BearPolicyNet* net, const char* path);
int bear_checkpoint_load(BearPolicyNet* net, const char* path);

#endif /* BEAR_NN_H */