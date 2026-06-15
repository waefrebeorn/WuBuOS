/*
 * Quick CartPole environment sanity check
 */

#define _POSIX_C_SOURCE 200809L
#include "bear_arena.h"
#include "bear_env.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main() {
    printf("Testing standard CartPole-v1 environment...\n");

    size_t cap = 64 * 1024 * 1024;
    BearArena arena;
    bear_arena_create(&arena, cap);

    BearEnv* env = bear_env_create(BEAR_ENV_CARTPOLE, 4, 1, 4, 1, 1, &arena);
    if (!env) {
        fprintf(stderr, "Failed to create env\n");
        return 1;
    }
    env->spec.max_episode_steps = 500;

    printf("Created env: obs_dim=%d, act_dim=%d, discrete=%d, max_steps=%d\n",
           env->spec.obs_dim, env->spec.act_dim, env->spec.act_discrete, env->spec.max_episode_steps);

    bear_env_reset_all(env, &arena);

    printf("Initial obs:\n");
    float* obs = (float*)env->obs.data;
    for (int i = 0; i < 4; ++i) {
        printf("  Env %d: x=%.4f, x_dot=%.4f, theta=%.4f, theta_dot=%.4f\n",
               i, obs[i*4], obs[i*4+1], obs[i*4+2], obs[i*4+3]);
    }

    /* Test a few steps with action=0 (left) */
    BearArena step_arena;
    bear_arena_create(&step_arena, 1024 * 1024);

    for (int step = 0; step < 10; ++step) {
        bear_arena_reset(&step_arena);

        /* All envs take action 0 */
        float* actions = (float*)env->actions.data;
        for (int i = 0; i < 4; ++i) actions[i] = 0.0f;

        /* Step */
        env->step(env, &env->actions, &env->rewards, &env->dones, &env->obs, &step_arena);

        float* rew = (float*)env->rewards.data;
        uint8_t* done = (uint8_t*)env->dones.data;
        obs = (float*)env->obs.data;

        printf("Step %d: rewards=[", step);
        for (int i = 0; i < 4; ++i) printf(" %.2f", rew[i]);
        printf(" ], dones=[");
        for (int i = 0; i < 4; ++i) printf(" %d", done[i]);
        printf(" ]\n");

        if (done[0]) {
            printf("Env 0 done at step %d!\n", step);
            break;
        }
    }

    bear_arena_destroy(&step_arena);
    bear_arena_destroy(&arena);

    printf("Test complete.\n");
    return 0;
}