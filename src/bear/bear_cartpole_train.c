/*
 * bear_cartpole_train.c  --  Pure C11 CartPole-v1 Trainer using N-Pole (1 pole)
 * 
 * Uses BEAR_ENV_NPOLE with 1 pole - exactly matches Gymnasium CartPole-v1:
 * - cart_mass=1.0, pole_mass=0.1, pole_length=1.0 (half=0.5)
 * - gravity=9.81, dt=0.02, force_mag=10.0
 * - angle_threshold=12° (0.20944 rad), x_threshold=2.4
 * - obs: [x, vx, sinθ, cosθ, ω] (5D), action: continuous force [-10, +10]
 * 
 * Compile: make bear_cartpole_train (in WuBuOS root)
 * Run: ./src/bear/bear_cartpole_train --envs 64 --iters 500 --lr 3e-4
 */

#define _POSIX_C_SOURCE 200809L
#include "bear_arena.h"
#include "bear_env.h"
#include "bear_nn.h"
#include "bear_ppo.h"
#include "bear_opt.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <string.h>

/* Simple logger */
static void train_logger(int iter, float total_steps, float return_mean,
                         float policy_loss, float value_loss, float entropy,
                         float lr, void* user_data) {
    (void)user_data;
    printf("Iter %4d | Steps %10.0f | Return %7.2f | PLoss %7.4f | VLoss %7.4f | Ent %7.4f | LR %.2e\n",
           iter, total_steps, return_mean, policy_loss, value_loss, entropy, lr);
    fflush(stdout);
}

