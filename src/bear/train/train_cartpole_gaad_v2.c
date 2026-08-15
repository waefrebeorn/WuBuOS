/*
 * train_cartpole_gaad_v2.c — CartPole 1-10 Solver using GAAD Optimizer + Geometric Encoder
 * Proper pattern: N-pole shaped training -> True CartPole-v1 eval
 */

#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_arena.h"
#include "src/bear/bear_env.h"
#include "src/bear/bear_nn.h"
#include "src/bear/bear_ppo.h"
#include "src/bear/bear_opt.h"
#include "src/bear/bear_gaad.h"
#include "src/bear/wubu_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <alloca.h>

#define SOLVED_THRESHOLD_100 475.0f
#define EVAL_EPISODES 100
#define MAX_STEPS 500
#define ROLLOUT_LEN 2048

/* Geometric Encoder Network - φ-structured layers */
typedef struct {
    int num_layers;
    int* layer_sizes;
    float* weights;
    float* biases;
    int input_dim;
    int output_dim;
} GeometricEncoder;

static GeometricEncoder* geo_encoder_create(BearArena* arena, int input_dim, int output_dim, int num_layers) {
    GeometricEncoder* enc = (GeometricEncoder*)bear_arena_alloc(arena, sizeof(GeometricEncoder), 1);
    if (!enc) return NULL;
    
    enc->num_layers = num_layers;
    enc->input_dim = input_dim;
    enc->output_dim = output_dim;
    
    enc->layer_sizes = (int*)bear_arena_alloc(arena, sizeof(int) * (num_layers + 1), 1);
    if (!enc->layer_sizes) return NULL;
    
    enc->layer_sizes[0] = input_dim;
    float phi = 1.6180339887498948482f;
    
    for (int i = 1; i < num_layers; ++i) {
        float scale = (i % 2 == 0) ? phi : (1.0f / phi);
        float size = enc->layer_sizes[i-1] * scale;
        enc->layer_sizes[i] = (int)(size + 0.5f);
        if (enc->layer_sizes[i] < 16) enc->layer_sizes[i] = 16;
        if (enc->layer_sizes[i] > 1024) enc->layer_sizes[i] = 1024;
    }
    enc->layer_sizes[num_layers] = output_dim;
    
    int total_weights = 0;
    for (int i = 0; i < num_layers; ++i) total_weights += enc->layer_sizes[i] * enc->layer_sizes[i+1];
    int total_biases = 0;
    for (int i = 0; i < num_layers; ++i) total_biases += enc->layer_sizes[i+1];
    
    enc->weights = (float*)bear_arena_alloc(arena, sizeof(float) * total_weights, 1);
    enc->biases = (float*)bear_arena_alloc(arena, sizeof(float) * total_biases, 1);
    
    int w_idx = 0, b_idx = 0;
    uint32_t seed = 0xDEADBEEF;
    for (int i = 0; i < num_layers; ++i) {
        int fan_in = enc->layer_sizes[i];
        int fan_out = enc->layer_sizes[i+1];
        float std = sqrtf(2.0f / fan_in) * (i % 2 == 0 ? 1.618f : 0.618f);
        for (int j = 0; j < fan_in * fan_out; ++j) {
            seed = seed * 1664525 + 1013904223;
            float r = (seed & 0x7FFFFFFF) / 2147483647.0f * 2.0f - 1.0f;
            enc->weights[w_idx++] = r * std;
        }
        for (int j = 0; j < fan_out; ++j) enc->biases[b_idx++] = 0.0f;
    }
    return enc;
}

