/*
 * bear_ppo_traj.c -- BearRL PPO trajectory buffers, GAE/V-Trace advantages,
 * and minibatch sampler (extracted from the monolithic bear_ppo.c).
 *
 * Self-contained: depends only on the public bear_ppo.h API. C11, no god headers.
 */

#include "bear_ppo.h"
#include "bear_arena.h"
#include "bear_nn.h"
#include "bear_opt.h"
#include "bear_simd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ===================================================================
 * Trajectory Buffer
 * =================================================================== */

int bear_traj_init(BearTrajectory* t, BearArena* arena,
                     int rollout_len, int num_envs, int max_agents,
                     int obs_dim, int act_dim, int act_discrete) {
    if (!t || !arena) return -1;
    memset(t, 0, sizeof(BearTrajectory));
    
    t->rollout_len = rollout_len;
    t->num_envs = num_envs;
    t->max_agents = max_agents;
    t->obs_dim = obs_dim;
    t->act_dim = act_dim;
    t->act_discrete = act_discrete;
    
    int B = num_envs * max_agents;
    
    /* [T, B, dim] */
    int64_t obs_shape[3] = { rollout_len, B, obs_dim };
    bear_tensor_create(arena, &t->obs, obs_shape, 3, BEAR_DTYPE_F32, "traj.obs");
    
    int64_t act_shape[3] = { rollout_len, B, act_dim };
    bear_tensor_create(arena, &t->actions, act_shape, 3, BEAR_DTYPE_F32, "traj.actions");
    
    int64_t scalar_shape[2] = { rollout_len, B };
    bear_tensor_create(arena, &t->logprobs, scalar_shape, 2, BEAR_DTYPE_F32, "traj.logprobs");
    bear_tensor_create(arena, &t->rewards, scalar_shape, 2, BEAR_DTYPE_F32, "traj.rewards");
    bear_tensor_create(arena, &t->dones, scalar_shape, 2, BEAR_DTYPE_U8, "traj.dones");
    bear_tensor_create(arena, &t->values, scalar_shape, 2, BEAR_DTYPE_F32, "traj.values");
    bear_tensor_create(arena, &t->advantages, scalar_shape, 2, BEAR_DTYPE_F32, "traj.advantages");
    bear_tensor_create(arena, &t->returns, scalar_shape, 2, BEAR_DTYPE_F32, "traj.returns");
    
    /* Running norm stats (for obs normalization) */
    t->obs_rms_mean = BEAR_ARENA_ALLOC(arena, float, obs_dim);
    t->obs_rms_var = BEAR_ARENA_ALLOC(arena, float, obs_dim);
    t->obs_rms_count = 0;
    for (int i = 0; i < obs_dim; ++i) { t->obs_rms_mean[i] = 0; t->obs_rms_var[i] = 1; }
    
    t->rew_rms_mean = BEAR_ARENA_ALLOC(arena, float, 1);
    t->rew_rms_var = BEAR_ARENA_ALLOC(arena, float, 1);
    t->rew_rms_count = 0;
    t->rew_rms_mean[0] = 0; t->rew_rms_var[0] = 1;
    
    return 0;
}

void bear_traj_reset(BearTrajectory* t) {
    if (!t) return;
    memset(t->obs.data, 0, bear_tensor_numel(&t->obs) * sizeof(float));
    memset(t->actions.data, 0, bear_tensor_numel(&t->actions) * sizeof(float));
    memset(t->logprobs.data, 0, bear_tensor_numel(&t->logprobs) * sizeof(float));
    memset(t->rewards.data, 0, bear_tensor_numel(&t->rewards) * sizeof(float));
    memset(t->dones.data, 0, bear_tensor_numel(&t->dones) * sizeof(uint8_t));
    memset(t->values.data, 0, bear_tensor_numel(&t->values) * sizeof(float));
    memset(t->advantages.data, 0, bear_tensor_numel(&t->advantages) * sizeof(float));
    memset(t->returns.data, 0, bear_tensor_numel(&t->returns) * sizeof(float));
}

