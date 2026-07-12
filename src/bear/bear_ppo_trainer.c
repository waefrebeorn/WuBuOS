/*
 * bear_ppo_trainer.c -- BearRL PPO trainer lifecycle, training iteration,
 * and checkpoint save/load (extracted from the monolithic bear_ppo.c).
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
 * Trainer Initialization
 * =================================================================== */

int bear_trainer_init(BearTrainer* trainer,
                       BearPolicyNet* policy, BearValueNet* critic,
                       BearEnv* env, const BearPPOConfig* cfg,
                       size_t global_arena_cap, size_t rollout_arena_cap,
                       size_t step_arena_cap) {
    if (!trainer || !policy || !critic || !env || !cfg) return -1;

    memset(trainer, 0, sizeof(BearTrainer));

    if (bear_arena_create(&trainer->global_arena, global_arena_cap) != 0) return -1;
    if (bear_arena_create(&trainer->rollout_arena, rollout_arena_cap) != 0) {
        bear_arena_destroy(&trainer->global_arena);
        return -1;
    }
    if (bear_arena_create(&trainer->step_arena, step_arena_cap) != 0) {
        bear_arena_destroy(&trainer->rollout_arena);
        bear_arena_destroy(&trainer->global_arena);
        return -1;
    }

    trainer->policy = policy;
    trainer->critic = critic;
    trainer->env = env;
    trainer->cfg = *cfg;

    if (bear_traj_init(&trainer->traj, &trainer->global_arena,
                        128, env->spec.num_envs, env->spec.max_agents,
                        env->spec.obs_dim, env->spec.act_dim, env->spec.act_discrete) != 0) {
        return -1;
    }

    trainer->opt_policy = bear_optimizer_create(&trainer->global_arena, BEAR_OPT_ADAM, cfg->lr);
    trainer->opt_critic = bear_optimizer_create(&trainer->global_arena, BEAR_OPT_ADAM, cfg->lr);

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

    BearEnv* env = trainer->env;
    BearPPOConfig* cfg = &trainer->cfg;
    BearTrajectory* traj = &trainer->traj;
    BearPolicyNet* policy = trainer->policy;

    /* 1. Collect rollout */
    bear_traj_reset(traj);
    bear_env_reset_all(env, &trainer->rollout_arena);

    double ep_return_sum = 0;
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

        /* Observation normalization (running mean/std) */
        if (trainer->cfg.normalize_obs) {
            int obs_dim = env->spec.obs_dim;
            float* obs_p = (float*)env->obs.data;
            float* mean = traj->obs_rms_mean;
            float* var = traj->obs_rms_var;
            int* count = &traj->obs_rms_count;
            
            /* Update running stats (Welford's online algorithm) - per sample (env) */
            for (int i = 0; i < B; ++i) {
                int c = *count + 1;
                for (int d = 0; d < obs_dim; ++d) {
                    float x = obs_p[i * obs_dim + d];
                    float delta = x - mean[d];
                    mean[d] += delta / c;
                    float delta2 = x - mean[d];
                    var[d] += delta * delta2;
                }
                *count = c;
            }
            
            /* Normalize observations in-place */
            for (int i = 0; i < B; ++i) {
                for (int d = 0; d < obs_dim; ++d) {
                    float std = sqrtf(var[d] / (*count) + 1e-8f);
                    obs_p[i * obs_dim + d] = (obs_p[i * obs_dim + d] - mean[d]) / std;
                }
            }
        }

        /* Policy forward: obs -> actions, logprobs */
        bear_policy_forward(trainer->policy, &env->obs, NULL,
                             &actions, &logprobs, NULL, &h_out,
                             &trainer->step_arena);
        for (int i = 0; i < B; i++) {
            float a = ((float*)actions.data)[i];
        }

        /* Value network forward: obs -> values */
        bear_value_forward(trainer->critic, &env->obs, &values, &trainer->step_arena);
        for (int i = 0; i < B; i++) {
            float v = ((float*)values.data)[i];
        }

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
                float ret = env->episode_return_snapshot ?
                    env->episode_return_snapshot[i] : env->episode_return[i];
                ep_return_sum += ret;
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
        
        /* Zero gradients at start of epoch */
        bear_policy_zero_grad(policy);
        bear_value_zero_grad(trainer->critic);
        
        while (bear_sampler_next(&sampler, traj, &mb_obs, &mb_actions,
                                 &mb_logprobs, &mb_advantages, &mb_returns,
                                 &mb_values, &mb_old_logprobs, &trainer->step_arena)) {

            int mb_size = (int)mb_obs.shape[0];
            int act_dim = trainer->policy->act_dim;

            /* Forward: get mean from policy */
            BearTensor fwd_actions, new_logprobs, h_out;
            bear_tensor_create(&trainer->step_arena, &fwd_actions,
                               (int64_t[]){mb_size, act_dim}, 2, BEAR_DTYPE_F32, "fwd_act");
            bear_tensor_create(&trainer->step_arena, &new_logprobs,
                               (int64_t[]){mb_size}, 1, BEAR_DTYPE_F32, "fwd_lp");
            bear_tensor_create(&trainer->step_arena, &h_out,
                               (int64_t[]){mb_size, 1}, 2, BEAR_DTYPE_F32, "h_out");
            bear_policy_forward(trainer->policy, &mb_obs, NULL, &fwd_actions, &new_logprobs,
                                 NULL, &h_out, &trainer->step_arena);

            /* For continuous actions: evaluate logprob of STORED actions under current mean */
            if (!trainer->policy->act_discrete) {
                float* mu = (float*)trainer->policy->layers[trainer->policy->num_layers - 1].z_pre.data;
                float* stored_act = (float*)mb_actions.data;
                float* nlp = (float*)new_logprobs.data;
                float ls = trainer->policy->logstd ? 0.0f : trainer->policy->logstd_fixed;
                float var = expf(2.0f * ls);
                float log_norm = -0.5f * logf(2.0f * 3.14159265f * var);
                for (int i = 0; i < mb_size; ++i) {
                    float lp = 0.0f;
                    for (int a = 0; a < act_dim; ++a) {
                        float diff = stored_act[i * act_dim + a] - mu[i * act_dim + a];
                        lp += -0.5f * diff * diff / var + log_norm;
                    }
                    nlp[i] = lp;
                }
            } else {
                /* Discrete: evaluate logprob of STORED actions (one-hot) under current probabilities */
                float* probs = (float*)fwd_actions.data;
                float* stored_act = (float*)mb_actions.data;
                float* nlp = (float*)new_logprobs.data;
                for (int i = 0; i < mb_size; ++i) {
                    /* Find the sampled action index (one-hot) */
                    int sampled = 0;
                    for (int a = 0; a < act_dim; ++a) {
                        if (stored_act[i * act_dim + a] > 0.5f) { sampled = a; break; }
                    }
                    nlp[i] = logf(probs[i * act_dim + sampled] + 1e-8f);
                }
            }

            /* Compute PPO loss for logging */
            float pl = 0;
            float* nlp_p = (float*)new_logprobs.data;
            float* olp_p = (float*)mb_old_logprobs.data;
            float* adv_p = (float*)mb_advantages.data;
            for (int i = 0; i < mb_size; ++i) {
                float diff_lp = nlp_p[i] - olp_p[i];
                if (diff_lp > 20.0f) diff_lp = 20.0f;
                if (diff_lp < -20.0f) diff_lp = -20.0f;
                float ratio = expf(diff_lp);
                float clipped = fmaxf(fminf(ratio, 1.0f + cfg->clip_coef), 1.0f - cfg->clip_coef);
                float surr1 = ratio * adv_p[i];
                float surr2 = clipped * adv_p[i];
                float sl = -fminf(surr1, surr2);
                if (isnan(sl) || isinf(sl)) sl = 0.0f;
                pl += sl;
            }
            pl /= mb_size;

            /* Backward: accumulate policy gradients */
            bear_policy_backward(trainer->policy, &mb_obs, &mb_actions,
                                  &mb_old_logprobs, &mb_advantages,
                                  cfg->clip_coef, 1.0f, &trainer->step_arena);

            /* Value forward + backward */
            BearTensor val_pred;
            bear_tensor_create(&trainer->step_arena, &val_pred,
                               (int64_t[]){mb_size}, 1, BEAR_DTYPE_F32, "val_pred");
            bear_value_forward(trainer->critic, &mb_obs, &val_pred, &trainer->step_arena);
            bear_value_backward(trainer->critic, &mb_obs, &val_pred,
                                 &mb_returns, cfg->vf_coef, &trainer->step_arena);

            total_policy_loss += pl;
            mb_count++;
        }
        
        /* Clip gradients to prevent explosion */
        bear_ppo_clip_grad_norm(trainer->policy, trainer->critic, 5.0f);

        /* Apply accumulated gradients via Adam */
        bear_ppo_apply_gradients(trainer->policy, trainer->critic,
                                   trainer->opt_policy, trainer->opt_critic);
        
        /* Increment optimizer step counter */
        trainer->opt_policy->step++;
        trainer->opt_critic->step++;
        
        free(sampler.indices);
    }
    
    /* Learning rate annealing */
    if (cfg->lr_anneal && trainer->iteration > 0) {
        float frac = 1.0f - (float)trainer->iteration / 10000.0f;
        if (frac < 0.1f) frac = 0.1f;
        bear_optimizer_set_lr(trainer->opt_policy, cfg->lr * frac);
        bear_optimizer_set_lr(trainer->opt_critic, cfg->lr * frac);
    }
    
    float avg_return = ep_count > 0 ? (float)(ep_return_sum / ep_count) : 0.0f;
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
    if (!trainer || !path) return -1;
    FILE* f = fopen(path, "wb");
    if (!f) return -1;

    int rc = -1;
    do {
        /* Trainer header */
        const char magic[8] = {'W','U','T','R','N','E','R','\0'};
        if (fwrite(magic, 1, 8, f) != 8) break;
        int32_t version = 1;
        if (fwrite(&version, sizeof(version), 1, f) != 1) break;

        /* Scalar trainer state */
        if (fwrite(&trainer->total_steps, sizeof(trainer->total_steps), 1, f) != 1) break;
        if (fwrite(&trainer->iteration,    sizeof(trainer->iteration),    1, f) != 1) break;
        if (fwrite(&trainer->best_return,  sizeof(trainer->best_return),  1, f) != 1) break;

        /* PPO config */
        if (fwrite(&trainer->cfg, sizeof(BearPPOConfig), 1, f) != 1) break;

        /* Write policy network checkpoint to sidecar file */
        int32_t has_policy = trainer->policy ? 1 : 0;
        if (fwrite(&has_policy, sizeof(has_policy), 1, f) != 1) break;
        if (has_policy) {
            int32_t policy_ok = 0;
            size_t plen = strlen(path) + 8;
            char* polpath = (char*)malloc(plen);
            if (!polpath) break;
            snprintf(polpath, plen, "%s.policy", path);
            int prc = bear_checkpoint_save(trainer->policy, polpath);
            free(polpath);
            policy_ok = (prc == 0) ? 1 : 0;
            if (fwrite(&policy_ok, sizeof(policy_ok), 1, f) != 1) break;
        }

        rc = 0;
    } while (0);

    fclose(f);
    return rc;
}

