/*
 * bear_ppo.h — PufferC/BearRL PPO Training Loop
 *
 * Full PPO with GAE(λ), VTrace, minibatches, CleanRL 37 details.
 * Pure C11, zero deps.
 */

#ifndef BEAR_PPO_H
#define BEAR_PPO_H

#include "bear_arena.h"
#include "bear_nn.h"
#include "bear_env.h"
#include "bear_opt.h"

/* ═══════════════════════════════════════════════════════════════════
 * PPO Hyperparameters (CleanRL-aligned defaults)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Learning */
    float lr;                    /* 3e-4 */
    int   epochs_per_iter;       /* 4-10 (CleanRL: 10 for Atari, 4 for Mujoco) */
    int   minibatch_size;        /* 256-4096 */
    
    /* GAE / V-Trace */
    float gamma;                 /* 0.99 */
    float gae_lambda;            /* 0.95 */
    int   use_vtrace;            /* 0=GAE, 1=V-Trace (PufferLib) */
    float vtrace_rho;            /* 1.0 = clamp ratio */
    float vtrace_c;              /* 1.0 = clamp ratio */
    
    /* Clipping */
    float clip_coef;             /* 0.2 (CleanRL) */
    float clip_coef_vf;          /* 0.2 for value clipping */
    
    /* Loss coefficients */
    float vf_coef;               /* 0.5 */
    float ent_coef;              /* 0.01 (CleanRL: 0.001 for discrete, 0.01 for continuous) */
    
    /* Normalization */
    int   normalize_adv;         /* 1 = per-minibatch norm */
    int   normalize_obs;         /* 1 = running norm */
    int   normalize_rewards;     /* 1 = running reward norm */
    
    /* Optimization */
    float max_grad_norm;         /* 0.5 (gradient clipping) */
    int   lr_anneal;             /* 1 = linear decay */
    float target_kl;             /* 0.0 = disabled, else early stop epoch */
    
    /* Architecture */
    int   share_actor_critic;    /* 0 = separate, 1 = shared backbone */
} BearPPOConfig;

static inline BearPPOConfig bear_ppo_default_config(void) {
    return (BearPPOConfig){
        .lr = 3e-4f,
        .epochs_per_iter = 4,
        .minibatch_size = 2048,
        .gamma = 0.99f,
        .gae_lambda = 0.95f,
        .use_vtrace = 0,
        .vtrace_rho = 1.0f,
        .vtrace_c = 1.0f,
        .clip_coef = 0.2f,
        .clip_coef_vf = 0.2f,
        .vf_coef = 0.5f,
        .ent_coef = 0.01f,
        .normalize_adv = 1,
        .normalize_obs = 1,
        .normalize_rewards = 0,
        .max_grad_norm = 0.5f,
        .lr_anneal = 1,
        .target_kl = 0.0f,
        .share_actor_critic = 0,
    };
}

/* ═══════════════════════════════════════════════════════════════════
 * Trajectory Buffers (SoA, pre-allocated per rollout)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    int     rollout_len;          /* steps per env per iter */
    int     num_envs;
    int     max_agents;
    int     obs_dim;
    int     act_dim;
    int     act_discrete;
    
    /* Stored trajectories [rollout_len, num_envs * max_agents, ...] */
    BearTensor obs;       /* [T, B, obs_dim] */
    BearTensor actions;   /* [T, B, act_dim] */
    BearTensor logprobs;  /* [T, B] */
    BearTensor rewards;   /* [T, B] */
    BearTensor dones;     /* [T, B] (uint8) */
    BearTensor values;    /* [T, B] */
    BearTensor advantages;/* [T, B] */
    BearTensor returns;   /* [T, B] */
    
    /* Running normalization stats */
    float* obs_rms_mean;
    float* obs_rms_var;
    int    obs_rms_count;
    
    float* rew_rms_mean;
    float* rew_rms_var;
    int    rew_rms_count;
} BearTrajectory;

/* Initialize trajectory buffer from arena */
int bear_traj_init(BearTrajectory* t, BearArena* arena,
                     int rollout_len, int num_envs, int max_agents,
                     int obs_dim, int act_dim, int act_discrete);

/* Reset trajectory (zero counters, keep allocation) */
void bear_traj_reset(BearTrajectory* t);

