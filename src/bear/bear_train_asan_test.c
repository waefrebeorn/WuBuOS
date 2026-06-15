/*
 * bear_train_asan_test.c  --  ASAN test for 64-env segfault with large networks
 * Config: 64 envs, 64x64 hidden, 10 epochs, 4096 minibatch
 */

#include "bear_arena.h"
#include "bear_env.h"
#include "bear_nn.h"
#include "bear_opt.h"
#include "bear_ppo.h"
#include "bear_simd.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

static void train_logger(int iter, float total_steps, float return_mean,
                         float policy_loss, float value_loss, float entropy,
                         float lr, void* user_data) {
    (void)user_data;
    printf("Iter %4d | Steps %.0f | Return %8.2f | PLoss %.4f | VLoss %.4f | Ent %.4f | LR %.2e\n",
           iter, total_steps, return_mean, policy_loss, value_loss, entropy, lr);
}

int main(int argc, char** argv) {
    int num_poles = 7;
    int num_envs = 64;
    int total_iters = 10;

    if (argc > 1) num_poles = atoi(argv[1]);
    if (argc > 2) num_envs = atoi(argv[2]);
    if (argc > 3) total_iters = atoi(argv[3]);

    if (num_poles < 1) num_poles = 1;
    if (num_poles > 10) num_poles = 10;
    if (num_envs < 1) num_envs = 1;
    if (num_envs > 4096) num_envs = 4096;

    printf("===== BearRL N-Pole Cartpole ASAN Test =====\n");
    printf("Poles: %d | Envs: %d | Target Iters: %d\n", num_poles, num_envs, total_iters);
    printf("Config: 64x64 hidden, 10 epochs, 4096 minibatch\n");
    printf("Sovereign C11 RL - No Python, No PyTorch, No Gym\n");
    fflush(stdout);

    uint64_t rng_state[2] = { 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull ^ (uint64_t)time(NULL) };

    /* Arena capacities - LARGER for 64x64 hidden + 64 envs */
    const size_t global_arena_cap = 512 * 1024 * 1024;   /* 512 MB for weights + traj */
    const size_t rollout_arena_cap = 64 * 1024 * 1024;   /* 64 MB for rollout temp */
    const size_t step_arena_cap = 128 * 1024 * 1024;     /* 128 MB per-step temp */

    BearArena global_arena;
    int arena_rc = bear_arena_create(&global_arena, global_arena_cap);
    if (arena_rc != 0) {
        fprintf(stderr, "Failed to create global arena\n");
        return 1;
    }

    printf("Creating %d-pole cartpole with %d vectorized envs...\n", num_poles, num_envs);
    fflush(stdout);
    BearEnv* env = bear_env_create_npole(num_poles, num_envs, &global_arena);
    if (!env) {
        fprintf(stderr, "Failed to create environment\n");
        return 1;
    }
    printf("Environment created: obs_dim=%d, act_dim=%d, continuous=%d\n",
           env->spec.obs_dim, env->spec.act_dim, !env->spec.act_discrete);
    fflush(stdout);

    /* Create policy network with 64x64 hidden */
    printf("Creating MLP policy network (64, 64)...\n");
    fflush(stdout);
    BearPolicyNet policy;
    int phid[] = { 64, 64 };
    int policy_rc = bear_policy_create_mlp(&policy, &global_arena,
                                   env->spec.obs_dim, env->spec.act_dim,
                                   env->spec.act_discrete, phid, 2);
    if (policy_rc != 0) {
        fprintf(stderr, "Failed to create policy network (rc=%d)\n", policy_rc);
        return 1;
    }
    printf("Policy network: MLP 64x64\n");
    bear_orthogonal_init_params(&policy, 1.0f);
    policy.logstd = NULL;
    policy.logstd_fixed = logf(0.5f);

    /* Create value network with 64x64 hidden */
    printf("Creating value network (64, 64)...\n");
    fflush(stdout);
    BearValueNet critic;
    int vhid[] = { 64, 64 };
    int critic_rc = bear_value_create(&critic, &global_arena,
                           env->spec.obs_dim, vhid, 2);
    if (critic_rc != 0) {
        fprintf(stderr, "Failed to create value network (rc=%d)\n", critic_rc);
        return 1;
    }
    bear_value_orthogonal_init(&critic, 1.0f);

    /* PPO Configuration - PROBLEMATIC CONFIG */
    BearPPOConfig cfg = bear_ppo_default_config();
    cfg.lr = 3e-4f;
    cfg.epochs_per_iter = 10;      /* <-- 10 epochs */
    cfg.minibatch_size = 4096;     /* <-- 4096 minibatch */
    cfg.gamma = 0.99f;
    cfg.gae_lambda = 0.95f;
    cfg.clip_coef = 0.2f;
    cfg.vf_coef = 0.5f;
    cfg.ent_coef = 0.01f;
    cfg.target_kl = 0.02f;
    cfg.lr_anneal = 1;
    cfg.normalize_rewards = 1;

    /* Initialize trainer */
    printf("Initializing trainer...\n");
    fflush(stdout);
    BearTrainer trainer;
    int trainer_rc = bear_trainer_init(&trainer, &policy, &critic, env, &cfg,
                           global_arena_cap, rollout_arena_cap, step_arena_cap);
    if (trainer_rc != 0) {
        fprintf(stderr, "Failed to initialize trainer (rc=%d)\n", trainer_rc);
        return 1;
    }

    bear_trainer_set_logger(&trainer, train_logger, NULL);

    printf("\nStarting training...\n");
    fflush(stdout);
    printf("Iter   Steps        Return    PLoss    VLoss    Ent      LR\n");
    fflush(stdout);
    printf("---------------------------------------------------------------\n");
    fflush(stdout);

    float best_return = -INFINITY;
    int stagnation = 0;

    for (int iter = 0; iter < total_iters; ++iter) {
        float avg_return = bear_trainer_iter(&trainer, rng_state);

        if (avg_return > best_return) {
            best_return = avg_return;
            stagnation = 0;
        } else {
            stagnation++;
        }

        /* Episode length curriculum */
        if (iter > 0 && iter % 100 == 0) {
            int ep_max = bear_npole_get_episode_length_max(env);
            if (ep_max > 0 && best_return > ep_max * 0.5f) {
                int new_max = ep_max + 500;
                if (new_max > 10000) new_max = 10000;
                if (new_max > ep_max) {
                    bear_npole_set_episode_length_max(env, new_max);
                    printf("[Curriculum] Episode length increased to %d\n", new_max);
                }
            }
        }

        if (avg_return >= 9500.0f) {
            printf("\n=== SOLVED! Average return %.2f >= 9500 ===\n", avg_return);
            break;
        }

        if (stagnation > 500) {
            printf("\n[Stagnation] No improvement for 500 iters, stopping\n");
            break;
        }
    }

    printf("\nTraining complete!\n");
    printf("Best return: %.2f\n", best_return);
    printf("Total steps: %d\n", trainer.total_steps);

    bear_env_close(env);
    bear_arena_destroy(&trainer.global_arena);
    bear_arena_destroy(&trainer.rollout_arena);
    bear_arena_destroy(&trainer.step_arena);
    bear_arena_destroy(&global_arena);

    return 0;
}