int bear_trainer_load(BearTrainer* trainer, const char* path) {
    if (!trainer || !path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;

    int rc = -1;
    do {
        char magic[8];
        if (fread(magic, 1, 8, f) != 8) break;
        if (memcmp(magic, "WUTRNER\0", 8) != 0) break;
        int32_t version = 0;
        if (fread(&version, sizeof(version), 1, f) != 1 || version != 1) break;

        if (fread(&trainer->total_steps, sizeof(trainer->total_steps), 1, f) != 1) break;
        if (fread(&trainer->iteration,    sizeof(trainer->iteration),    1, f) != 1) break;
        if (fread(&trainer->best_return,  sizeof(trainer->best_return),  1, f) != 1) break;

        BearPPOConfig saved_cfg;
        if (fread(&saved_cfg, sizeof(BearPPOConfig), 1, f) != 1) break;
        trainer->cfg = saved_cfg;

        int32_t has_policy = 0;
        if (fread(&has_policy, sizeof(has_policy), 1, f) != 1) break;
        if (has_policy) {
            int32_t policy_ok = 0;
            if (fread(&policy_ok, sizeof(policy_ok), 1, f) != 1) break;
            if (policy_ok && trainer->policy) {
                size_t plen = strlen(path) + 8;
                char* polpath = (char*)malloc(plen);
                if (!polpath) { rc = -2; break; }
                snprintf(polpath, plen, "%s.policy", path);
                int prc = bear_checkpoint_load(trainer->policy, polpath);
                free(polpath);
                if (prc != 0) { rc = -3; break; }
            }
        }

        rc = 0;
    } while (0);

    fclose(f);
    return rc;
}