/* --- Main --- */
int main(int argc, char** argv) {
    int num_envs = 64;
    int num_poles = 1;
    int total_iters = 500;
    int rollout_len = 2048;
    float lr = 3e-4f;
    int epochs = 10;
    int minibatch = 64;
    int seed = (int)time(NULL);
    int eval_interval = 25;
    const char* save_path = "models/cartpole_npole.bear";
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--envs") == 0 && i + 1 < argc) num_envs = atoi(argv[++i]);
        else if (strcmp(argv[i], "--poles") == 0 && i + 1 < argc) num_poles = atoi(argv[++i]);
        else if (strcmp(argv[i], "--iters") == 0 && i + 1 < argc) total_iters = atoi(argv[++i]);
        else if (strcmp(argv[i], "--rollout") == 0 && i + 1 < argc) rollout_len = atoi(argv[++i]);
        else if (strcmp(argv[i], "--lr") == 0 && i + 1 < argc) lr = atof(argv[++i]);
        else if (strcmp(argv[i], "--epochs") == 0 && i + 1 < argc) epochs = atoi(argv[++i]);
        else if (strcmp(argv[i], "--mb") == 0 && i + 1 < argc) minibatch = atoi(argv[++i]);
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = atoi(argv[++i]);
        else if (strcmp(argv[i], "--eval-interval") == 0 && i + 1 < argc) eval_interval = atoi(argv[++i]);
        else if (strcmp(argv[i], "--save") == 0 && i + 1 < argc) save_path = argv[++i];
        else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("  --envs N           Parallel environments (default: 64)\n");
            printf("  --poles N          Number of poles (default: 1 = CartPole-v1)\n");
            printf("  --iters N          Training iterations (default: 500)\n");
            printf("  --rollout N        Rollout length (default: 2048)\n");
            printf("  --lr F             Learning rate (default: 3e-4)\n");
            printf("  --epochs N         PPO epochs per iter (default: 10)\n");
            printf("  --mb N             Minibatch size (default: 64)\n");
            printf("  --seed N           Random seed (default: time)\n");
            printf("  --eval-interval N  Evaluate every N iters (default: 25)\n");
            printf("  --save PATH        Save path (default: models/cartpole_npole.bear)\n");
            return 0;
        }
    }
    
    if (num_envs > 256) num_envs = 256;
    if (num_envs < 1) num_envs = 1;
    if (num_poles > 10) num_poles = 10;
    if (num_poles < 1) num_poles = 1;
    
    /* RNG */
    uint64_t rng_state[2] = { 0xDEADBEEFDEADBEEFull ^ seed, 0xCAFEBABECAFEBABEull ^ (uint64_t)time(NULL) };
    
    printf("+==================================================================+\n");
    printf("|  WUBU CARTPOLE-v1  --  N-Pole (%d pole)  --  Pure C11 Bear Stack      |\n", num_poles);
    printf("+==================================================================+\n");
    printf("|  Envs: %d | Poles: %d | Iters: %d | Rollout: %d                |\n", num_envs, num_poles, total_iters, rollout_len);
    printf("|  LR: %.1e | Epochs: %d | Minibatch: %d | Seed: %d             |\n", lr, epochs, minibatch, seed);
    printf("+==================================================================+\n\n");
    
    /* --- Arenas --- */
    size_t global_cap = 128 * 1024 * 1024;
    size_t rollout_cap = 64 * 1024 * 1024;
    size_t step_cap = 8 * 1024 * 1024;
    
    BearArena global_arena, rollout_arena, step_arena;
    if (bear_arena_create(&global_arena, global_cap) != 0) return 1;
    if (bear_arena_create(&rollout_arena, rollout_cap) != 0) return 1;
    if (bear_arena_create(&step_arena, step_cap) != 0) return 1;
    
    /* --- Create N-Pole Environment (1 pole = CartPole-v1) --- */
    printf("Creating N-pole CartPole (%d poles, %d envs)...\n", num_poles, num_envs);
    BearEnv* env = bear_env_create_npole(num_poles, num_envs, &global_arena);
    if (!env) {
        fprintf(stderr, "Failed to create environment\n");
        return 1;
    }
    printf("Environment: obs_dim=%d (x, vx, sinθ, cosθ, ω...), act_dim=%d (continuous force), max_steps=%d\n",
           env->spec.obs_dim, env->spec.act_dim, env->spec.max_episode_steps);
    
    bear_env_reset_all(env, &global_arena);
    
    /* --- Create MLP Policy (continuous Gaussian) --- */
    BearPolicyNet policy;
    int phid[] = { 128, 128 };
    int rc = bear_policy_create_mlp(&policy, &global_arena,
                                    env->spec.obs_dim, env->spec.act_dim,
                                    env->spec.act_discrete, phid, 2);
    if (rc != 0) {
        fprintf(stderr, "Policy creation failed: %d\n", rc);
        return 1;
    }
    bear_orthogonal_init_params(&policy, 1.0f);
    /* Gaussian policy: logstd = log(1.0) = 0 */
    policy.logstd = NULL;
    policy.logstd_fixed = 0.0f;
    
    /* --- Create Value Network --- */
    BearValueNet critic;
    int vhid[] = { 128, 128 };
    rc = bear_value_create(&critic, &global_arena, env->spec.obs_dim, vhid, 2);
    if (rc != 0) {
        fprintf(stderr, "Value creation failed: %d\n", rc);
        return 1;
    }
    bear_value_orthogonal_init(&critic, 1.0f);
    
    /* --- PPO Config matching SB3 CartPole --- */
    BearPPOConfig cfg = bear_ppo_default_config();
    cfg.lr = lr;
    cfg.epochs_per_iter = epochs;
    cfg.minibatch_size = minibatch;
    cfg.gamma = 0.99f;
    cfg.gae_lambda = 0.95f;
    cfg.clip_coef = 0.2f;
    cfg.clip_coef_vf = 0.2f;
    cfg.vf_coef = 0.5f;
    cfg.ent_coef = 0.0f;
    cfg.target_kl = 0.02f;
    cfg.lr_anneal = 1;
    cfg.normalize_adv = 1;
    cfg.normalize_obs = 1;
    cfg.max_grad_norm = 0.5f;
    cfg.use_vtrace = 0;
    cfg.normalize_rewards = 1;
    
    /* --- Optimizers --- */
    BearOptimizer* opt_policy = bear_optimizer_create(&global_arena, BEAR_OPT_ADAM, lr);
    BearOptimizer* opt_critic = bear_optimizer_create(&global_arena, BEAR_OPT_ADAM, lr);
    
    /* Register parameters */
    for (int i = 0; i < policy.num_layers; ++i) {
        BearParam* p = policy.layers[i].param;
        if (p && p->weight.data) bear_optimizer_register(opt_policy, p);
    }
    for (int i = 0; i < critic.num_layers; ++i) {
        BearParam* p = critic.layers[i].param;
        if (p && p->weight.data) bear_optimizer_register(opt_critic, p);
    }
    
    /* --- Trainer --- */
    BearTrainer trainer;
    rc = bear_trainer_init(&trainer, &policy, &critic, env, &cfg,
                           global_cap, rollout_cap, step_cap);
    if (rc != 0) {
        fprintf(stderr, "Trainer init failed: %d\n", rc);
        return 1;
    }
    
    trainer.global_arena = global_arena;
    trainer.rollout_arena = rollout_arena;
    trainer.step_arena = step_arena;
    trainer.opt_policy = opt_policy;
    trainer.opt_critic = opt_critic;
    
    bear_trainer_set_logger(&trainer, train_logger, NULL);
    
    printf("\nStarting training (%d iters = ~%d steps)...\n\n", 
           total_iters, total_iters * num_envs * rollout_len);
    printf("Target: 10,000 steps (CartPole-v1 max with N-pole physics)\n\n");
    
    float best_return = -INFINITY;
    int stagnation = 0;
    
    for (int iter = 0; iter < total_iters; ++iter) {
        float avg_return = bear_trainer_iter(&trainer, rng_state);
        
        /* LR anneal */
        if (cfg.lr_anneal) {
            float frac = 1.0f - (float)trainer.total_steps / (float)(total_iters * num_envs * rollout_len);
            float new_lr = lr * fmaxf(frac, 0.1f);
            bear_optimizer_set_lr(opt_policy, new_lr);
            bear_optimizer_set_lr(opt_critic, new_lr);
        }
        
        if (avg_return > best_return) {
            best_return = avg_return;
            stagnation = 0;
        } else {
            stagnation++;
        }
        
        /* Periodic evaluation */
        if (iter % eval_interval == 0 && iter > 0) {
            size_t prev_used = step_arena.used;
            float eval_sum = 0.0f;
            
            for (int ep = 0; ep < 10; ++ep) {
                bear_env_reset_all(env, &step_arena);
                float ep_ret = 0.0f;
                
                for (int step = 0; step < env->spec.max_episode_steps; ++step) {
                    BearTensor act, logp, val, h_out;
                    int64_t B = num_envs;
                    int64_t act_shape[2] = { B, env->spec.act_dim };
                    int64_t scalar_shape[1] = { B };
                    int64_t h_shape[2] = { B, policy.hid_size };
                    
                    bear_tensor_create(&step_arena, &act, act_shape, 2, BEAR_DTYPE_F32, "eval.act");
                    bear_tensor_create(&step_arena, &logp, scalar_shape, 1, BEAR_DTYPE_F32, "eval.logp");
                    bear_tensor_create(&step_arena, &val, scalar_shape, 1, BEAR_DTYPE_F32, "eval.val");
                    bear_tensor_create(&step_arena, &h_out, h_shape, 2, BEAR_DTYPE_F32, "eval.h_out");
                    
                    bear_policy_forward(&policy, &env->obs, NULL, &act, &logp, &val, &h_out, &step_arena);
                    bear_policy_deterministic(&policy, &act);  /* Deterministic for eval */
                    
                    BearTensor rew, done, next_obs;
                    bear_tensor_create(&step_arena, &rew, scalar_shape, 1, BEAR_DTYPE_F32, "eval.rew");
                    bear_tensor_create(&step_arena, &done, scalar_shape, 1, BEAR_DTYPE_U8, "eval.done");
                    bear_tensor_create(&step_arena, &next_obs, (int64_t[]){B, env->spec.obs_dim}, 2, BEAR_DTYPE_F32, "eval.next_obs");
                    
                    env->step(env, &act, &rew, &done, &next_obs, &step_arena);
                    
                    for (int e = 0; e < num_envs; ++e) {
                        if (ep == 0) ep_ret += ((float*)rew.data)[e];
                    }
                    
                    if (((uint8_t*)done.data)[0]) break;
                    memcpy(env->obs.data, next_obs.data, num_envs * env->spec.obs_dim * sizeof(float));
                    bear_arena_reset(&step_arena);
                }
                eval_sum += ep_ret;
            }
            
            float eval_avg = eval_sum / 10.0f;
            printf("\n[Eval iter %d] Best: %.2f | Steps: %d | 10-ep det avg: %.2f (target: ~10000)\n\n", 
                   iter, best_return, trainer.total_steps, eval_avg);
        }
        
        /* Progress */
        if (iter % 5 == 0) {
            printf("Iter %4d | Steps %8d | Return %6.1f | Best %6.1f | Global %.1f%%\n",
                   iter, trainer.total_steps, avg_return, best_return,
                   100.0f * trainer.global_arena.used / trainer.global_arena.cap);
        }
        
        /* Early stop if solved (N-pole target is 10000, but early stop at 5000) */
        if (best_return >= 5000.0f) {
            printf("\n=== SOLVED! Best return %.2f >= 5000 ===\n", best_return);
            break;
        }
        
        if (stagnation > 200) {
            printf("\nStagnation at iter %d, best=%.2f\n", iter, best_return);
            break;
        }
    }
    
    printf("\nTraining complete! Best return: %.2f\n", best_return);
    
    /* Save checkpoints */
    if (bear_policy_save(&policy, save_path) == 0) {
        printf("Policy saved to %s\n", save_path);
    }
    if (bear_value_save(&critic, "models/cartpole_value.bear") == 0) {
        printf("Value saved to models/cartpole_value.bear\n");
    }
    
    /* Cleanup */
    bear_arena_destroy(&global_arena);
    bear_arena_destroy(&rollout_arena);
    bear_arena_destroy(&step_arena);
    
    return 0;
}