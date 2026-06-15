/*
 * bear_eval.c  --  BearRL Evaluation Protocol
 * 
 * Proper evaluation: separate eval env, deterministic policy, multi-seed statistics.
 * Matches SB3/Gym evaluation standards.
 */

#include "bear_arena.h"
#include "bear_env.h"
#include "bear_nn.h"
#include "bear_simd.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define EVAL_EPISODES 100
#define MAX_SEEDS 5

static void eval_logger(int episode, float episode_return, float episode_length, void* user_data) {
    (void)user_data;
    printf("  Episode %3d: Return = %8.2f, Length = %4.0f\n", episode, episode_return, episode_length);
}

float run_evaluation(BearEnv* env, BearPolicyNet* policy, int num_episodes, int seed) {
    /* RNG state */
    uint64_t rng_state[2] = { 0xDEADBEEFDEADBEEFull ^ (uint64_t)seed, 0xCAFEBABECAFEBABEull ^ (uint64_t)seed };
    
    /* Arena for evaluation (smaller, no training buffers) */
    const size_t eval_arena_cap = 8 * 1024 * 1024;
    BearArena eval_arena;
    bear_arena_create(&eval_arena, eval_arena_cap);
    
    /* Reset environment with specific seed */
    srand(seed);
    bear_env_reset_all(env, &eval_arena);
    
    float total_return = 0.0f;
    float total_length = 0.0f;
    int completed = 0;
    
    BearTensor obs_single;
    int64_t obs_shape[2] = { 1, env->spec.obs_dim };
    bear_tensor_create(&eval_arena, &obs_single, obs_shape, 2, BEAR_DTYPE_F32, "eval.obs");
    
    BearTensor act_t, logp_t, val_t;
    int64_t act_shape[2] = { 1, env->spec.act_dim };
    bear_tensor_create(&eval_arena, &act_t, act_shape, 2, BEAR_DTYPE_F32, "eval.act");
    int64_t scalar_shape[1] = { 1 };
    bear_tensor_create(&eval_arena, &logp_t, scalar_shape, 1, BEAR_DTYPE_F32, "eval.logp");
    bear_tensor_create(&eval_arena, &val_t, scalar_shape, 1, BEAR_DTYPE_F32, "eval.val");
    
    BearArena step_arena;
    bear_arena_create(&step_arena, 1024 * 1024);
    
    for (int ep = 0; ep < num_episodes; ++ep) {
        bear_env_reset_all(env, &eval_arena);
        bear_arena_reset(&step_arena);
        
        float episode_return = 0.0f;
        int episode_length = 0;
        int done = 0;
        
        while (!done && episode_length < env->spec.max_episode_steps) {
            /* Get observation */
            float* obs_data = (float*)env->obs.data;
            memcpy(obs_single.data, obs_data, env->spec.obs_dim * sizeof(float));
            
            /* Deterministic policy inference */
            bear_policy_forward(policy, &obs_single, NULL, &act_t, &logp_t, &val_t, NULL, &step_arena);
            bear_policy_deterministic(policy, &act_t);
            
            /* Clip action */
            float* act_data = (float*)act_t.data;
            float action = act_data[0];
            if (action > 10.0f) action = 10.0f;
            if (action < -10.0f) action = -10.0f;
            
            /* Step environment */
            float* env_act = (float*)env->actions.data;
            env_act[0] = action;
            
            bear_arena_reset(&step_arena);
            bear_env_step(env, &env->actions, &env->rewards, &env->dones, &env->obs, &step_arena);
            
            float reward = ((float*)env->rewards.data)[0];
            uint8_t done_flag = ((uint8_t*)env->dones.data)[0];
            
            episode_return += reward;
            episode_length++;
            done = done_flag;
        }
        
        total_return += episode_return;
        total_length += episode_length;
        completed++;
        
        eval_logger(ep, episode_return, (float)episode_length, NULL);
    }
    
    float mean_return = total_return / completed;
    float mean_length = total_length / completed;
    
    bear_arena_destroy(&step_arena);
    bear_arena_destroy(&eval_arena);
    
    return mean_return;
}