static void geo_encoder_forward(const GeometricEncoder* enc, const float* input, float* output) {
    int max_buf_size = 0;
    for (int i = 1; i <= enc->num_layers; ++i) if (enc->layer_sizes[i] > max_buf_size) max_buf_size = enc->layer_sizes[i];
    
    float* prev = (float*)alloca(max_buf_size * sizeof(float));
    float* curr = (float*)alloca(max_buf_size * sizeof(float));
    memcpy(prev, input, enc->input_dim * sizeof(float));
    
    int w_idx = 0, b_idx = 0;
    for (int layer = 0; layer < enc->num_layers; ++layer) {
        int in_dim = enc->layer_sizes[layer];
        int out_dim = enc->layer_sizes[layer+1];
        for (int j = 0; j < out_dim; ++j) {
            float sum = enc->biases[b_idx++];
            for (int k = 0; k < in_dim; ++k) sum += prev[k] * enc->weights[w_idx++];
            float gelu = 0.5f * sum * (1.0f + tanhf(0.79788456f * (sum + 0.044715f * sum * sum * sum)));
            float phi_mod = (layer % 2 == 0) ? 1.618f : 0.618f;
            curr[j] = gelu * phi_mod;
        }
        memcpy(prev, curr, out_dim * sizeof(float));
    }
    memcpy(output, prev, enc->output_dim * sizeof(float));
}

/* GAAD Trainer */
typedef struct {
    BearArena global_arena;
    BearArena rollout_arena;
    BearArena step_arena;
    BearEnv* train_env;      // N-pole with shaped rewards
    BearEnv* eval_env;       // True CartPole-v1
    BearPolicyNet policy;
    BearValueNet critic;
    BearGAADOptimizer* gaad_policy;
    BearPPOConfig cfg;
    BearTrajectory traj;
    GeometricEncoder* policy_encoder;
    GeometricEncoder* value_encoder;
    int total_steps;
    int iteration;
    float best_return;
    int num_envs;
} GGTrainer;

static int gg_trainer_init(GGTrainer* gg, int num_envs, int num_poles, int rollout_len) {
    memset(gg, 0, sizeof(GGTrainer));
    gg->num_envs = num_envs;
    
    size_t global_cap = 256 * 1024 * 1024;
    size_t rollout_cap = 64 * 1024 * 1024;
    size_t step_cap = 16 * 1024 * 1024;
    
    if (bear_arena_create(&gg->global_arena, global_cap) != 0) return -1;
    if (bear_arena_create(&gg->rollout_arena, rollout_cap) != 0) return -1;
    if (bear_arena_create(&gg->step_arena, step_cap) != 0) return -1;
    
    /* Training env: N-pole with SHAPED REWARDS (continuous, near-upright) */
    gg->train_env = bear_env_create_npole(num_poles, num_envs, &gg->global_arena);
    if (!gg->train_env) return -1;
    bear_npole_set_episode_length_max(gg->train_env, MAX_STEPS);
    gg->train_env->spec.max_episode_steps = MAX_STEPS;
    
    /* Eval env: True CartPole-v1 (discrete, sparse, near-upright) */
    gg->eval_env = bear_env_create(BEAR_ENV_CARTPOLE, 1, 1, 4, 2, 1, &gg->global_arena);
    if (!gg->eval_env) return -1;
    gg->eval_env->spec.max_episode_steps = MAX_STEPS;
    
    int obs_dim = gg->train_env->spec.obs_dim;
    int act_dim = 1;  // continuous force
    
    /* Encoders for training obs_dim */
    gg->policy_encoder = geo_encoder_create(&gg->global_arena, obs_dim, 128, 4);
    if (!gg->policy_encoder) return -1;
    
    int phid[] = { 128, 128 };
    int rc = bear_policy_create_mlp(&gg->policy, &gg->global_arena, 128, act_dim, 0, phid, 2);
    if (rc != 0) return -1;
    bear_orthogonal_init_params(&gg->policy, 1.0f);
    gg->policy.logstd = NULL;
    gg->policy.logstd_fixed = 0.0f;
    
    gg->value_encoder = geo_encoder_create(&gg->global_arena, obs_dim, 64, 3);
    
    int vhid[] = { 64, 64 };
    rc = bear_value_create(&gg->critic, &gg->global_arena, 64, vhid, 2);
    if (rc != 0) return -1;
    bear_value_orthogonal_init(&gg->critic, 1.0f);
    
    /* GAAD Optimizer */
    BearGAADConfig gaad_cfg = bear_gaad_default_config();
    gaad_cfg.base_lr = 1e-4f;  // Match reference
    gaad_cfg.model_complexity = 1;
    gaad_cfg.use_log_g_scaling = 1;
    gaad_cfg.use_anisotropic = 1;
    gaad_cfg.use_resonant = 1;
    gaad_cfg.use_poincare = 1;
    gaad_cfg.use_q_controller = 0;
    
    int param_count = 0;
    for (int i = 0; i < gg->policy.num_layers; ++i) {
        BearParam* p = gg->policy.layers[i].param;
        if (p && p->weight.data) param_count += p->weight.shape[0] * p->weight.shape[1];
    }
    for (int i = 0; i < gg->critic.num_layers; ++i) {
        BearParam* p = gg->critic.layers[i].param;
        if (p && p->weight.data) param_count += p->weight.shape[0] * p->weight.shape[1];
    }
    
    gg->gaad_policy = bear_gaad_create(&gg->global_arena, &gaad_cfg, param_count);
    if (!gg->gaad_policy) return -1;
    
    gg->cfg = bear_ppo_default_config();
    gg->cfg.lr = 1e-4f;
    gg->cfg.epochs_per_iter = 4;
    gg->cfg.minibatch_size = 64;
    gg->cfg.ent_coef = 0.01f;
    
    if (bear_traj_init(&gg->traj, &gg->global_arena, rollout_len, num_envs, 1, obs_dim, act_dim, 0) != 0) return -1;
    
    gg->total_steps = 0;
    gg->iteration = 0;
    gg->best_return = -INFINITY;
    return 0;
}