void bear_traj_store(BearTrajectory* t, int step_idx,
                      const BearTensor* obs, const BearTensor* actions,
                      const BearTensor* logprobs, const BearTensor* rewards,
                      const BearTensor* dones, const BearTensor* values) {
    if (!t || step_idx < 0 || step_idx >= t->rollout_len) return;
    
    int B = t->num_envs * t->max_agents;
    int obs_offset = step_idx * B * t->obs_dim;
    int act_offset = step_idx * B * t->act_dim;
    int scalar_offset = step_idx * B;
    
    int obs_bytes = B * t->obs_dim * sizeof(float);
    int act_bytes = B * t->act_dim * sizeof(float);
    int scalar_bytes = B * sizeof(float);
    
    memcpy((uint8_t*)t->obs.data + obs_offset, obs->data, obs_bytes);
    memcpy((uint8_t*)t->actions.data + act_offset, actions->data, act_bytes);
    memcpy((uint8_t*)t->logprobs.data + scalar_offset, logprobs->data, scalar_bytes);
    memcpy((uint8_t*)t->rewards.data + scalar_offset, rewards->data, scalar_bytes);
    memcpy((uint8_t*)t->dones.data + scalar_offset, dones->data, B * sizeof(uint8_t));
    memcpy((uint8_t*)t->values.data + scalar_offset, values->data, scalar_bytes);
}

/* ===================================================================
 * Advantage Computation: GAE(λ) and V-Trace
 * =================================================================== */

void bear_compute_advantages(BearTrajectory* t, const BearPPOConfig* cfg,
                              BearArena* temp_arena) {
    int T = t->rollout_len;
    int B = t->num_envs * t->max_agents;
    
    if (cfg->use_vtrace) {
        /* V-Trace requires target policy logprobs - placeholder for now */
        /* Fall back to GAE */
    }
    
    /* GAE(λ) */
    float* rewards = (float*)t->rewards.data;
    uint8_t* dones = (uint8_t*)t->dones.data;
    float* values = (float*)t->values.data;
    float* advantages = (float*)t->advantages.data;
    float* returns = (float*)t->returns.data;
    
    for (int b = 0; b < B; ++b) {
        float gae = 0.0f;
        for (int t_idx = T - 1; t_idx >= 0; --t_idx) {
            int idx = t_idx * B + b;
            int next_idx = (t_idx + 1) * B + b;
            
            float reward = rewards[idx];
            float value = values[idx];
            float next_value = (t_idx == T - 1) ? 0.0f : values[next_idx];
            uint8_t done = dones[idx];
            
            float delta = reward + cfg->gamma * next_value * (1.0f - done) - value;
            gae = delta + cfg->gamma * cfg->gae_lambda * (1.0f - done) * gae;
            advantages[idx] = gae;
            returns[idx] = gae + value;
        }
    }
    
    /* Reward normalization */
    if (t->rew_rms_count > 0) {
        float rew_mean = t->rew_rms_mean[0];
        float rew_std = sqrtf(t->rew_rms_var[0] / t->rew_rms_count + 1e-8f);
        for (int i = 0; i < T * B; ++i) {
            rewards[i] = (rewards[i] - rew_mean) / rew_std;
        }
    }
}

void bear_vtrace_compute(const float* rewards, const uint8_t* dones,
                          const float* values, const float* logprobs,
                          const float* target_logprobs,
                          float* advantages, float* returns,
                          int T, int B, const BearPPOConfig* cfg) {
    float rho_bar = cfg->vtrace_rho;
    float c_bar = cfg->vtrace_c;
    
    for (int b = 0; b < B; ++b) {
        float vs_next = 0.0f;
        for (int t = T - 1; t >= 0; --t) {
            int idx = t * B + b;
            float rho = expf(target_logprobs[idx] - logprobs[idx]);
            rho = fminf(rho, rho_bar);
            float c = fminf(rho, c_bar);
            
            float delta = rho * (rewards[idx] + cfg->gamma * vs_next * (1.0f - dones[idx]) - values[idx]);
            float vs = delta + c * (values[idx] - vs_next);
            vs_next = vs;
            
            advantages[idx] = delta;
            returns[idx] = vs;
        }
    }
}

/* ===================================================================
 * Minibatch Sampler
 * =================================================================== */

void bear_sampler_init(BearMinibatchSampler* s, BearTrajectory* t, int minibatch_size,
                        uint64_t rng_state[2]) {
    int T = t->rollout_len;
    int B = t->num_envs * t->max_agents;
    int total = T * B;
    
    s->total_samples = total;
    s->minibatch_size = minibatch_size;
    s->num_minibatches = (total + minibatch_size - 1) / minibatch_size;
    s->cursor = 0;
    
    s->indices = malloc(total * sizeof(int));
    for (int i = 0; i < total; ++i) s->indices[i] = i;
    
    /* Fisher-Yates shuffle using rng_state */
    uint64_t state = rng_state[0] ^ rng_state[1];
    for (int i = total - 1; i > 0; --i) {
        state = state * 1103515245 + 12345;  /* LCG */
        int j = (int)(state % (i + 1));
        int tmp = s->indices[i];
        s->indices[i] = s->indices[j];
        s->indices[j] = tmp;
    }
}

