/*
 * bear_cartpole_v1_solve.c  --  Train on N-Pole (1-pole, continuous, shaped rewards)
 *                           Evaluate on true CartPole-v1 (discrete, sparse rewards)
 *
 * Strategy: N-Pole 1-pole trains well with continuous actions + shaped rewards.
 *           Evaluate by converting continuous force -> discrete action (sign).
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

#define SOLVED_THRESHOLD 475.0f
#define EVAL_EPISODES 100
#define MAX_STEPS 500
#define ROLLOUT_LEN 2048

/* Simple logger */
static void train_logger(int iter, float total_steps, float return_mean,
                         float policy_loss, float value_loss, float entropy,
                         float lr, void* user_data) {
    (void)user_data;
    printf("Iter %4d | Steps %10.0f | Return %7.2f | PLoss %7.4f | VLoss %7.4f | Ent %7.4f | LR %.2e\n",
           iter, total_steps, return_mean, policy_loss, value_loss, entropy, lr);
    fflush(stdout);
}

/* Deterministic evaluation on TRUE CartPole-v1 (discrete actions, sparse rewards) */
static float evaluate_true_cartpole_v1(BearPolicyNet* policy, int num_episodes, int seed) {
    size_t cap = 16 * 1024 * 1024;
    BearArena arena;
    bear_arena_create(&arena, cap);

    /* Create TRUE CartPole-v1 environment */
    BearEnv* env = bear_env_create(BEAR_ENV_CARTPOLE, 1, 1, 4, 2, 1, &arena);
    if (!env) {
        fprintf(stderr, "Failed to create true CartPole env\n");
        return -1;
    }
    env->spec.max_episode_steps = MAX_STEPS;

    srand(seed);
    bear_env_reset_all(env, &arena);

    float total_return = 0.0f;
    int completed = 0;

    BearArena step_arena;
    bear_arena_create(&step_arena, 1024 * 1024);

    for (int ep = 0; ep < num_episodes; ++ep) {
        bear_env_reset_all(env, &arena);
        bear_arena_reset(&step_arena);

        float episode_return = 0.0f;
        int done = 0;

        for (int step = 0; step < MAX_STEPS && !done; ++step) {
            bear_arena_reset(&step_arena);

            /* Deterministic policy forward - continuous force output */
            BearTensor act, logp, val;
            int64_t act_shape[2] = { 1, 1 };  /* N-pole uses act_dim=1 continuous */
            int64_t scalar_shape[1] = { 1 };
            bear_tensor_create(&step_arena, &act, act_shape, 2, BEAR_DTYPE_F32, "eval.act");
            bear_tensor_create(&step_arena, &logp, scalar_shape, 1, BEAR_DTYPE_F32, "eval.logp");
            bear_tensor_create(&step_arena, &val, scalar_shape, 1, BEAR_DTYPE_F32, "eval.val");

            bear_policy_forward(policy, &env->obs, NULL, &act, &logp, &val, NULL, &step_arena);
            bear_policy_deterministic(policy, &act);

            /* Convert continuous force to discrete action: force > 0 -> 1 (right), force <= 0 -> 0 (left) */
            float force = ((float*)act.data)[0];
            float discrete_act = (force > 0.0f) ? 1.0f : 0.0f;

            /* Step TRUE CartPole env */
            BearTensor rew, done_t, next_obs;
            bear_tensor_create(&step_arena, &rew, scalar_shape, 1, BEAR_DTYPE_F32, "eval.rew");
            bear_tensor_create(&step_arena, &done_t, scalar_shape, 1, BEAR_DTYPE_U8, "eval.done");
            int64_t obs_shape[2] = { 1, 4 };
            bear_tensor_create(&step_arena, &next_obs, obs_shape, 2, BEAR_DTYPE_F32, "eval.next_obs");

            /* Set action for true env (discrete) */
            float* true_act = (float*)env->actions.data;
            true_act[0] = discrete_act;

            env->step(env, &env->actions, &rew, &done_t, &next_obs, &step_arena);

            float reward = ((float*)rew.data)[0];
            uint8_t done_flag = ((uint8_t*)done_t.data)[0];

            episode_return += reward;
            done = done_flag;

            memcpy(env->obs.data, next_obs.data, 4 * sizeof(float));
        }

        total_return += episode_return;
        completed++;
        if (ep % 20 == 0) {
            printf("  Ep %3d: Return = %.1f\n", ep, episode_return);
        }
    }

    bear_arena_destroy(&step_arena);
    bear_arena_destroy(&arena);

    return total_return / completed;
}

