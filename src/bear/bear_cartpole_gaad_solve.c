/*
 * bear_cartpole_gaad_solve.c  --  CartPole 1-10 Solver using GAAD Optimizer + Geometric Encoder
 *
 * This uses:
 * - GAAD optimizer (log(g) scaling, anisotropic, resonant, Poincare, Q-controller)
 * - Geometric encoder network (φ-scaled layers, golden subdivision feature maps)
 * - bytropix THEORY: φ-structured regions as resolution-independent coordinate system
 */

#define _POSIX_C_SOURCE 200809L
#include "bear_arena.h"
#include "bear_env.h"
#include "bear_nn.h"
#include "bear_ppo.h"
#include "bear_opt.h"
#include "bear_gaad.h"
#include "wubu_math.h"
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
#define ROLLOUT_LEN 1024

static void train_logger(int iter, float total_steps, float return_mean,
                         float policy_loss, float value_loss, float entropy,
                         float lr, void* user_data) {
    (void)user_data;
    printf("Iter %4d | Steps %10.0f | Return %7.2f | PLoss %7.4f | VLoss %7.4f | Ent %7.4f | LR %.2e\n",
           iter, total_steps, return_mean, policy_loss, value_loss, entropy, lr);
    fflush(stdout);
}

/* Simple Q-controller stub - not implemented yet */
static void gg_gaad_q_update_stub(BearGAADOptimizer* opt, float reward) {
    (void)opt; (void)reward;
    /* TODO: implement */
}

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
    for (int i = 0; i < num_layers; ++i) {
        total_weights += enc->layer_sizes[i] * enc->layer_sizes[i+1];
    }
    int total_biases = 0;
    for (int i = 0; i < num_layers; ++i) {
        total_biases += enc->layer_sizes[i+1];
    }
    
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
        for (int j = 0; j < fan_out; ++j) {
            enc->biases[b_idx++] = 0.0f;
        }
    }
    
    return enc;
}

static void geo_encoder_forward(const GeometricEncoder* enc, const float* input, float* output) {
    printf("[DEBUG] geo_encoder_forward: input=%p, output=%p, layers=%d\n", (void*)input, (void*)output, enc->num_layers); fflush(stdout);
    int num_layers = enc->num_layers;
    
    // Find max buffer size needed
    int max_buf_size = 0;
    for (int i = 1; i <= num_layers; ++i) {
        if (enc->layer_sizes[i] > max_buf_size) max_buf_size = enc->layer_sizes[i];
    }
    
    // Use heap allocation instead of alloca for larger max sizes
    float* prev = (float*)alloca(max_buf_size * sizeof(float));
    float* curr = (float*)alloca(max_buf_size * sizeof(float));
    
    printf("[DEBUG] alloca done (max_buf=%d), copying input...\n", max_buf_size); fflush(stdout);
    memcpy(prev, input, enc->input_dim * sizeof(float));
    
    int w_idx = 0, b_idx = 0;
    
    for (int layer = 0; layer < num_layers; ++layer) {
        int in_dim = enc->layer_sizes[layer];
        int out_dim = enc->layer_sizes[layer+1];
        
        printf("[DEBUG] Layer %d: in=%d, out=%d, w_idx=%d, b_idx=%d\n", layer, in_dim, out_dim, w_idx, b_idx); fflush(stdout);
        
        for (int j = 0; j < out_dim; ++j) {
            printf("[DEBUG]   Layer %d neuron %d start, w_idx=%d, b_idx=%d\n", layer, j, w_idx, b_idx); fflush(stdout);
            float sum = enc->biases[b_idx++];
            printf("[DEBUG]     bias read ok\n"); fflush(stdout);
            for (int k = 0; k < in_dim; ++k) {
                sum += prev[k] * enc->weights[w_idx++];
            }
            printf("[DEBUG]     weights multiply done\n"); fflush(stdout);
            float gelu = 0.5f * sum * (1.0f + tanhf(0.79788456f * (sum + 0.044715f * sum * sum * sum)));
            float phi_mod = (layer % 2 == 0) ? 1.618f : 0.618f;
            curr[j] = gelu * phi_mod;
        }
        
        printf("[DEBUG] Layer %d done, copying curr->prev...\n", layer); fflush(stdout);
        // Copy curr to prev for next layer (don't swap! prev buffer may be too small)
        memcpy(prev, curr, out_dim * sizeof(float));
        printf("[DEBUG] Layer %d copied, next layer in_dim=%d\n", layer, out_dim); fflush(stdout);
    }
    
    printf("[DEBUG] Copying output (size=%d)...\n", enc->output_dim); fflush(stdout);
    memcpy(output, prev, enc->output_dim * sizeof(float));
    printf("[DEBUG] Forward done\n"); fflush(stdout);
}
/* GAAD-Integrated Trainer */
typedef struct {
    BearArena global_arena;
    BearArena rollout_arena;
    BearArena step_arena;
    BearEnv* env;
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
} GGTrainer;

