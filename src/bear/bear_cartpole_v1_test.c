/*
 * bear_cartpole_v1_test.c  --  Standard CartPole-v1 Solver & Evaluation
 *
 * Uses the proper BEAR_ENV_CARTPOLE implementation which matches Gymnasium CartPole-v1:
 * - 4D observation: [x, x_dot, theta, theta_dot]
 * - Discrete actions: {0, 1} (left/right force)
 * - Force magnitude 10.0, dt=0.02
 * - Angle threshold 12° (0.20944 rad), x threshold 2.4
 * - Episode length: 500 steps max
 * - Reward: 1.0 per step
 * - Solved: avg return >= 475 over 100 episodes
 */

#define _POSIX_C_SOURCE 200809L
#include "bear_arena.h"
#include "bear_env.h"
#include "bear_nn.h"
#include "bear_ppo.h"
#include "bear_opt.h"
#include "bear_simd.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <string.h>

#define SOLVED_THRESHOLD 475.0f
#define EVAL_EPISODES 100
#define MAX_STEPS 500
#define ROLLOUT_LEN 512  /* Faster iterations */

/* Simple logger */
static void train_logger(int iter, float total_steps, float return_mean,
                         float policy_loss, float value_loss, float entropy,
                         float lr, void* user_data) {
    (void)user_data;
    printf("Iter %4d | Steps %10.0f | Return %7.2f | PLoss %7.4f | VLoss %7.4f | Ent %7.4f | LR %.2e\n",
           iter, total_steps, return_mean, policy_loss, value_loss, entropy, lr);
    fflush(stdout);
}

/* Deterministic evaluation on standard CartPole-v1 (discrete actions) */
static float evaluate_cartpole_v1(BearEnv* env, BearPolicyNet* policy, int num_episodes) {
    BearArena eval_arena;
    bear_arena_create(&eval_arena, 4 * 1024 * 1024);

    float total_return = 0.0f;
    int completed = 0;

    for (int ep = 0; ep < num_episodes; ++ep) {
        bear_env_reset_all(env, &eval_arena);
        float episode_return = 0.0f;
        int done = 0;

        BearArena step_arena;
        bear_arena_create(&step_arena, 1024 * 1024);

        for (int step = 0; step < MAX_STEPS && !done; ++step) {
            bear_arena_reset(&step_arena);

            /* Deterministic policy forward */
            BearTensor act, logp, val;
            int64_t act_shape[2] = { env->spec.num_envs, env->spec.act_dim };
            int64_t scalar_shape[1] = { env->spec.num_envs };
            bear_tensor_create(&step_arena, &act, act_shape, 2, BEAR_DTYPE_F32, "eval.act");
            bear_tensor_create(&step_arena, &logp, scalar_shape, 1, BEAR_DTYPE_F32, "eval.logp");
            bear_tensor_create(&step_arena, &val, scalar_shape, 1, BEAR_DTYPE_F32, "eval.val");

            bear_policy_forward(policy, &env->obs, NULL, &act, &logp, &val, NULL, &step_arena);
            bear_policy_deterministic(policy, &act);

            /* Action is already discrete {0,1} for BEAR_ENV_CARTPOLE */
            /* No conversion needed - policy outputs logits for 2 actions, deterministic picks argmax */

            /* Step environment */
            BearTensor rew, done_t, next_obs;
            bear_tensor_create(&step_arena, &rew, scalar_shape, 1, BEAR_DTYPE_F32, "eval.rew");
            bear_tensor_create(&step_arena, &done_t, scalar_shape, 1, BEAR_DTYPE_U8, "eval.done");
            int64_t obs_shape[2] = { env->spec.num_envs, env->spec.obs_dim };
            bear_tensor_create(&step_arena, &next_obs, obs_shape, 2, BEAR_DTYPE_F32, "eval.next_obs");

            env->step(env, &env->actions, &rew, &done_t, &next_obs, &step_arena);

            float reward = ((float*)rew.data)[0];
            uint8_t done_flag = ((uint8_t*)done_t.data)[0];

            episode_return += reward;
            done = done_flag;

            memcpy(env->obs.data, next_obs.data, env->spec.num_envs * env->spec.obs_dim * sizeof(float));
        }

        total_return += episode_return;
        completed++;
        bear_arena_destroy(&step_arena);
    }

    bear_arena_destroy(&eval_arena);
    return total_return / completed;
}

