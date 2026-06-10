/*
 * bear_train.c — BearRL Training Entry Point
 * 
 * Sovereign C11 RL training for N-Pole Cartpole (7-10 poles)
 * No Python, no PyTorch, no Gym. Pure metal.
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

/* Simple logger callback */
static void train_logger(int iter, float total_steps, float return_mean,
                         float policy_loss, float value_loss, float entropy,
                         float lr, void* user_data) {
    (void)user_data;
    printf("Iter %4d | Steps %.0f | Return %8.2f | PLoss %.4f | VLoss %.4f | Ent %.4f | LR %.2e\n",
           iter, total_steps, return_mean, policy_loss, value_loss, entropy, lr);
}

int main(int argc, char** argv) {
    int num_poles = 7;
    int num_envs = 1024;
    int total_iters = 10000;

    if (argc > 1) num_poles = atoi(argv[1]);
    if (argc > 2) num_envs = atoi(argv[2]);
    if (argc > 3) total_iters = atoi(argv[3]);

    if (num_poles < 1) num_poles = 1;
    if (num_poles > 10) num_poles = 10;
    if (num_envs < 1) num_envs = 1;
    if (num_envs > 4096) num_envs = 4096;
    printf("===== BearRL N-Pole Cartpole Training =====\n");
    printf("Poles: %d | Envs: %d | Target Iters: %d\n", num_poles, num_envs, total_iters);
    printf("Sovereign C11 RL - No Python, No PyTorch, No Gym\n");
    fflush(stdout);

    /* RNG state for reproducibility */
    uint64_t rng_state[2] = { 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull ^ (uint64_t)time(NULL) };

    /* Arena capacities */
    const size_t global_arena_cap = 256 * 1024 * 1024;   /* 256 MB for weights + traj */
    const size_t rollout_arena_cap = 32 * 1024 * 1024;   /* 32 MB for rollout temp */
    const size_t step_arena_cap = 32 * 1024 * 1024;      /* 32 MB per-step temp (reset each step) */

    /* Global arena for persistent allocations (weights, trajectory buffers) */
    BearArena global_arena;
    int arena_rc = bear_arena_create(&global_arena, global_arena_cap);
    if (arena_rc != 0) {
        fprintf(stderr, "Failed to create global arena\n");
        return 1;
    }

    /* Create N-pole cartpole environment */
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

    /* Create policy network (small MLP for numerical gradient feasibility) */
    printf("Creating MLP policy network...\n");
    fflush(stdout);
    BearPolicyNet policy;
    int phid[] = { 16, 16 };
    int policy_rc = bear_policy_create_mlp(&policy, &global_arena,
                                   env->spec.obs_dim, env->spec.act_dim,
                                   env->spec.act_discrete, phid, 2);
    if (policy_rc != 0) {
        fprintf(stderr, "Failed to create policy network (rc=%d)\n", policy_rc);
        return 1;
    }
    printf("Policy network: MLP 16x16\n");
    bear_orthogonal_init_params(&policy, 1.0f);

    /* Create value network (small MLP) */
    printf("Creating value network...\n");
    fflush(stdout);
    BearValueNet critic;
    int vhid[] = { 16, 16 };
    int critic_rc = bear_value_create(&critic, &global_arena,
                           env->spec.obs_dim, vhid, 2);
    if (critic_rc != 0) {
        fprintf(stderr, "Failed to create value network (rc=%d)\n", critic_rc);
        return 1;
    }
    bear_value_orthogonal_init(&critic, 1.0f);

    /* PPO Configuration (Yacine/ CleanRL aligned) */
    BearPPOConfig cfg = bear_ppo_default_config();
    cfg.lr = 3e-4f;
    cfg.epochs_per_iter = 4;
    cfg.minibatch_size = 2048;
    cfg.gamma = 0.99f;
    cfg.gae_lambda = 0.95f;
    cfg.clip_coef = 0.2f;
    cfg.vf_coef = 0.5f;
    cfg.ent_coef = 0.01f;  /* continuous action */
    cfg.target_kl = 0.02f;
    cfg.lr_anneal = 1;

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

    /* Set logger */
    bear_trainer_set_logger(&trainer, train_logger, NULL);

    /* Training loop */
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

        /* Track best */
        if (avg_return > best_return) {
            best_return = avg_return;
            stagnation = 0;
        } else {
            stagnation++;
        }

        /* Episode length curriculum (Yacine's trick) */
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

        /* Early stop if solved */
        if (avg_return >= 9500.0f) {
            printf("\n=== SOLVED! Average return %.2f >= 9500 ===\n", avg_return);
            break;
        }

        /* Stagnation detection */
        if (stagnation > 500) {
            printf("\n[Stagnation] No improvement for 500 iters, stopping\n");
            break;
        }
    }

    printf("\nTraining complete!\n");
    printf("Best return: %.2f\n", best_return);
    printf("Total steps: %d\n", trainer.total_steps);

    /* Cleanup */
    bear_env_close(env);
    bear_arena_destroy(&trainer.global_arena);
    bear_arena_destroy(&trainer.rollout_arena);
    bear_arena_destroy(&trainer.step_arena);
    bear_arena_destroy(&global_arena);

    return 0;
}