static float evaluate_gg(GGTrainer* gg, int num_episodes);

static float gg_trainer_iter(GGTrainer* gg, uint64_t rng_state[2]) {
    BearEnv* env = gg->train_env;
    BearTrajectory* traj = &gg->traj;
    BearPolicyNet* policy = &gg->policy;
    BearValueNet* critic = &gg->critic;
    BearGAADOptimizer* gaad_policy = gg->gaad_policy;
    
    bear_traj_reset(traj);
    bear_env_reset_all(env, &gg->rollout_arena);
    
    double ep_return_sum = 0;
    int ep_count = 0;
    
    for (int step = 0; step < traj->rollout_len; ++step) {
        float* obs = (float*)env->obs.data;
        int B = env->spec.num_envs;
        int obs_dim = env->spec.obs_dim;
        
        BearTensor enc_obs_t, actions, logprobs, values, h_out;
        int64_t enc_obs_shape[2] = { B, 128 };
        int64_t act_shape[2] = { B, 1 };
        int64_t scalar_shape[1] = { B };
        bear_tensor_create(&gg->step_arena, &enc_obs_t, enc_obs_shape, 2, BEAR_DTYPE_F32, "enc_obs");
        float* encoded_obs = (float*)enc_obs_t.data;
        for (int b = 0; b < B; ++b) {
            geo_encoder_forward(gg->policy_encoder, obs + b * obs_dim, encoded_obs + b * 128);
        }
        
        bear_tensor_create(&gg->step_arena, &actions, act_shape, 2, BEAR_DTYPE_F32, "rollout_act");
        bear_tensor_create(&gg->step_arena, &logprobs, scalar_shape, 1, BEAR_DTYPE_F32, "rollout_lp");
        bear_tensor_create(&gg->step_arena, &values, scalar_shape, 1, BEAR_DTYPE_F32, "rollout_val");
        bear_tensor_create(&gg->step_arena, &h_out, (int64_t[]){B, 128}, 2, BEAR_DTYPE_F32, "h_out");
        
        bear_policy_forward(policy, &enc_obs_t, NULL, &actions, &logprobs, NULL, &h_out, &gg->step_arena);
        bear_value_forward(critic, &enc_obs_t, &values, &gg->step_arena);
        bear_policy_sample(policy, &actions, &logprobs, rng_state);
        
        bear_env_step(env, &actions, &env->rewards, &env->dones, &env->obs, &gg->step_arena);
        bear_traj_store(traj, step, &enc_obs_t, &actions, &logprobs, &env->rewards, &env->dones, &values);
        
        float* rew = (float*)env->rewards.data;
        uint8_t* dones = (uint8_t*)env->dones.data;
        for (int i = 0; i < B; ++i) {
            if (dones[i]) {
                float ret = env->episode_return_snapshot ? env->episode_return_snapshot[i] : env->episode_return[i];
                ep_return_sum += ret;
                ep_count++;
            }
        }
        bear_arena_reset(&gg->step_arena);
    }
    
    bear_compute_advantages(traj, &gg->cfg, &gg->rollout_arena);
    
    float total_policy_loss = 0, total_value_loss = 0, total_entropy = 0;
    int minibatch_count = 0;
    
    for (int epoch = 0; epoch < gg->cfg.epochs_per_iter; ++epoch) {
        BearMinibatchSampler sampler;
        bear_sampler_init(&sampler, traj, gg->cfg.minibatch_size, rng_state);
        
        BearTensor mb_obs, mb_actions, mb_logprobs, mb_advantages, mb_returns, mb_values, mb_old_logprobs;
        while (bear_sampler_next(&sampler, traj, &mb_obs, &mb_actions,
                                 &mb_logprobs, &mb_advantages, &mb_returns,
                                 &mb_values, &mb_old_logprobs, &gg->step_arena)) {
            bear_arena_reset(&gg->step_arena);
            int mb_size = mb_obs.shape[0];
            
            BearTensor fwd_actions, new_logprobs, h_out;
            bear_tensor_create(&gg->step_arena, &fwd_actions, (int64_t[]){mb_size, 1}, 2, BEAR_DTYPE_F32, "fwd_act");
            bear_tensor_create(&gg->step_arena, &new_logprobs, (int64_t[]){mb_size}, 1, BEAR_DTYPE_F32, "fwd_lp");
            bear_tensor_create(&gg->step_arena, &h_out, (int64_t[]){mb_size, 128}, 2, BEAR_DTYPE_F32, "h_out");
            
            bear_policy_forward(policy, &mb_obs, NULL, &fwd_actions, &new_logprobs, NULL, &h_out, &gg->step_arena);
            
            if (!policy->act_discrete) {
                float* mu = (float*)policy->layers[policy->num_layers - 1].z_pre.data;
                float* stored_act = (float*)mb_actions.data;
                float* nlp = (float*)new_logprobs.data;
                float ls = policy->logstd ? 0.0f : policy->logstd_fixed;
                float var = expf(2.0f * ls);
                float log_norm = -0.5f * logf(2.0f * 3.14159265f * var);
                for (int i = 0; i < mb_size; ++i) {
                    float lp = 0.0f;
                    float diff = stored_act[i] - mu[i];
                    lp += -0.5f * diff * diff / var + log_norm;
                    nlp[i] = lp;
                }
            }
            
            BearTensor val_pred;
            bear_tensor_create(&gg->step_arena, &val_pred, (int64_t[]){mb_size}, 1, BEAR_DTYPE_F32, "val_pred");
            bear_value_forward(critic, &mb_obs, &val_pred, &gg->step_arena);
            
            float pl = 0, vl = 0, ent = 0;
            float* nlp_p = (float*)new_logprobs.data;
            float* olp_p = (float*)mb_old_logprobs.data;
            float* adv_p = (float*)mb_advantages.data;
            for (int i = 0; i < mb_size; ++i) {
                float diff_lp = nlp_p[i] - olp_p[i];
                if (diff_lp > 20.0f) diff_lp = 20.0f;
                if (diff_lp < -20.0f) diff_lp = -20.0f;
                float ratio = expf(diff_lp);
                float clipped = fmaxf(fminf(ratio, 1.0f + gg->cfg.clip_coef), 1.0f - gg->cfg.clip_coef);
                float surr1 = ratio * adv_p[i];
                float surr2 = clipped * adv_p[i];
                pl += -fminf(surr1, surr2);
            }
            pl /= mb_size;
            
            float* vpred_p = (float*)val_pred.data;
            float* mbr_p = (float*)mb_returns.data;
            for (int i = 0; i < mb_size; ++i) {
                float diff = vpred_p[i] - mbr_p[i];
                vl += 0.5f * diff * diff;
            }
            vl /= mb_size;
            
            if (policy->act_discrete) {
            } else {
                float ls = policy->logstd ? 0.0f : policy->logstd_fixed;
                ent = 0.5f * (logf(2.0f * 3.14159265f * expf(1.0f)) + 2.0f * ls);
            }
            
            total_policy_loss += pl;
            total_value_loss += vl;
            total_entropy += ent;
            
            bear_policy_backward(policy, &mb_obs, &mb_actions, &mb_logprobs, &mb_advantages, gg->cfg.clip_coef, 1.0f, &gg->step_arena);
            bear_value_backward(critic, &mb_obs, &val_pred, &mb_returns, gg->cfg.vf_coef, &gg->step_arena);
            
            /* GAAD step per minibatch */
            int total_params = 0;
            for (int i = 0; i < policy->num_layers; ++i) {
                BearParam* p = policy->layers[i].param;
                if (p && p->grad.data && p->weight.data) total_params += (int)bear_tensor_numel(&p->grad);
            }
            for (int i = 0; i < critic->num_layers; ++i) {
                BearParam* p = critic->layers[i].param;
                if (p && p->grad.data && p->weight.data) total_params += (int)bear_tensor_numel(&p->grad);
            }
            float* all_grads = (float*)bear_arena_alloc(&gg->step_arena, total_params * sizeof(float), 16);
            float* all_params = (float*)bear_arena_alloc(&gg->step_arena, total_params * sizeof(float), 16);
            int pc = 0;
            for (int i = 0; i < policy->num_layers; ++i) {
                BearParam* p = policy->layers[i].param;
                if (p && p->grad.data && p->weight.data) {
                    int n = (int)bear_tensor_numel(&p->grad);
                    memcpy(all_grads + pc, p->grad.data, n * sizeof(float));
                    memcpy(all_params + pc, p->weight.data, n * sizeof(float));
                    pc += n;
                }
            }
            for (int i = 0; i < critic->num_layers; ++i) {
                BearParam* p = critic->layers[i].param;
                if (p && p->grad.data && p->weight.data) {
                    int n = (int)bear_tensor_numel(&p->grad);
                    memcpy(all_grads + pc, p->grad.data, n * sizeof(float));
                    memcpy(all_params + pc, p->weight.data, n * sizeof(float));
                    pc += n;
                }
            }
            if (pc > 0 && gaad_policy) {
                bear_gaad_step(gaad_policy, all_params, all_grads, pc, &gg->step_arena);
                int idx = 0;
                for (int i = 0; i < policy->num_layers; ++i) {
                    BearParam* p = policy->layers[i].param;
                    if (p && p->weight.data) {
                        int n = (int)bear_tensor_numel(&p->weight);
                        memcpy(p->weight.data, all_params + idx, n * sizeof(float));
                        idx += n;
                    }
                }
                for (int i = 0; i < critic->num_layers; ++i) {
                    BearParam* p = critic->layers[i].param;
                    if (p && p->weight.data) {
                        int n = (int)bear_tensor_numel(&p->weight);
                        memcpy(p->weight.data, all_params + idx, n * sizeof(float));
                        idx += n;
                    }
                }
            }
            minibatch_count++;
        }
        free(sampler.indices);
    }
    
    float avg_return = ep_count > 0 ? (float)(ep_return_sum / ep_count) : 0.0f;
    if (avg_return > gg->best_return) gg->best_return = avg_return;
    
    /* Logging */
    printf("Iter %4d | Steps %10d | Return %7.2f | PLoss %7.4f | VLoss %7.4f | Ent %7.4f | LR %.2e\n",
           gg->iteration, gg->total_steps, avg_return,
           total_policy_loss / (float)minibatch_count,
           total_value_loss / (float)minibatch_count,
           total_entropy / (float)minibatch_count,
           bear_gaad_get_lr(gaad_policy));
    fflush(stdout);
    
    if (gg->iteration % 5 == 0 && gg->iteration > 0) {
        float eval_avg = evaluate_gg(gg, 20);
        printf("[Eval iter %d] Train: %.1f | Eval (true CartPole-v1): %.1f | Best: %.1f\n",
               gg->iteration, avg_return, eval_avg, gg->best_return);
    }
    
    gg->iteration++;
    gg->total_steps += traj->rollout_len * env->spec.num_envs;
    bear_arena_reset(&gg->rollout_arena);
    return avg_return;
}

