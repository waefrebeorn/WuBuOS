/*
 * bear_ppo.c — PufferC/BearRL PPO Training Loop Implementation
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

/* ═══════════════════════════════════════════════════════════════════
 * Trajectory Buffer
 * ═══════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════
 * Advantage Computation: GAE(λ) and V-Trace
 * ═══════════════════════════════════════════════════════════════════ */

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

/* ═══════════════════════════════════════════════════════════════════
 * Minibatch Sampler
 * ═══════════════════════════════════════════════════════════════════ */

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
    
    /* Normalize advantages per minibatch */
    if (1 /* always normalize per CleanRL */) {
        float mean = 0, std = 0;
        for (int i = 0; i < mb_size; ++i) mean += mb_adv_p[i];
        mean /= mb_size;
        for (int i = 0; i < mb_size; ++i) std += (mb_adv_p[i] - mean) * (mb_adv_p[i] - mean);
        std = sqrtf(std / mb_size + 1e-8f);
        for (int i = 0; i < mb_size; ++i) mb_adv_p[i] = (mb_adv_p[i] - mean) / std;
    }
    
    s->cursor++;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * PPO Loss Computation
 * ════════════════════════════════════════════════════════════════════ */

BearPPOLoss bear_ppo_loss(const BearPolicyNet* policy, const BearValueNet* critic,
                          const BearTensor* obs, const BearTensor* actions,
                          const BearTensor* old_logprobs, const BearTensor* advantages,
                          const BearTensor* returns, const BearTensor* old_values,
                          const BearPPOConfig* cfg,
                          BearArena* temp_arena) {
    (void)critic;
    BearPPOLoss loss = {0};
    int batch = (int)obs->shape[0];
    int act_dim = policy->act_dim;
    int act_discrete = policy->act_discrete;
    
    /* Forward pass to get new logprobs, values, entropy */
    BearTensor new_actions, new_logprobs, new_values, h_out;
    int64_t act_shape[2] = { batch, act_dim };
    int64_t scalar_shape[1] = { batch };
    
    bear_tensor_create(temp_arena, &new_actions, act_shape, 2, BEAR_DTYPE_F32, "new_act");
    bear_tensor_create(temp_arena, &new_logprobs, scalar_shape, 1, BEAR_DTYPE_F32, "new_lp");
    bear_tensor_create(temp_arena, &new_values, scalar_shape, 1, BEAR_DTYPE_F32, "new_val");
    bear_tensor_create(temp_arena, &h_out, (int64_t[]){batch, 1}, 2, BEAR_DTYPE_F32, "h_out");
    
    bear_policy_forward(policy, obs, NULL, &new_actions, &new_logprobs, &new_values, &h_out, temp_arena);
    fprintf(stderr, "FWD OK batch=%d\n", batch);
    
    /* NaN check */
    float* nvp = (float*)new_values.data;
    float* nlp = (float*)new_logprobs.data;
    for (int i = 0; i < batch; ++i) {
        if (nvp[i] != nvp[i]) { fprintf(stderr, "NAN in new_values at i=%d\n", i); }
        if (nlp[i] != nlp[i]) { fprintf(stderr, "NAN in new_logprobs at i=%d\n", i); }
    }
    
    float* new_lp_p = (float*)new_logprobs.data;
    float* old_lp_p = (float*)old_logprobs->data;
    float* adv_p = (float*)advantages->data;
    float* ret_p = (float*)returns->data;
    float* old_v_p = (float*)old_values->data;
    float* new_v_p = (float*)new_values.data;
    
    /* Policy loss: clipped surrogate */
    float policy_loss = 0.0f;
    float clip_frac = 0.0f;
    float approx_kl = 0.0f;
    
    for (int i = 0; i < batch; ++i) {
        float ratio = expf(new_lp_p[i] - old_lp_p[i]);
        
        float clipped = fmaxf(fminf(ratio, 1.0f + cfg->clip_coef), 1.0f - cfg->clip_coef);
        float surr1 = ratio * adv_p[i];
        float surr2 = clipped * adv_p[i];
        policy_loss += -fminf(surr1, surr2);
        
        if (ratio > 1.0f + cfg->clip_coef || ratio < 1.0f - cfg->clip_coef)
            clip_frac += 1.0f;
        approx_kl += (ratio - 1.0f) - logf(ratio + 1e-8f);
    }
    policy_loss /= batch;
    clip_frac /= batch;
    approx_kl /= batch;
    
    /* Value loss: clipped MSE */
    float value_loss = 0.0f;
    for (int i = 0; i < batch; ++i) {
        float v_pred = new_v_p[i];
        float v_clipped = fmaxf(fminf(v_pred, old_v_p[i] + cfg->clip_coef_vf),
                                 old_v_p[i] - cfg->clip_coef_vf);
        float loss1 = (v_pred - ret_p[i]) * (v_pred - ret_p[i]);
        float loss2 = (v_clipped - ret_p[i]) * (v_clipped - ret_p[i]);
        value_loss += fmaxf(loss1, loss2);
    }
    value_loss = 0.5f * value_loss / batch;
    
    /* Entropy bonus */
    float entropy = 0.0f;
    if (act_discrete) {
        float* probs = (float*)actions->data;
        for (int i = 0; i < batch; ++i) {
            for (int a = 0; a < act_dim; ++a) {
                if (probs[i * act_dim + a] > 1e-8f)
                    entropy -= probs[i * act_dim + a] * logf(probs[i * act_dim + a] + 1e-8f);
            }
        }
        entropy /= batch;
    }
    
    loss.policy_loss = policy_loss;
    loss.value_loss = value_loss;
    loss.entropy_loss = entropy;
    loss.total_loss = policy_loss + cfg->vf_coef * value_loss - cfg->ent_coef * entropy;
    loss.approx_kl = approx_kl;
    loss.clip_frac = clip_frac;
    
    return loss;
}