/* Training run with standard CartPole-v1 (discrete) */
static float train_cartpole_v1(int num_envs, int total_iters, float lr) {
    uint64_t rng_state[2] = { 0xDEADBEEFDEADBEEFull ^ (uint64_t)time(NULL),
                               0xCAFEBABECAFEBABEull ^ (uint64_t)time(NULL) };

    size_t global_cap = 128 * 1024 * 1024;
    size_t rollout_cap = 16 * 1024 * 1024;
    size_t step_cap = 256 * 1024 * 1024;

    BearArena global_arena, rollout_arena, step_arena;
    if (bear_arena_create(&global_arena, global_cap) != 0) return -1;
    if (bear_arena_create(&rollout_arena, rollout_cap) != 0) return -1;
    if (bear_arena_create(&step_arena, step_cap) != 0) return -1;

    /* Create standard CartPole-v1 environment (discrete actions) */
    printf("Creating standard CartPole-v1 (discrete actions)...\n");
    BearEnv* env = bear_env_create(BEAR_ENV_CARTPOLE, num_envs, 1, 4, 1, 1, &global_arena);
    if (!env) {
        fprintf(stderr, "Failed to create environment\n");
        return -1;
    }

    env->spec.max_episode_steps = MAX_STEPS;

    printf("Environment: obs_dim=%d, act_dim=%d (discrete), max_steps=%d\n",
           env->spec.obs_dim, env->spec.act_dim, MAX_STEPS);

    /* Create MLP Policy (discrete categorical, 2-layer 128 hidden) */
    BearPolicyNet policy;
    int phid[] = { 128, 128 };
    int rc = bear_policy_create_mlp(&policy, &global_arena,
                                    env->spec.obs_dim, env->spec.act_dim,
                                    env->spec.act_discrete, phid, 2);
    if (rc != 0) {
        fprintf(stderr, "Policy creation failed: %d\n", rc);
        return -1;
    }
    bear_orthogonal_init_params(&policy, 1.0f);

    /* Create Value Network */
    BearValueNet critic;
    int vhid[] = { 128, 128 };
    rc = bear_value_create(&critic, &global_arena, env->spec.obs_dim, vhid, 2);
    if (rc != 0) {
        fprintf(stderr, "Value creation failed: %d\n", rc);
        return -1;
    }
    bear_value_orthogonal_init(&critic, 1.0f);

    /* PPO Config (SB3 CartPole defaults) */
    BearPPOConfig cfg = bear_ppo_default_config();
    cfg.lr = lr;
    cfg.epochs_per_iter = 10;
    cfg.minibatch_size = 64;
    cfg.gamma = 0.99f;
    cfg.gae_lambda = 0.95f;
    cfg.clip_coef = 0.2f;
    cfg.clip_coef_vf = 0.2f;
    cfg.vf_coef = 0.5f;
    cfg.ent_coef = 0.0f;      /* SB3 default for CartPole */
    cfg.target_kl = 0.02f;
    cfg.lr_anneal = 1;
    cfg.normalize_adv = 0;  /* Disable per-minibatch adv norm - can zero out signal */
    cfg.normalize_obs = 1;
    cfg.max_grad_norm = 0.5f;
    cfg.normalize_rewards = 0;  /* CartPole: constant reward=1/step, normalization kills signal */

    BearOptimizer* opt_policy = bear_optimizer_create(&global_arena, BEAR_OPT_ADAM, lr);
    BearOptimizer* opt_critic = bear_optimizer_create(&global_arena, BEAR_OPT_ADAM, lr);

    for (int i = 0; i < policy.num_layers; ++i) {
        BearParam* p = policy.layers[i].param;
        if (p && p->weight.data) bear_optimizer_register(opt_policy, p);
    }
    for (int i = 0; i < critic.num_layers; ++i) {
        BearParam* p = critic.layers[i].param;
        if (p && p->weight.data) bear_optimizer_register(opt_critic, p);
    }

    /* Trainer */
    BearTrainer trainer;
    rc = bear_trainer_init(&trainer, &policy, &critic, env, &cfg,
                           global_cap, rollout_cap, step_cap);
    if (rc != 0) {
        fprintf(stderr, "Trainer init failed: %d\n", rc);
        return -1;
    }
    trainer.global_arena = global_arena;
    trainer.rollout_arena = rollout_arena;
    trainer.step_arena = step_arena;
    trainer.opt_policy = opt_policy;
    trainer.opt_critic = opt_critic;

    /* Override rollout length in trajectory */
    trainer.traj.rollout_len = ROLLOUT_LEN;

    bear_trainer_set_logger(&trainer, train_logger, NULL);

    printf("\nTraining (%d iters, %d envs, rollout=%d)...\n\n", total_iters, num_envs, ROLLOUT_LEN);
    printf("Target: %d steps (CartPole-v1 max), Solved threshold: %.0f avg over %d eps\n\n",
           MAX_STEPS, SOLVED_THRESHOLD, EVAL_EPISODES);

    float best_return = -INFINITY;

    for (int iter = 0; iter < total_iters; ++iter) {
        float avg_return = bear_trainer_iter(&trainer, rng_state);

        /* LR anneal */
        if (cfg.lr_anneal) {
            float frac = 1.0f - (float)trainer.total_steps / (float)(total_iters * num_envs * ROLLOUT_LEN);
            float new_lr = lr * fmaxf(frac, 0.1f);
            bear_optimizer_set_lr(opt_policy, new_lr);
            bear_optimizer_set_lr(opt_critic, new_lr);
        }

        if (avg_return > best_return) best_return = avg_return;

        /* Periodic evaluation every 10 iterations */
        if (iter > 0 && iter % 10 == 0) {
            float eval_avg = evaluate_cartpole_v1(env, &policy, 20);
            printf("\n[Eval iter %d] Train avg: %.1f | Eval (20 ep): %.1f | Best: %.1f | Target: %.0f\n\n",
                   iter, avg_return, eval_avg, best_return, SOLVED_THRESHOLD);

            if (eval_avg >= SOLVED_THRESHOLD) {
                printf("=== SOLVED at iter %d! Eval avg %.1f >= %.0f ===\n", iter, eval_avg, SOLVED_THRESHOLD);
                break;
            }
        }

        if (iter % 5 == 0) {
            printf("Iter %4d | Steps %8d | Return %6.1f | Best %6.1f\n",
                   iter, trainer.total_steps, avg_return, best_return);
        }
    }

    /* Final thorough evaluation */
    printf("\n=== FINAL EVALUATION (%d episodes) ===\n", EVAL_EPISODES);
    float final_eval = evaluate_cartpole_v1(env, &policy, EVAL_EPISODES);
    printf("Final eval average: %.2f (threshold: %.0f)\n", final_eval, SOLVED_THRESHOLD);
    printf("Status: %s\n", final_eval >= SOLVED_THRESHOLD ? "SOLVED ✓" : "NOT SOLVED ✗");

    /* Save if solved */
    if (final_eval >= SOLVED_THRESHOLD) {
        /* bear_policy_save(&policy, "models/cartpole_v1_solved.bear");
        bear_value_save(&critic, "models/cartpole_v1_value.bear"); */
        printf("Model save stub - would save to models/cartpole_v1_solved.bear\n");
    }

    /* Cleanup */
    bear_arena_destroy(&global_arena);
    bear_arena_destroy(&rollout_arena);
    bear_arena_destroy(&step_arena);

    return final_eval;
}

int main(int argc, char** argv) {
    int num_envs = 64;
    int total_iters = 100;
    float lr = 3e-4f;
    int seed = (int)time(NULL);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--envs") == 0 && i + 1 < argc) num_envs = atoi(argv[++i]);
        else if (strcmp(argv[i], "--iters") == 0 && i + 1 < argc) total_iters = atoi(argv[++i]);
        else if (strcmp(argv[i], "--lr") == 0 && i + 1 < argc) lr = atof(argv[++i]);
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--envs N] [--iters N] [--lr F] [--seed N]\n", argv[0]);
            return 0;
        }
    }

    srand(seed);
    printf("+==================================================================+\n");
    printf("|  WUBU CARTPOLE-v1 STANDARD SOLVER  --  Pure C11 Bear RL            |\n");
    printf("+==================================================================+\n");
    printf("|  Envs: %d | Iters: %d | LR: %.1e | Seed: %d                    \n", num_envs, total_iters, lr, seed);
    printf("|  Target: %d steps, Solved: avg >= %.0f over %d eps               \n", MAX_STEPS, SOLVED_THRESHOLD, EVAL_EPISODES);
    printf("+==================================================================+\n\n");

    train_cartpole_v1(num_envs, total_iters, lr);

    return 0;
}