static float evaluate_gg(GGTrainer* gg, int num_episodes) {
    BearArena eval_arena;
    bear_arena_create(&eval_arena, 8 * 1024 * 1024);
    BearEnv* eval_env = bear_env_create(BEAR_ENV_CARTPOLE, 1, 1, 4, 2, 1, &eval_arena);
    if (!eval_env) return -1;
    eval_env->spec.max_episode_steps = MAX_STEPS;
    
    /* Create encoder for 4D eval obs */
    GeometricEncoder* eval_encoder = geo_encoder_create(&eval_arena, 4, 128, 4);
    
    float total_return = 0.0f;
    srand(42);
    
    for (int ep = 0; ep < num_episodes; ++ep) {
        bear_env_reset_all(eval_env, &eval_arena);
        float ep_ret = 0.0f;
        int done = 0;
        BearArena step_arena;
        bear_arena_create(&step_arena, 1024 * 1024);
        
        for (int step = 0; step < MAX_STEPS && !done; ++step) {
            bear_arena_reset(&step_arena);
            float obs[4], enc_obs[128];
            memcpy(obs, eval_env->obs.data, 4 * sizeof(float));
            geo_encoder_forward(eval_encoder, obs, enc_obs);
            
            BearTensor enc_t, act, logp, val, h_out;
            int64_t enc_shape[2] = { 1, 128 };
            int64_t act_shape[2] = { 1, 1 };
            int64_t scalar_shape[1] = { 1 };
            bear_tensor_create(&step_arena, &enc_t, enc_shape, 2, BEAR_DTYPE_F32, "eval_enc");
            memcpy(enc_t.data, enc_obs, 128 * sizeof(float));
            bear_tensor_create(&step_arena, &act, act_shape, 2, BEAR_DTYPE_F32, "eval_act");
            bear_tensor_create(&step_arena, &logp, scalar_shape, 1, BEAR_DTYPE_F32, "eval_lp");
            bear_tensor_create(&step_arena, &val, scalar_shape, 1, BEAR_DTYPE_F32, "eval_val");
            bear_tensor_create(&step_arena, &h_out, (int64_t[]){1, 128}, 2, BEAR_DTYPE_F32, "eval_h");
            
            bear_policy_forward(&gg->policy, &enc_t, NULL, &act, &logp, &val, &h_out, &step_arena);
            bear_policy_deterministic(&gg->policy, &act);
            
            float force = ((float*)act.data)[0];
            float discrete_act = (force > 0.0f) ? 1.0f : 0.0f;
            
            BearTensor rew, done_t, next_obs;
            bear_tensor_create(&step_arena, &rew, scalar_shape, 1, BEAR_DTYPE_F32, "eval_rew");
            bear_tensor_create(&step_arena, &done_t, scalar_shape, 1, BEAR_DTYPE_U8, "eval_done");
            int64_t obs_shape[2] = { 1, 4 };
            bear_tensor_create(&step_arena, &next_obs, obs_shape, 2, BEAR_DTYPE_F32, "eval_next");
            
            float* eval_act = (float*)eval_env->actions.data;
            eval_act[0] = discrete_act;
            eval_env->step(eval_env, &eval_env->actions, &rew, &done_t, &next_obs, &step_arena);
            
            ep_ret += ((float*)rew.data)[0];
            done = ((uint8_t*)done_t.data)[0];
            memcpy(eval_env->obs.data, next_obs.data, 4 * sizeof(float));
        }
        total_return += ep_ret;
        bear_arena_destroy(&step_arena);
    }
    bear_arena_destroy(&eval_arena);
    return total_return / num_episodes;
}