void bear_ppo_update(BearPolicyNet* policy, BearValueNet* critic,
                      const BearPPOLoss* loss, BearOptimizer* opt) {
    /* Gradients already accumulated in bear_ppo_loss via autograd? 
     * This is a stub - in full impl we'd have backward pass here.
     * For now, optimizer step is called externally. */
    (void)policy; (void)critic; (void)loss; (void)opt;
}

/* ═══════════════════════════════════════════════════════════════════
 * Trainer Initialization
 * ═══════════════════════════════════════════════════════════════════ */

int bear_trainer_init(BearTrainer* trainer,
                       BearPolicyNet* policy, BearValueNet* critic,
                       BearEnv* env, const BearPPOConfig* cfg,
                       size_t global_arena_cap, size_t rollout_arena_cap,
                       size_t step_arena_cap) {
    fprintf(stderr, "TRAINER_INIT: start\n"); fflush(stderr);
    if (!trainer || !policy || !critic || !env || !cfg) return -1;
    
    memset(trainer, 0, sizeof(BearTrainer));
    fprintf(stderr, "TRAINER_INIT: memset done\n"); fflush(stderr);
    
    if (bear_arena_create(&trainer->global_arena, global_arena_cap) != 0) return -1;
    fprintf(stderr, "TRAINER_INIT: global arena created\n"); fflush(stderr);
    if (bear_arena_create(&trainer->rollout_arena, rollout_arena_cap) != 0) {
        bear_arena_destroy(&trainer->global_arena);
        return -1;
    }
    fprintf(stderr, "TRAINER_INIT: rollout arena created\n"); fflush(stderr);
    if (bear_arena_create(&trainer->step_arena, step_arena_cap) != 0) {
        bear_arena_destroy(&trainer->rollout_arena);
        bear_arena_destroy(&trainer->global_arena);
        return -1;
    }
    fprintf(stderr, "TRAINER_INIT: step arena created\n"); fflush(stderr);
    
    trainer->policy = policy;
    trainer->critic = critic;
    trainer->env = env;
    trainer->cfg = *cfg;
    
    fprintf(stderr, "TRAINER_INIT: about to traj_init\n"); fflush(stderr);
    if (bear_traj_init(&trainer->traj, &trainer->global_arena,
                        128, env->spec.num_envs, env->spec.max_agents,
                        env->spec.obs_dim, env->spec.act_dim, env->spec.act_discrete) != 0) {
        fprintf(stderr, "TRAINER_INIT: traj_init FAILED\n"); fflush(stderr);
        return -1;
    }
    fprintf(stderr, "TRAINER_INIT: traj_init done\n"); fflush(stderr);
    
    trainer->opt_policy = bear_optimizer_create(&trainer->global_arena, BEAR_OPT_ADAM, cfg->lr);
    trainer->opt_critic = bear_optimizer_create(&trainer->global_arena, BEAR_OPT_ADAM, cfg->lr);
    fprintf(stderr, "TRAINER_INIT: optimizers created\n"); fflush(stderr);
    
    return 0;
}