int main(int argc, char** argv) {
    int num_poles = 7;
    int num_envs = 16;  /* Smaller for eval */
    int num_episodes = EVAL_EPISODES;
    int num_seeds = MAX_SEEDS;
    const char* policy_path = "policy.bear";
    const char* value_path = "value.bear";
    
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--poles") == 0 && i + 1 < argc) num_poles = atoi(argv[++i]);
        else if (strcmp(argv[i], "--envs") == 0 && i + 1 < argc) num_envs = atoi(argv[++i]);
        else if (strcmp(argv[i], "--episodes") == 0 && i + 1 < argc) num_episodes = atoi(argv[++i]);
        else if (strcmp(argv[i], "--seeds") == 0 && i + 1 < argc) num_seeds = atoi(argv[++i]);
        else if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc) policy_path = argv[++i];
        else if (strcmp(argv[i], "--value") == 0 && i + 1 < argc) value_path = argv[++i];
        else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: bear_eval [--poles N] [--envs N] [--episodes N] [--seeds N] [--policy path] [--value path]\n");
            return 0;
        }
    }
    
    if (num_seeds > MAX_SEEDS) num_seeds = MAX_SEEDS;
    if (num_seeds < 1) num_seeds = 1;
    
    printf("===== BearRL N-Pole Cartpole Evaluation =====\n");
    printf("Poles: %d | Envs: %d | Episodes/seed: %d | Seeds: %d\n", num_poles, num_envs, num_episodes, num_seeds);
    printf("Policy: %s | Value: %s\n", policy_path, value_path);
    fflush(stdout);
    
    float seed_returns[MAX_SEEDS];
    float seed_lengths[MAX_SEEDS];
    
    for (int s = 0; s < num_seeds; ++s) {
        int seed = 42 + s * 100;  // Fixed seeds for reproducibility
        
        /* Create environment */
        const size_t arena_cap = 64 * 1024 * 1024;
        BearArena global_arena;
        bear_arena_create(&global_arena, arena_cap);
        
        BearEnv* env = bear_env_create_npole(num_poles, num_envs, &global_arena);
        if (!env) {
            fprintf(stderr, "Failed to create environment\n");
            return 1;
        }
        
        /* Create policy network */
        BearPolicyNet policy;
        int phid[] = { 128, 128 };
        bear_policy_create_mlp(&policy, &global_arena, env->spec.obs_dim, env->spec.act_dim, 0, phid, 2);
        
        /* Load trained weights */
        if (bear_policy_load(&policy, policy_path) != 0) {
            fprintf(stderr, "Failed to load policy from %s\n", policy_path);
            bear_env_close(env);
            bear_arena_destroy(&global_arena);
            return 1;
        }
        printf("Loaded policy from %s\n", policy_path);
        
        /* Create value network */
        BearValueNet critic;
        int vhid[] = { 16, 16 };
        bear_value_create(&critic, &global_arena, env->spec.obs_dim, vhid, 2);
        
        if (bear_value_load(&critic, value_path) != 0) {
            fprintf(stderr, "Failed to load value from %s\n", value_path);
            bear_env_close(env);
            bear_arena_destroy(&global_arena);
            return 1;
        }
        printf("Loaded value from %s\n", value_path);
        
        /* Run evaluation */
        printf("\n[Seed %d] Running evaluation...\n", seed);
        float mean_return = run_evaluation(env, &policy, num_episodes, seed);
        
        seed_returns[s] = mean_return;
        
        bear_env_close(env);
        bear_arena_destroy(&global_arena);
    }
    
    /* Compute statistics */
    float mean = 0.0f, var = 0.0f;
    for (int s = 0; s < num_seeds; ++s) mean += seed_returns[s];
    mean /= num_seeds;
    for (int s = 0; s < num_seeds; ++s) var += (seed_returns[s] - mean) * (seed_returns[s] - mean);
    float std = sqrtf(var / num_seeds);
    
    printf("\n===== EVALUATION RESULTS =====\n");
    for (int s = 0; s < num_seeds; ++s) {
        printf("  Seed %d: %.2f\n", 42 + s * 100, seed_returns[s]);
    }
    printf("  Mean: %.2f\n", mean);
    printf("  Std:  %.2f\n", std);
    printf("  95%% CI: [%.2f, %.2f]\n", mean - 1.96f * std / sqrtf(num_seeds), mean + 1.96f * std / sqrtf(num_seeds));
    
    return 0;
}