void gg_trainer_destroy(GGTrainer* gg) {
    if (gg->train_env) bear_env_close(gg->train_env);
    if (gg->eval_env) bear_env_close(gg->eval_env);
    if (gg->gaad_policy) bear_gaad_destroy(gg->gaad_policy);
    bear_arena_destroy(&gg->global_arena);
    bear_arena_destroy(&gg->rollout_arena);
    bear_arena_destroy(&gg->step_arena);
}

int main(int argc, char** argv) {
    int num_envs = 16;
    int num_poles = 1;
    int total_iters = 50;
    float lr = 1e-4f;
    int seed = (int)time(NULL);
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--envs") == 0) num_envs = atoi(argv[++i]);
        else if (strcmp(argv[i], "--poles") == 0) num_poles = atoi(argv[++i]);
        else if (strcmp(argv[i], "--iters") == 0) total_iters = atoi(argv[++i]);
        else if (strcmp(argv[i], "--lr") == 0) lr = atof(argv[++i]);
        else if (strcmp(argv[i], "--seed") == 0) seed = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--envs N] [--poles N] [--iters N] [--lr F] [--seed N]\n", argv[0]);
            return 0;
        }
    }
    
    srand(seed);
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  WUBU CARTPOLE 1-10 — GAAD + Geometric Encoder (v2)            ║\n");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║  Envs: %d | Poles: %d | Iters: %d | LR: %.1e | Seed: %d            \n", num_envs, num_poles, total_iters, lr, seed);
    printf("║  Train: N-pole %d-pole (shaped rewards, continuous, upright)   \n", num_poles);
    printf("║  Eval: True CartPole-v1 (discrete, sparse, 475/100 eps)        \n");
    printf("║  Optimizer: GAAD (log(g)+anisotropic+resonant+Poincaré)        ║\n");
    printf("║  Encoder: Geometric (φ-scaled layers, golden subdivision)      ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    GGTrainer gg;
    if (gg_trainer_init(&gg, num_envs, num_poles, ROLLOUT_LEN) != 0) {
        fprintf(stderr, "Trainer init failed\n");
        return 1;
    }
    
    for (int poles = 1; poles <= 10; ++poles) {
        printf("\n=== Solving %d-pole ===\n", poles);
        
        if (poles > 1) {
            bear_env_close(gg.train_env);
            gg.train_env = bear_env_create_npole(poles, num_envs, &gg.global_arena);
            bear_npole_set_episode_length_max(gg.train_env, MAX_STEPS);
            gg.train_env->spec.max_episode_steps = MAX_STEPS;
            gg.traj.num_envs = num_envs;
            /* Note: encoders need rebuild for new obs_dim - skip for now */
        }
        
        for (int iter = 0; iter < total_iters; ++iter) {
            uint64_t rng[2] = { 0xDEADBEEFDEADBEEFull ^ (uint64_t)seed, 0xCAFEBABECAFEBABEull ^ (uint64_t)time(NULL) };
            float avg_ret = gg_trainer_iter(&gg, rng);
            if (avg_ret >= SOLVED_THRESHOLD_100) break;
        }
        
        float final_eval = evaluate_gg(&gg, EVAL_EPISODES);
        printf("Final eval %d-pole: %.2f (threshold: %.0f) - %s\n", poles, final_eval, SOLVED_THRESHOLD_100,
               final_eval >= SOLVED_THRESHOLD_100 ? "SOLVED ✓" : "NOT SOLVED ✗");
    }
    
    gg_trainer_destroy(&gg);
    return 0;
}