/* Store step data at index */
void bear_traj_store(BearTrajectory* t, int step_idx,
                      const BearTensor* obs, const BearTensor* actions,
                      const BearTensor* logprobs, const BearTensor* rewards,
                      const BearTensor* dones, const BearTensor* values);

/* ═══════════════════════════════════════════════════════════════════
 * Advantage Computation
 * ═══════════════════════════════════════════════════════════════════ */

/* Compute GAE(λ) or V-Trace advantages */
void bear_compute_advantages(BearTrajectory* t, const BearPPOConfig* cfg,
                              BearArena* temp_arena);

/* V-Trace implementation (PufferLib style) */
void bear_vtrace_compute(const float* rewards, const uint8_t* dones,
                          const float* values, const float* logprobs,
                          const float* target_logprobs,
                          float* advantages, float* returns,
                          int T, int B, const BearPPOConfig* cfg);

/* ═══════════════════════════════════════════════════════════════════
 * Minibatch Sampler
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    int total_samples;      /* T * B */
    int minibatch_size;
    int num_minibatches;
    int* indices;           /* shuffled indices, length total_samples */
    int cursor;             /* current minibatch */
} BearMinibatchSampler;

void bear_sampler_init(BearMinibatchSampler* s, BearTrajectory* t, int minibatch_size,
                        uint64_t rng_state[2]);

/* Get next minibatch (returns 1 if available, 0 if epoch done) */
int bear_sampler_next(BearMinibatchSampler* s, BearTrajectory* t,
                       BearTensor* mb_obs, BearTensor* mb_actions,
                       BearTensor* mb_logprobs, BearTensor* mb_advantages,
                       BearTensor* mb_returns, BearTensor* mb_values,
                       BearTensor* mb_old_logprobs,
                       BearArena* temp_arena);

/* ═══════════════════════════════════════════════════════════════════
 * PPO Loss & Update Step
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    float policy_loss;
    float value_loss;
    float entropy_loss;
    float total_loss;
    float approx_kl;
    float clip_frac;
    float lr;
} BearPPOLoss;

/* Compute PPO loss for minibatch */
BearPPOLoss bear_ppo_loss(const BearPolicyNet* policy, const BearValueNet* critic,
                           const BearTensor* obs, const BearTensor* actions,
                           const BearTensor* old_logprobs, const BearTensor* advantages,
                           const BearTensor* returns, const BearTensor* old_values,
                           const BearPPOConfig* cfg,
                           BearArena* temp_arena);

/* Apply gradients via optimizer */
void bear_ppo_update(BearPolicyNet* policy, BearValueNet* critic,
                      const BearPPOLoss* loss, BearOptimizer* opt);

/* ═══════════════════════════════════════════════════════════════════
 * Main Training Loop
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    BearPolicyNet* policy;
    BearValueNet*   critic;
    BearEnv*        env;
    BearPPOConfig   cfg;
    BearTrajectory  traj;
    BearOptimizer*  opt_policy;
    BearOptimizer*  opt_critic;
    BearArena       global_arena;
    BearArena       rollout_arena;
    BearArena       step_arena;       /* per-step temp arena (reset each env step) */
    int             total_steps;
    int             iteration;
    float           best_return;
} BearTrainer;

/* Initialize trainer */
int bear_trainer_init(BearTrainer* trainer,
                       BearPolicyNet* policy, BearValueNet* critic,
                       BearEnv* env, const BearPPOConfig* cfg,
                       size_t global_arena_cap, size_t rollout_arena_cap,
                       size_t step_arena_cap);

/* Single training iteration: collect -> compute -> update */
float bear_trainer_iter(BearTrainer* trainer, uint64_t rng_state[2]);

/* Save/load checkpoint */
int bear_trainer_save(const BearTrainer* trainer, const char* path);
int bear_trainer_load(BearTrainer* trainer, const char* path);

/* ═══════════════════════════════════════════════════════════════════
 * Callbacks / Logging
 * ═══════════════════════════════════════════════════════════════════ */

typedef void (*bear_log_fn)(int iter, float total_steps, float return_mean,
                             float policy_loss, float value_loss, float entropy,
                             float lr, void* user_data);

void bear_trainer_set_logger(BearTrainer* trainer, bear_log_fn fn, void* user_data);

#endif /* BEAR_PPO_H */