/* Training using N-Pole 1-pole (continuous, shaped rewards) */
static float train_npole_1pole(int num_envs, int total_iters, float lr, int seed) {
    uint64_t rng_state[2] = { 0xDEADBEEFDEADBEEFull ^ (uint64_t)seed,
                               0xCAFEBABECAFEBABEull ^ (uint64_t)time(NULL) };
    int iter_seed = seed;

    size_t global_cap = 128 * 1024 * 1024;
    size_t rollout_cap = 64 * 1024 * 1024;
    size_t step_cap = 16 * 1024 * 1024;

    BearArena global_arena, rollout_arena, step_arena;
    if (bear_arena_create(&global_arena, global_cap) != 0) return -1;
    if (bear_arena_create(&rollout_arena, rollout_cap) != 0) return -1;
    if (bear_arena_create(&step_arena, step_cap) != 0) return -1;

    /* Create N-Pole 1-pole environment (continuous actions, shaped rewards) */
    printf("Creating N-Pole 1-pole (continuous, shaped rewards)...\n");
    BearEnv* env = bear_env_create_npole(1, num_envs, &global_arena);
    if (!env) {
        fprintf(stderr, "Failed to create N-Pole environment\n");
        return -1;
    }

    /* Configure for CartPole-v1 parity: fixed 500 steps */
    bear_npole_set_episode_length_max(env, 500);
    env->spec.max_episode_steps = 500;

    printf("Environment: obs_dim=%d, act_dim=%d (continuous), max_steps=%d\n",
           env->spec.obs_dim, env->spec.act_dim, 500);

    /* Create MLP Policy (continuous Gaussian, 2-layer 128 hidden) */
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
    policy.logstd = NULL;
    policy.logstd_fixed = 0.0f;  /* log(1.0) */

    /* Create Value Network */
    BearValueNet critic;
    int vhid[] = { 128, 128 };
    rc = bear_value_create(&critic, &global_arena, env->spec.obs_dim, vhid, 2);
    if (rc != 0) {
        fprintf(stderr, "Value creation failed: %d\n", rc);
        return -1;
    }
    bear_value_orthogonal_init(&critic, 1.0f);

    /* PPO Config (SB3 CartPole defaults, but with small entropy for exploration) */
    BearPPOConfig cfg = bear_ppo_default_config();
    cfg.lr = lr;
    cfg.epochs_per_iter = 10;
    cfg.minibatch_size = 64;
    cfg.gamma = 0.99f;
    cfg.gae_lambda = 0.95f;
    cfg.clip_coef = 0.2f;
    cfg.clip_coef_vf = 0.2f;
    cfg.vf_coef = 0.5f;
    cfg.ent_coef = 0.01f;      /* Small entropy for discrete-like exploration */
    cfg.target_kl = 0.02f;
    cfg.lr_anneal = 1;
    cfg.normalize_adv = 1;
    cfg.normalize_obs = 1;
    cfg.max_grad_norm = 0.5f;
    cfg.normalize_rewards = 1;

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
    trainer.traj.rollout_len = ROLLOUT_LEN;

    bear_trainer_set_logger(&trainer, train_logger, NULL);

    printf("\nTraining (%d iters, %d envs, rollout=%d)...\n\n", total_iters, num_envs, ROLLOUT_LEN);
    printf("Target: %d steps, Solved threshold: %.0f avg over %d eps (TRUE CartPole-v1)\n\n",
           MAX_STEPS, SOLVED_THRESHOLD, EVAL_EPISODES);

    float best_eval = -INFINITY;

    for (int iter = 0; iter < total_iters; ++iter) {
        float avg_return = bear_trainer_iter(&trainer, rng_state);

        /* LR anneal */
        if (cfg.lr_anneal) {
            float frac = 1.0f - (float)trainer.total_steps / (float)(total_iters * num_envs * ROLLOUT_LEN);
            float new_lr = lr * fmaxf(frac, 0.1f);
            bear_optimizer_set_lr(opt_policy, new_lr);
            bear_optimizer_set_lr(opt_critic, new_lr);
        }

        /* Periodic evaluation on TRUE CartPole-v1 every 10 iterations */
        if (iter > 0 && iter % 10 == 0) {
            float eval_avg = evaluate_true_cartpole_v1(&policy, 20, iter_seed + iter);  /* Quick eval 20 eps */
            printf("\n[Eval iter %d] N-pole train avg: %.1f | True CartPole-v1 eval (20 ep): %.1f | Best: %.1f | Target: %.0f\n\n",
                   iter, avg_return, eval_avg, best_eval, SOLVED_THRESHOLD);

            if (eval_avg > best_eval) best_eval = eval_avg;

            if (eval_avg >= SOLVED_THRESHOLD) {
                printf("=== SOLVED at iter %d! True CartPole-v1 eval avg %.1f >= %.0f ===\n", iter, eval_avg, SOLVED_THRESHOLD);
                break;
            }
        }

        if (iter % 5 == 0) {
            printf("Iter %4d | Steps %8d | N-pole Return %6.1f | Best Eval %.1f\n",
                   iter, trainer.total_steps, avg_return, best_eval);
        }
    }

    /* Final thorough evaluation on TRUE CartPole-v1 */
    printf("\n=== FINAL EVALUATION ON TRUE CARTPOLE-v1 (%d episodes) ===\n", EVAL_EPISODES);
    float final_eval = evaluate_true_cartpole_v1(&policy, EVAL_EPISODES, iter_seed + total_iters);
    printf("Final eval average: %.2f (threshold: %.0f)\n", final_eval, SOLVED_THRESHOLD);
    printf("Status: %s\n", final_eval >= SOLVED_THRESHOLD ? "SOLVED ✓" : "NOT SOLVED ✗");

    /* Save if solved */
    if (final_eval >= SOLVED_THRESHOLD) {
        bear_policy_save(&policy, "models/cartpole_v1_solved.bear");
        bear_value_save(&critic, "models/cartpole_v1_value.bear");
        printf("Model saved to models/cartpole_v1_solved.bear\n");
    }

    /* Cleanup */
    bear_arena_destroy(&global_arena);
    bear_arena_destroy(&rollout_arena);
    bear_arena_destroy(&step_arena);

    return final_eval;
}

int main(int argc, char** argv) {
    int num_envs = 64;
    int total_iters = 200;
    float lr = 1e-4f;  /* Lower LR for continuous control */
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
    printf("+===================================================================+\n");
    printf("|  WUBU CARTPOLE-v1 SOLVER  --  N-Pole Train / True Eval              |\n");
    printf("+==================================================================+\n");
    printf("|  Envs: %d | Iters: %d | LR: %.1e | Seed: %d                    \n", num_envs, total_iters, lr, seed);
    printf("|  Train: N-Pole 1-pole (continuous, shaped)                       |\n");
    printf("|  Eval:  True CartPole-v1 (discrete, sparse, 475/100 eps)         |\n");
    printf("+==================================================================+\n\n");

    train_npole_1pole(num_envs, total_iters, lr, seed);

    return 0;
}