int bear_sampler_next(BearMinibatchSampler* s, BearTrajectory* t,
                       BearTensor* mb_obs, BearTensor* mb_actions,
                       BearTensor* mb_logprobs, BearTensor* mb_advantages,
                       BearTensor* mb_returns, BearTensor* mb_values,
                       BearTensor* mb_old_logprobs,
                       BearArena* temp_arena) {
    if (s->cursor >= s->num_minibatches) return 0;
    
    int start = s->cursor * s->minibatch_size;
    int end = start + s->minibatch_size;
    if (end > s->total_samples) end = s->total_samples;
    int mb_size = end - start;
    
    int T = t->rollout_len;
    int B = t->num_envs * t->max_agents;
    int obs_dim = t->obs_dim;
    int act_dim = t->act_dim;
    
    /* Create minibatch tensors */
    int64_t obs_shape[2] = { mb_size, obs_dim };
    int64_t act_shape[2] = { mb_size, act_dim };
    int64_t scalar_shape[1] = { mb_size };
    
    bear_tensor_create(temp_arena, mb_obs, obs_shape, 2, BEAR_DTYPE_F32, "mb_obs");
    bear_tensor_create(temp_arena, mb_actions, act_shape, 2, BEAR_DTYPE_F32, "mb_actions");
    bear_tensor_create(temp_arena, mb_logprobs, scalar_shape, 1, BEAR_DTYPE_F32, "mb_logprobs");
    bear_tensor_create(temp_arena, mb_advantages, scalar_shape, 1, BEAR_DTYPE_F32, "mb_adv");
    bear_tensor_create(temp_arena, mb_returns, scalar_shape, 1, BEAR_DTYPE_F32, "mb_ret");
    bear_tensor_create(temp_arena, mb_values, scalar_shape, 1, BEAR_DTYPE_F32, "mb_val");
    bear_tensor_create(temp_arena, mb_old_logprobs, scalar_shape, 1, BEAR_DTYPE_F32, "mb_old_logp");
    
    float* mb_obs_p = (float*)mb_obs->data;
    float* mb_act_p = (float*)mb_actions->data;
    float* mb_lp_p = (float*)mb_logprobs->data;
    float* mb_adv_p = (float*)mb_advantages->data;
    float* mb_ret_p = (float*)mb_returns->data;
    float* mb_val_p = (float*)mb_values->data;
    float* mb_olp_p = (float*)mb_old_logprobs->data;
    
    float* obs_p = (float*)t->obs.data;
    float* act_p = (float*)t->actions.data;
    float* lp_p = (float*)t->logprobs.data;
    float* adv_p = (float*)t->advantages.data;
    float* ret_p = (float*)t->returns.data;
    float* val_p = (float*)t->values.data;
    
    for (int i = 0; i < mb_size; ++i) {
        int idx = s->indices[start + i];
        int t_idx = idx / B;
        int b_idx = idx % B;
        
        int obs_src = t_idx * B * t->obs_dim + b_idx * t->obs_dim;
        int act_src = t_idx * B * t->act_dim + b_idx * t->act_dim;
        int scalar_src = t_idx * B + b_idx;
        
        memcpy(mb_obs_p + i * t->obs_dim, obs_p + obs_src, t->obs_dim * sizeof(float));
        memcpy(mb_act_p + i * t->act_dim, act_p + act_src, t->act_dim * sizeof(float));
        mb_lp_p[i] = lp_p[scalar_src];
        mb_adv_p[i] = adv_p[scalar_src];
        mb_ret_p[i] = ret_p[scalar_src];
        mb_val_p[i] = val_p[scalar_src];
        mb_olp_p[i] = lp_p[scalar_src];
    }
    
    /* Normalize advantages per minibatch (skip if std ≈ 0) */
    {
        float mean = 0, std = 0;
        for (int i = 0; i < mb_size; ++i) mean += mb_adv_p[i];
        mean /= mb_size;
        for (int i = 0; i < mb_size; ++i) std += (mb_adv_p[i] - mean) * (mb_adv_p[i] - mean);
        std = sqrtf(std / mb_size);
        if (std > 1e-6f) {
            for (int i = 0; i < mb_size; ++i) mb_adv_p[i] = (mb_adv_p[i] - mean) / std;
        } else {
            /* All advantages are the same  --  set to 0 */
            for (int i = 0; i < mb_size; ++i) mb_adv_p[i] = 0.0f;
        }
    }
    
    s->cursor++;
    return 1;
}

/* ===================================================================
 * PPO Loss Computation
 * ==================================================================== */

