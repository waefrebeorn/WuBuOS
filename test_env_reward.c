/* Test env step reward computation */
#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_arena.h"
#include "src/bear/bear_env.h"
#include "src/bear/bear_nn.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main() {
    BearArena arena;
    bear_arena_create(&arena, 1024*1024);

    /* Create 1-pole env */
    BearEnv* env = bear_env_create_npole(1, 1, &arena);
    if (!env) { printf("env create failed\n"); return 1; }
    bear_env_reset_all(env, &arena);

    /* Run a few steps with random actions */
    for (int step = 0; step < 10; ++step) {
        BearTensor actions, rewards, dones, next_obs;
        int64_t act_shape[2] = {1, 1};
        int64_t scalar_shape[1] = {1};
        int64_t obs_shape[2] = {1, 6};  /* x, vx, sin, cos, omega (wait, obs dim is 2+4*N = 6 for 1 pole) */
        
        bear_tensor_create(&arena, &actions, act_shape, 2, BEAR_DTYPE_F32, "act");
        bear_tensor_create(&arena, &rewards, scalar_shape, 1, BEAR_DTYPE_F32, "rew");
        bear_tensor_create(&arena, &dones, scalar_shape, 1, BEAR_DTYPE_U8, "dn");
        bear_tensor_create(&arena, &next_obs, obs_shape, 2, BEAR_DTYPE_F32, "obs");
        
        /* Random action */
        ((float*)actions.data)[0] = ((float)rand() / RAND_MAX - 0.5f) * 160.0f;  /* [-80, 80] */
        
        bear_env_step(env, &actions, &rewards, &dones, &next_obs, &arena);
        
        float rew = ((float*)rewards.data)[0];
        uint8_t done = ((uint8_t*)dones.data)[0];
        float* obs = (float*)next_obs.data;
        
        printf("Step %d: action=%.2f, reward=%.4f, done=%d, obs=[%.3f, %.3f, %.3f, %.3f, %.3f]\n",
            step, ((float*)actions.data)[0], rew, done, obs[0], obs[1], obs[2], obs[3], obs[4]);
        
        if (done) {
            printf("Episode done, reset...\n");
        }
        
        bear_arena_reset(&arena);
    }
    
    bear_arena_destroy(&arena);
    return 0;
}