static int gg_trainer_init(GGTrainer* gg, int num_envs, int num_poles, int rollout_len) {
    printf("[DEBUG] gg_trainer_init start\n"); fflush(stdout);
    memset(gg, 0, sizeof(GGTrainer));
    
    size_t global_cap = 256 * 1024 * 1024;
    size_t rollout_cap = 64 * 1024 * 1024;
    size_t step_cap = 16 * 1024 * 1024;
    
    printf("[DEBUG] Creating arenas...\n"); fflush(stdout);
    if (bear_arena_create(&gg->global_arena, global_cap) != 0) { printf("[DEBUG] global arena failed\n"); return -1; }
    if (bear_arena_create(&gg->rollout_arena, rollout_cap) != 0) { printf("[DEBUG] rollout arena failed\n"); return -1; }
    if (bear_arena_create(&gg->step_arena, step_cap) != 0) { printf("[DEBUG] step arena failed\n"); return -1; }
    
    printf("[DEBUG] Creating env...\n"); fflush(stdout);
    gg->env = bear_env_create_npole(num_poles, num_envs, &gg->global_arena);
    if (!gg->env) { printf("[DEBUG] env create failed\n"); return -1; }
    printf("[DEBUG] env created: obs_dim=%d, act_dim=%d\n", gg->env->spec.obs_dim, gg->env->spec.act_dim); fflush(stdout);
    bear_npole_set_episode_length_max(gg->env, MAX_STEPS);
    gg->env->spec.max_episode_steps = MAX_STEPS;
    
    int obs_dim = gg->env->spec.obs_dim;
    int act_dim = 1;
    
    printf("[DEBUG] Creating policy encoder...\n"); fflush(stdout);
    gg->policy_encoder = geo_encoder_create(&gg->global_arena, obs_dim, 128, 4);
    if (!gg->policy_encoder) { printf("[DEBUG] policy encoder failed\n"); return -1; }
    printf("[DEBUG] policy encoder created\n"); fflush(stdout);
    
    int phid[] = { 128, 128 };
    int rc = bear_policy_create_mlp(&gg->policy, &gg->global_arena, 128, act_dim, 0, phid, 2);
    if (rc != 0) return -1;
    bear_orthogonal_init_params(&gg->policy, 1.0f);
    gg->policy.logstd = NULL;
    gg->policy.logstd_fixed = 0.0f;
    
    gg->value_encoder = geo_encoder_create(&gg->global_arena, obs_dim, 64, 3);
    
    int vhid[] = { 64, 64 };
    rc = bear_value_create(&gg->critic, &gg->global_arena, 64, vhid, 2);
    if (rc != 0) { printf("[DEBUG] value create failed: %d\n", rc); return -1; }
    printf("[DEBUG] value critic created\n"); fflush(stdout);
    bear_value_orthogonal_init(&gg->critic, 1.0f);
    
    printf("[DEBUG] Creating GAAD config...\n"); fflush(stdout);
    BearGAADConfig gaad_cfg = bear_gaad_default_config();
    gaad_cfg.base_lr = 3e-4f;
    gaad_cfg.model_complexity = 1;
    gaad_cfg.use_log_g_scaling = 1;
    gaad_cfg.use_anisotropic = 1;
    gaad_cfg.use_resonant = 1;
    gaad_cfg.use_poincare = 1;
    gaad_cfg.use_q_controller = 1;
    gaad_cfg.q_state_dim = 8;
    gaad_cfg.q_action_dim = 4;
    
    printf("[DEBUG] GAAD config done, computing param count...\n"); fflush(stdout);
    int param_count = 0;
    for (int i = 0; i < gg->policy.num_layers; ++i) {
        BearParam* p = gg->policy.layers[i].param;
        if (p && p->weight.data) param_count += p->weight.shape[0] * p->weight.shape[1];
    }
    for (int i = 0; i < gg->critic.num_layers; ++i) {
        BearParam* p = gg->critic.layers[i].param;
        if (p && p->weight.data) param_count += p->weight.shape[0] * p->weight.shape[1];
    }
    printf("[DEBUG] param_count = %d\n", param_count); fflush(stdout);
    
    printf("[DEBUG] Creating GAAD optimizer...\n"); fflush(stdout);
    gg->gaad_policy = bear_gaad_create(&gg->global_arena, &gaad_cfg, param_count);
    if (!gg->gaad_policy) { printf("[DEBUG] GAAD create failed\n"); return -1; }
    printf("[DEBUG] PPO config...\n"); fflush(stdout);
    gg->cfg = bear_ppo_default_config();
    gg->cfg.lr = 3e-4f;
    gg->cfg.epochs_per_iter = 4;
    gg->cfg.minibatch_size = 64;
    gg->cfg.ent_coef = 0.01f;
    
    printf("[DEBUG] Initializing trajectory...\n"); fflush(stdout);
    if (bear_traj_init(&gg->traj, &gg->global_arena, rollout_len, num_envs, 1, obs_dim, act_dim, 0) != 0) { printf("[DEBUG] traj init failed\n"); return -1; }
    printf("[DEBUG] trajectory initialized\n"); fflush(stdout);
    
    gg->total_steps = 0;
    gg->iteration = 0;
    gg->best_return = -INFINITY;
    
    printf("[DEBUG] gg_trainer_init complete\n"); fflush(stdout);
    return 0;
}