static bear_log_fn g_trainer_logger = NULL;
static void* g_trainer_log_ud = NULL;

void bear_trainer_set_logger(BearTrainer* trainer, bear_log_fn fn, void* user_data) {
    (void)trainer;
    g_trainer_logger = fn;
    g_trainer_log_ud = user_data;
}

float bear_trainer_iter(BearTrainer* trainer, uint64_t rng_state[2]) {
    if (!trainer) return 0.0f;
    fprintf(stderr, "TRAINER_ITER start\n"); fflush(stderr);
    
    BearEnv* env = trainer->env;
    BearPPOConfig* cfg = &trainer->cfg;
    BearTrajectory* traj = &trainer->traj;
    BearPolicyNet* policy = trainer->policy;
    fprintf(stderr, "TRAINER_ITER vars set\n"); fflush(stderr);
    
    /* 1. Collect rollout */
    bear_traj_reset(traj);
    bear_env_reset_all(env, &trainer->rollout_arena);
    
    float ep_return_sum = 0;
    int ep_count = 0;
    
    for (int step = 0; step < traj->rollout_len; ++step) {
        /* Forward pass to get actions */
        BearTensor actions, logprobs, values, h_out;
        int B = env->spec.num_envs * env->spec.max_agents;
        
        int64_t act_shape_arr[2] = { B, env->spec.act_dim };
        int64_t scalar_shape_arr[1] = { B };
        
        bear_tensor_create(&trainer->step_arena, &actions,
                           act_shape_arr, 2, BEAR_DTYPE_F32, "rollout_act");
        bear_tensor_create(&trainer->step_arena, &logprobs,
                           scalar_shape_arr, 1, BEAR_DTYPE_F32, "rollout_lp");
        bear_tensor_create(&trainer->step_arena, &values,
                           scalar_shape_arr, 1, BEAR_DTYPE_F32, "rollout_val");
        int64_t h_shape_arr[2] = { B, policy->hid_size > 0 ? policy->hid_size : 1 };
        bear_tensor_create(&trainer->step_arena, &h_out,
                           h_shape_arr, 2, BEAR_DTYPE_F32, "h_out");
        
        fprintf(stderr, "ABOUT TO FWD step=%d\n", step); fflush(stderr);
        bear_policy_forward(trainer->policy, &env->obs, NULL,
                             &actions, &logprobs, &values, &h_out,
                             &trainer->step_arena);
        fprintf(stderr, "FWD DONE step=%d\n", step); fflush(stderr);
        
        bear_policy_sample(trainer->policy, &actions, &logprobs, rng_state);
        
        /* Step environment */
        bear_env_step(env, &actions, &env->rewards, &env->dones,
                       &env->obs, &trainer->step_arena);
        
        /* Store trajectory */
        bear_traj_store(traj, step, &env->obs, &actions,
                         &logprobs, &env->rewards, &env->dones, &values);
        
        /* Track episode returns */
        uint8_t* dones = (uint8_t*)env->dones.data;
        for (int i = 0; i < env->spec.num_envs; ++i) {
            if (dones[i]) {
                ep_return_sum += env->episode_return[i];
                ep_count++;
            }
        }
        
        /* Reset step arena for next step */
        bear_arena_reset(&trainer->step_arena);
    }
    
    /* 2. Compute advantages */
    bear_compute_advantages(&trainer->traj, &trainer->cfg, &trainer->rollout_arena);
    
    /* 3. PPO update epochs */
    float total_policy_loss = 0, total_value_loss = 0, total_entropy = 0;
    
    for (int epoch = 0; epoch < cfg->epochs_per_iter; ++epoch) {
        BearMinibatchSampler sampler;
        bear_sampler_init(&sampler, traj, cfg->minibatch_size, rng_state);
        
        BearTensor mb_obs, mb_actions, mb_logprobs, mb_advantages;
        BearTensor mb_returns, mb_values, mb_old_logprobs;
        
        int mb_count = 0;
        
        while (bear_sampler_next(&sampler, traj, &mb_obs, &mb_actions,
                                 &mb_logprobs, &mb_advantages, &mb_returns,
                                 &mb_values, &mb_old_logprobs, &trainer->step_arena)) {
            
            fprintf(stderr, "PPO UPDATE: checking forward pass\n"); fflush(stderr);
            BearPPOLoss loss = bear_ppo_loss(trainer->policy, trainer->critic,
                                               &mb_obs, &mb_actions, &mb_old_logprobs,
                                               &mb_advantages, &mb_returns, &mb_values,
                                               cfg, &trainer->step_arena);
            fprintf(stderr, "LOSS: p=%f v=%f e=%f total=%f\n", loss.policy_loss, loss.value_loss, loss.entropy_loss, loss.total_loss); fflush(stderr);
            
            bear_ppo_update(trainer->policy, trainer->critic, &loss, trainer->opt_policy);
            
            total_policy_loss += loss.policy_loss;
            total_value_loss += loss.value_loss;
            total_entropy += loss.entropy_loss;
            mb_count++;
            
            if (cfg->target_kl > 0 && loss.approx_kl > cfg->target_kl * 1.5f) {
                break;
            }
        }
        
        free(sampler.indices);
    }
    
    /* Learning rate annealing */
    if (cfg->lr_anneal && trainer->iteration > 0) {
        float frac = 1.0f - (float)trainer->iteration / 10000.0f;
        if (frac < 0.1f) frac = 0.1f;
        bear_optimizer_set_lr(trainer->opt_policy, cfg->lr * frac);
        bear_optimizer_set_lr(trainer->opt_critic, cfg->lr * frac);
    }
    
    float avg_return = ep_count > 0 ? ep_return_sum / ep_count : 0.0f;
    if (avg_return > trainer->best_return) trainer->best_return = avg_return;
    
    /* Log */
    if (g_trainer_logger) {
        g_trainer_logger(trainer->iteration, trainer->total_steps, avg_return,
                          total_policy_loss / (cfg->epochs_per_iter * 1.0f),
                          total_value_loss / (cfg->epochs_per_iter * 1.0f),
                          total_entropy / (cfg->epochs_per_iter * 1.0f),
                          bear_optimizer_get_lr(trainer->opt_policy), g_trainer_log_ud);
    }
    
    trainer->iteration++;
    trainer->total_steps += trainer->traj.rollout_len * env->spec.num_envs;
    
    bear_arena_reset(&trainer->rollout_arena);
    
    return avg_return;
}

int bear_trainer_save(const BearTrainer* trainer, const char* path) {
    (void)trainer; (void)path;
    return 0;
}

int bear_trainer_load(BearTrainer* trainer, const char* path) {
    (void)trainer; (void)path;
    return 0;
}