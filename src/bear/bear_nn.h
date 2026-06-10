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

    /* Forward-pass stored activations (for backward) */
    /* z_pre: [batch, out_features]  pre-activation  x @ W^T + b   */
    /* a_post: [batch, out_features] post-activation act(z_pre)   */
    /* For layer 0 the "input" (a_prev) is the observation tensor */
    BearTensor z_pre;
    BearTensor a_post;
    int act_storage;
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
    int fwd_stored;          /* flag: forward pass stored activations */
    /* Gaussian policy for continuous actions */
    float* logstd;           /* [act_dim] learned log-std (NULL if fixed) */
    float   logstd_fixed;    /* fixed logstd value when logstd==NULL */
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
    int fwd_stored;
} BearValueNet;

int bear_value_create(BearValueNet* vnet, BearArena* param_arena,
                       int obs_dim, const int* hid_sizes, int num_hid);

void bear_value_forward(const BearValueNet* vnet,
                         const BearTensor* obs,  /* [batch, obs_dim] */
                         BearTensor* values,     /* [batch] */
                         BearArena* temp_arena);

/* ═══════════════════════════════════════════════════════════════════
 * Backward Pass (analytical gradients)
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Policy backward: compute gradients for policy network.
 * Must be called AFTER bear_policy_forward (which stores activations).
 *
 * For discrete actions (categorical policy):
 *   dlogit[i] = (ratio_clipped[i] * adv[i]) / mb_size
 *   where ratio_clipped = clip(exp(new_lp - old_lp), 1-eps, 1+eps)
 *
 * For continuous actions (Gaussian policy):
 *   dmu[i] = (ratio_clipped[i] * adv[i]) / mb_size * (action[i] - mu[i]) / std^2
 *
 * obs:       [mb, obs_dim]  minibatch observations
 * actions:   [mb, act_dim]  one-hot chosen actions (discrete) or raw actions (continuous)
 * old_logprobs: [mb]
 * advantages:   [mb]  (already normalized)
 * clip_coef:   PPO clip epsilon
 * policy_grad_scale: additional scale factor (e.g. 1.0)
 *
 * Returns 0 on success.
 */
int bear_policy_backward(BearPolicyNet* net,
                          const BearTensor* obs,
                          const BearTensor* actions,
                          const BearTensor* old_logprobs,
                          const BearTensor* advantages,
                          float clip_coef,
                          float policy_grad_scale,
                          BearArena* temp_arena);

/* Internal: discrete backward */
int bear_policy_backward_discrete(BearPolicyNet* net,
                                    const BearTensor* obs,
                                    const BearTensor* actions,
                                    const BearTensor* old_logprobs,
                                    const BearTensor* advantages,
                                    float clip_coef,
                                    float policy_grad_scale,
                                    BearArena* temp_arena);

/* Internal: continuous (Gaussian) backward */
int bear_policy_backward_continuous(BearPolicyNet* net,
                                     const BearTensor* obs,
                                     const BearTensor* actions,
                                     const BearTensor* old_logprobs,
                                     const BearTensor* advantages,
                                     float clip_coef,
                                     float policy_grad_scale,
                                     BearArena* temp_arena);

/*
 * Value backward: compute gradients for value network.
 * Must be called AFTER bear_value_forward (which stores activations).
 *
 * Loss = 0.5 * (V - target)^2
 * dV = (V - target) / mb_size
 *
 * values:   [mb]  current value predictions
 * targets:  [mb]  returns (GAE + old_values)
 * vf_coef:  value loss coefficient
 */
int bear_value_backward(BearValueNet* vnet,
                         const BearTensor* obs,
                         const BearTensor* values,
                         const BearTensor* targets,
                         float vf_coef,
                         BearArena* temp_arena);

/* Zero all gradients in policy and value networks */
void bear_policy_zero_grad(BearPolicyNet* net);
void bear_value_zero_grad(BearValueNet* vnet);

/* ══════════════════════════════════════════════════════════════════
 * Utility: Xavier/Orthogonal Init, Checkpointing
 * ═══════════════════════════════════════════════════════════════════ */

void bear_orthogonal_init_params(BearPolicyNet* net, float gain);
void bear_value_orthogonal_init(BearValueNet* vnet, float gain);

int bear_checkpoint_save(const BearPolicyNet* net, const char* path);
int bear_checkpoint_load(BearPolicyNet* net, const char* path);

#endif /* BEAR_NN_H */