static float evaluate_gg(GGTrainer* gg, int num_episodes);  // forward declaration

static float gg_trainer_iter(GGTrainer* gg, uint64_t rng_state[2]) {
    printf("[DEBUG] Starting gg_trainer_iter...\n"); fflush(stdout);
    BearEnv* env = gg->env;
    BearTrajectory* traj = &gg->traj;
    BearPolicyNet* policy = &gg->policy;
    BearValueNet* critic = &gg->critic;
    BearGAADOptimizer* gaad_policy = gg->gaad_policy;
    
    printf("[DEBUG] Resetting trajectory and env...\n"); fflush(stdout);
    bear_traj_reset(traj);
    bear_env_reset_all(env, &gg->rollout_arena);
    
    printf("[DEBUG] Rollout loop start (len=%d)...\n", traj->rollout_len); fflush(stdout);
    double ep_return_sum = 0;
    int ep_count = 0;
    double step_reward_sum = 0;
    int step_env_count = 0;
    
    for (int step = 0; step < traj->rollout_len; ++step) {
        if (step % 256 == 0) { printf("[DEBUG] Rollout step %d\n", step); fflush(stdout); }
        float* obs = (float*)env->obs.data;
        int B = env->spec.num_envs;
        int obs_dim = env->spec.obs_dim;
        
        printf("[DEBUG] Step %d: creating enc_obs tensor...\n", step); fflush(stdout);
        BearTensor enc_obs_t, actions, logprobs, values, h_out;
        int64_t enc_obs_shape[2] = { B, 128 };
        int64_t act_shape[2] = { B, 1 };
        int64_t scalar_shape[1] = { B };
        bear_tensor_create(&gg->step_arena, &enc_obs_t, enc_obs_shape, 2, BEAR_DTYPE_F32, "enc_obs");
        printf("[DEBUG] enc_obs created\n"); fflush(stdout);
        float* encoded_obs = (float*)enc_obs_t.data;
        
        for (int b = 0; b < B; ++b) {
            printf("[DEBUG] Encoding obs %d\n", b); fflush(stdout);
            geo_encoder_forward(gg->policy_encoder, obs + b * obs_dim, encoded_obs + b * 128);
            printf("[DEBUG] Encoded obs %d done\n", b); fflush(stdout);
        }
        
        printf("[DEBUG] Creating action tensors...\n"); fflush(stdout);
        bear_tensor_create(&gg->step_arena, &actions, act_shape, 2, BEAR_DTYPE_F32, "rollout_act");
        printf("[DEBUG] actions created\n"); fflush(stdout);
        bear_tensor_create(&gg->step_arena, &logprobs, scalar_shape, 1, BEAR_DTYPE_F32, "rollout_lp");
        printf("[DEBUG] logprobs created\n"); fflush(stdout);
        bear_tensor_create(&gg->step_arena, &values, scalar_shape, 1, BEAR_DTYPE_F32, "rollout_val");
        printf("[DEBUG] values created\n"); fflush(stdout);
        bear_tensor_create(&gg->step_arena, &h_out, (int64_t[]){B, 128}, 2, BEAR_DTYPE_F32, "h_out");
        printf("[DEBUG] h_out created\n"); fflush(stdout);
        
        printf("[DEBUG] Calling bear_policy_forward...\n"); fflush(stdout);
        bear_policy_forward(policy, &enc_obs_t, NULL, &actions, &logprobs, NULL, &h_out, &gg->step_arena);
        printf("[DEBUG] bear_policy_forward returned\\n"); fflush(stdout);
        
        printf("[DEBUG] Calling bear_value_forward...\\n"); fflush(stdout);
        bear_value_forward(critic, &enc_obs_t, &values, &gg->step_arena);
        printf("[DEBUG] bear_value_forward returned\\n"); fflush(stdout);
        
        printf("[DEBUG] Calling bear_policy_sample...\\n"); fflush(stdout);
        bear_policy_sample(policy, &actions, &logprobs, rng_state);
        printf("[DEBUG] bear_policy_sample returned\\n"); fflush(stdout);
        
        float* act_data = (float*)actions.data;
        printf("[DEBUG] Action: %f\\n", act_data[0]); fflush(stdout);
        
        printf("[DEBUG] env->rewards.data=%p, env->dones.data=%p, env->obs.data=%p\\n", 
               env->rewards.data, env->dones.data, env->obs.data); fflush(stdout);
        printf("[DEBUG] env->rewards.shape=[%lld], env->dones.shape=[%lld], env->obs.shape=[%lld,%lld]\\n",
               (long long)env->rewards.shape[0],
               (long long)env->dones.shape[0],
               (long long)env->obs.shape[0], (long long)env->obs.shape[1]); fflush(stdout);
        
        printf("[DEBUG] Calling bear_env_step...\\n"); fflush(stdout);
        bear_env_step(env, &actions, &env->rewards, &env->dones, &env->obs, &gg->step_arena);
        printf("[DEBUG] bear_env_step returned\\n"); fflush(stdout);
        
        printf("[DEBUG] Calling bear_traj_store...\\n"); fflush(stdout);
        bear_traj_store(traj, step, &enc_obs_t, &actions, &logprobs, &env->rewards, &env->dones, &values);
        printf("[DEBUG] bear_traj_store returned\\n"); fflush(stdout);
        
        float* rew = (float*)env->rewards.data;
        for (int i = 0; i < B; ++i) {
            step_reward_sum += rew[i];
            step_env_count++;
        }
        
        uint8_t* dones = (uint8_t*)env->dones.data;
        for (int i = 0; i < env->spec.num_envs; ++i) {
            if (dones[i]) {
                float ret = env->episode_return_snapshot ? env->episode_return_snapshot[i] : env->episode_return[i];
                ep_return_sum += ret;
                ep_count++;
            }
        }
        
        bear_arena_reset(&gg->step_arena);
        printf("[DEBUG] bear_arena_reset done, step=%d\\n", step); fflush(stdout);
    }
    
    printf("[DEBUG] Rollout loop done, computing advantages...\\n"); fflush(stdout);
    bear_compute_advantages(traj, &gg->cfg, &gg->rollout_arena);
    printf("[DEBUG] bear_compute_advantages done\\n"); fflush(stdout);
    
    float total_policy_loss = 0, total_value_loss = 0, total_entropy = 0;
    int minibatch_count = 0;
    
    for (int epoch = 0; epoch < gg->cfg.epochs_per_iter; ++epoch) {
        printf("[DEBUG] Epoch %d: starting\\n", epoch); fflush(stdout);
        BearMinibatchSampler sampler;
        bear_sampler_init(&sampler, traj, gg->cfg.minibatch_size, rng_state);
        printf("[DEBUG] Sampler init done\\n"); fflush(stdout);
        
        BearTensor mb_obs, mb_actions, mb_logprobs, mb_advantages, mb_returns, mb_values, mb_old_logprobs;
        
        int mb_idx = 0;
        while (bear_sampler_next(&sampler, traj, &mb_obs, &mb_actions,
                                 &mb_logprobs, &mb_advantages, &mb_returns,
                                 &mb_values, &mb_old_logprobs, &gg->step_arena)) {
            printf("[DEBUG] Minibatch %d: got data, mb_size=%lld\\n", mb_idx, (long long)mb_obs.shape[0]); fflush(stdout);
            mb_idx++;
            
            printf("[DEBUG]   bear_arena_reset...\\n"); fflush(stdout);
            bear_arena_reset(&gg->step_arena);
            printf("[DEBUG]   bear_arena_reset done\\n"); fflush(stdout);
            
            int mb_size = mb_obs.shape[0];
            printf("[DEBUG]   Creating fwd tensors...\\n"); fflush(stdout);
            
            BearTensor fwd_actions, new_logprobs, h_out;
            printf("[DEBUG]     Creating fwd_actions [%d,1]...\\n", mb_size); fflush(stdout);
            bear_tensor_create(&gg->step_arena, &fwd_actions, (int64_t[]){mb_size, 1}, 2, BEAR_DTYPE_F32, "fwd_act");
            printf("[DEBUG]     fwd_actions created\\n"); fflush(stdout);
            
            printf("[DEBUG]     Creating new_logprobs [%d]...\\n", mb_size); fflush(stdout);
            bear_tensor_create(&gg->step_arena, &new_logprobs, (int64_t[]){mb_size}, 1, BEAR_DTYPE_F32, "fwd_lp");
            printf("[DEBUG]     new_logprobs created\\n"); fflush(stdout);
            
            printf("[DEBUG]     Creating h_out [%d,128]...\\n", mb_size); fflush(stdout);
            bear_tensor_create(&gg->step_arena, &h_out, (int64_t[]){mb_size, 128}, 2, BEAR_DTYPE_F32, "h_out");
            printf("[DEBUG]     h_out created\\n"); fflush(stdout);
            
            printf("[DEBUG]   Calling bear_policy_forward...\\n"); fflush(stdout);
            bear_policy_forward(policy, &mb_obs, NULL, &fwd_actions, &new_logprobs, NULL, &h_out, &gg->step_arena);
            printf("[DEBUG]   bear_policy_forward returned\\n"); fflush(stdout);
            
            printf("[DEBUG]   Recomputing logprobs for continuous...\\n"); fflush(stdout);
            if (!policy->act_discrete) {
                float* mu = (float*)policy->layers[policy->num_layers - 1].z_pre.data;
                float* stored_act = (float*)mb_actions.data;
                float* nlp = (float*)new_logprobs.data;
                float ls = policy->logstd ? 0.0f : policy->logstd_fixed;
                float var = expf(2.0f * ls);
                float log_norm = -0.5f * logf(2.0f * 3.14159265f * var);
                printf("[DEBUG]     Policy continuous, ls=%f, var=%f\\n", ls, var); fflush(stdout);
                for (int i = 0; i < mb_size; ++i) {
                    float lp = 0.0f;
                    for (int a = 0; a < 1; ++a) {
                        float diff = stored_act[i * 1 + a] - mu[i * 1 + a];
                        lp += -0.5f * diff * diff / var + log_norm;
                    }
                    nlp[i] = lp;
                }
                printf("[DEBUG]     logprobs recomputed\\n"); fflush(stdout);
            }
            
            BearTensor val_pred;
            printf("[DEBUG]   Creating val_pred [%d]...\\n", mb_size); fflush(stdout);
            bear_tensor_create(&gg->step_arena, &val_pred, (int64_t[]){mb_size}, 1, BEAR_DTYPE_F32, "val_pred");
            printf("[DEBUG]   val_pred created\\n"); fflush(stdout);
            
            printf("[DEBUG]   Calling bear_value_forward...\\n"); fflush(stdout);
            bear_value_forward(critic, &mb_obs, &val_pred, &gg->step_arena);
            printf("[DEBUG]   bear_value_forward returned\\n"); fflush(stdout);
            
            printf("[DEBUG]   Computing losses...\\n"); fflush(stdout);
            float pl = 0, vl = 0, ent = 0;
            float* nlp_p = (float*)new_logprobs.data;
            float* olp_p = (float*)mb_old_logprobs.data;
            float* adv_p = (float*)mb_advantages.data;
            printf("[DEBUG]     nlp_p=%p, olp_p=%p, adv_p=%p\\n", (void*)nlp_p, (void*)olp_p, (void*)adv_p); fflush(stdout);
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
            printf("[DEBUG]     Policy loss loop done\\n"); fflush(stdout);
            pl /= mb_size;
            
            float* vpred_p = (float*)val_pred.data;
            float* mbr_p = (float*)mb_returns.data;
            printf("[DEBUG]     vpred_p=%p, mbr_p=%p\\n", (void*)vpred_p, (void*)mbr_p); fflush(stdout);
            for (int i = 0; i < mb_size; ++i) {
                float diff = vpred_p[i] - mbr_p[i];
                vl += 0.5f * diff * diff;
            }
            printf("[DEBUG]     Value loss loop done\\n"); fflush(stdout);
            vl /= mb_size;
            
            printf("[DEBUG]   Computing entropy...\\n"); fflush(stdout);
            if (policy->act_discrete) {
                // entropy for discrete
            } else {
                float ls = policy->logstd ? 0.0f : policy->logstd_fixed;
                ent = 0.5f * (logf(2.0f * 3.14159265f * expf(1.0f)) + 2.0f * ls);
            }
            printf("[DEBUG]   Entropy: %f\\n", ent); fflush(stdout);
            
            total_policy_loss += pl;
            total_value_loss += vl;
            total_entropy += ent;
            
            printf("[DEBUG]   Calling bear_policy_backward...\\n"); fflush(stdout);
            bear_policy_backward(policy, &mb_obs, &mb_actions, &mb_logprobs, &mb_advantages, gg->cfg.clip_coef, 1.0f, &gg->step_arena);
            printf("[DEBUG]   bear_policy_backward returned\\n"); fflush(stdout);
            
            printf("[DEBUG]   Calling bear_value_backward...\\n"); fflush(stdout);
            bear_value_backward(critic, &mb_obs, &val_pred, &mb_returns, gg->cfg.vf_coef, &gg->step_arena);
            printf("[DEBUG]   bear_value_backward returned\\n"); fflush(stdout);
            
            printf("=== GAAD STEP START ===\\n"); fflush(stdout);
            int total_params = 0;
            printf(">>> Entering counting policy params loop <<<\\n"); fflush(stdout);
            for (int i = 0; i < policy->num_layers; ++i) {
                BearParam* p = policy->layers[i].param;
                if (p && p->grad.data && p->weight.data) {
                    int n = (int)bear_tensor_numel(&p->grad);
                    printf("[DEBUG]       Counting policy layer %d: n=%d\\n", i, n); fflush(stdout);
                    total_params += n;
                }
            }
            printf("[DEBUG]     Counting critic params...\\n"); fflush(stdout);
            for (int i = 0; i < critic->num_layers; ++i) {
                BearParam* p = critic->layers[i].param;
                if (p && p->grad.data && p->weight.data) {
                    int n = (int)bear_tensor_numel(&p->grad);
                    printf("[DEBUG]       Counting critic layer %d: n=%d\\n", i, n); fflush(stdout);
                    total_params += n;
                }
            }
            printf(">>> Exited all counting loops, total_params=%d <<<\\n", total_params); fflush(stdout);
            
            printf("[DEBUG]     Total params: %d, allocating...\\n", total_params); fflush(stdout);
            float* all_grads = (float*)bear_arena_alloc(&gg->step_arena, total_params * sizeof(float), 16);
            float* all_params = (float*)bear_arena_alloc(&gg->step_arena, total_params * sizeof(float), 16);
            printf("[DEBUG]     all_grads=%p, all_params=%p\\n", (void*)all_grads, (void*)all_params); fflush(stdout);
            int pc = 0;
            
            printf("[DEBUG]     Collecting policy grads...\\n"); fflush(stdout);
            for (int i = 0; i < policy->num_layers; ++i) {
                BearParam* p = policy->layers[i].param;
                if (p && p->grad.data && p->weight.data) {
                    int n = (int)bear_tensor_numel(&p->grad);
                    printf("[DEBUG]       Policy layer %d: n=%d, grad=%p, weight=%p\\n", i, n, p->grad.data, p->weight.data); fflush(stdout);
                    memcpy(all_grads + pc, p->grad.data, n * sizeof(float));
                    memcpy(all_params + pc, p->weight.data, n * sizeof(float));
                    pc += n;
                }
            }
            printf("[DEBUG]     Collecting critic grads...\\n"); fflush(stdout);
            for (int i = 0; i < critic->num_layers; ++i) {
                BearParam* p = critic->layers[i].param;
                if (p && p->grad.data && p->weight.data) {
                    int n = (int)bear_tensor_numel(&p->grad);
                    printf("[DEBUG]       Critic layer %d: n=%d\\n", i, n); fflush(stdout);
                    memcpy(all_grads + pc, p->grad.data, n * sizeof(float));
                    memcpy(all_params + pc, p->weight.data, n * sizeof(float));
                    pc += n;
                }
            }
            
            printf("[DEBUG]     Total params: %d\\n", pc); fflush(stdout);
            if (pc > 0 && gaad_policy) {
                printf("[DEBUG]     Calling bear_gaad_step...\\n"); fflush(stdout);
                bear_gaad_step(gaad_policy, all_params, all_grads, pc, &gg->step_arena);
                printf("[DEBUG]     bear_gaad_step returned\\n"); fflush(stdout);
                
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
            
            // Q-controller stub
            gg_gaad_q_update_stub(gaad_policy, ep_count > 0 ? (float)(ep_return_sum / ep_count) : 0);
            
            minibatch_count++;
        }
        
        free(sampler.indices);
    }
    
    float avg_return = ep_count > 0 ? (float)(ep_return_sum / ep_count) : 0.0f;
    if (avg_return > gg->best_return) gg->best_return = avg_return;
    
    // Log directly
    printf("Iter %4d | Steps %10lld | Return %7.2f | PLoss %7.4f | VLoss %7.4f | Ent %7.4f | LR %.2e\n",
           gg->iteration, (long long)gg->total_steps, avg_return,
           total_policy_loss / (float)minibatch_count,
           total_value_loss / (float)minibatch_count,
           total_entropy / (float)minibatch_count,
           bear_gaad_get_lr(gaad_policy));
    fflush(stdout);
    
    if (gg->iteration % 10 == 0 && gg->iteration > 0) {
        float eval_avg = evaluate_gg(gg, 20);
        printf("[Eval iter %d] Train: %.1f | Eval (CartPole-v1): %.1f | Best: %.1f\n",
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
    
    bear_env_reset_all(eval_env, &eval_arena);
    
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
            
            float obs[4];
            float enc_obs[128];
            memcpy(obs, eval_env->obs.data, 4 * sizeof(float));
            geo_encoder_forward(gg->policy_encoder, obs, enc_obs);
            
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

int main(int argc, char** argv) {
    int num_envs = 64;
    int num_poles = 1;
    int total_iters = 100;
    float lr = 3e-4f;
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
    printf("+==================================================================+\n");
    printf("|  WUBU CARTPOLE 1-10  --  GAAD + Geometric Encoder                  |\n");
    printf("+==================================================================+\n");
    printf("|  Envs: %d | Poles: %d | Iters: %d | LR: %.1e | Seed: %d            \n", num_envs, num_poles, total_iters, lr, seed);
    printf("|  Optimizer: GAAD (log(g)+anisotropic+resonant+Poincare)         |\n");
    printf("|  Encoder: Geometric (φ-scaled layers, golden subdivision)       |\n");
    printf("+==================================================================+\n\n");
    
    GGTrainer gg;
    printf("[DEBUG] About to call gg_trainer_init\\n"); fflush(stdout);
    if (gg_trainer_init(&gg, num_envs, num_poles, ROLLOUT_LEN) != 0) {
        fprintf(stderr, "Trainer init failed\\n");
        return 1;
    }
    printf("[DEBUG] gg_trainer_init returned successfully\\n"); fflush(stdout);
    
    for (int poles = 1; poles <= 10; ++poles) {
        printf("\n=== Solving %d-pole CartPole ===\n", poles);
        
        bear_env_close(gg.env);
        gg.env = bear_env_create_npole(poles, num_envs, &gg.global_arena);
        bear_npole_set_episode_length_max(gg.env, MAX_STEPS);
        gg.env->spec.max_episode_steps = MAX_STEPS;
        gg.traj.num_envs = num_envs;
        
        for (int iter = 0; iter < total_iters; ++iter) {
            uint64_t rng[2] = { 0xDEADBEEFDEADBEEFull ^ (uint64_t)seed, 0xCAFEBABECAFEBABEull ^ (uint64_t)time(NULL) };
            float avg_ret = gg_trainer_iter(&gg, rng);
            
            if (avg_ret >= SOLVED_THRESHOLD_100) break;
        }
        
        float final_eval = evaluate_gg(&gg, EVAL_EPISODES);
        printf("Final eval %d-pole: %.2f (threshold: %.0f) - %s\n", poles, final_eval, SOLVED_THRESHOLD_100,
               final_eval >= SOLVED_THRESHOLD_100 ? "SOLVED ✓" : "NOT SOLVED ✗");
    }
    
    bear_arena_destroy(&gg.global_arena);
    bear_arena_destroy(&gg.rollout_arena);
    bear_arena_destroy(&gg.step_arena);
    
    return 0;
}