
#include "bear_arena.h"
#include "bear_env.h"
#include <stdio.h>
#include <math.h>

#define BEAR_MAX_N_POLES 10
#define BEAR_MAX_ENVS 4096

struct NPoleCartState {
    int num_poles;
    int state_dim;
    float cart_mass;
    float pole_masses[BEAR_MAX_N_POLES];
    float pole_lengths[BEAR_MAX_N_POLES];
    float gravity;
    float force_mag;
    float dt;
    float angle_threshold;
    float cart_pos_threshold;
    int max_episode_steps;
    int episode_length_min;
    int episode_length_max;
    float cart_x[BEAR_MAX_ENVS];
    float cart_vx[BEAR_MAX_ENVS];
    float theta[BEAR_MAX_ENVS][BEAR_MAX_N_POLES];
    float omega[BEAR_MAX_ENVS][BEAR_MAX_N_POLES];
    int episode_length[BEAR_MAX_ENVS];
    int episode_step_counter[BEAR_MAX_ENVS];
    float total_mass;
    float pole_mass_length[BEAR_MAX_N_POLES];
};

extern struct NPoleCartState g_npole_state;

int main() {
    BearArena arena;
    bear_arena_create(&arena, 64 * 1024 * 1024);
    
    BearEnv* env = bear_env_create_npole(7, 4, &arena);
    if (!env) { printf("Failed\n"); return 1; }
    
    bear_env_reset_all(env, &arena);
    
    printf("Init: angle_thresh=%.4f, cart_thresh=%.2f, dt=%.4f\n",
           g_npole_state.angle_threshold, g_npole_state.cart_pos_threshold, g_npole_state.dt);
    printf("ep_len_min=%d, ep_len_max=%d\n",
           g_npole_state.episode_length_min, g_npole_state.episode_length_max);
    printf("ep_length[0..3]: %d %d %d %d\n",
           g_npole_state.episode_length[0], g_npole_state.episode_length[1],
           g_npole_state.episode_length[2], g_npole_state.episode_length[3]);
    
    // Step tensors
    BearTensor actions, rewards, dones, next_obs;
    int B = env->spec.num_envs;
    int64_t act_shape[2] = { B, env->spec.act_dim };
    int64_t scalar_shape[1] = { B };
    int64_t obs_shape[2] = { B, env->spec.obs_dim };
    bear_tensor_create(&arena, &actions, act_shape, 2, BEAR_DTYPE_F32, "act");
    bear_tensor_create(&arena, &rewards, scalar_shape, 1, BEAR_DTYPE_F32, "rew");
    bear_tensor_create(&arena, &dones, scalar_shape, 1, BEAR_DTYPE_U8, "done");
    bear_tensor_create(&arena, &next_obs, obs_shape, 2, BEAR_DTYPE_F32, "next_obs");
    float* act_data = (float*)actions.data;
    for (int i = 0; i < B; ++i) act_data[i] = 0.0f;
    
    for (int step = 0; step < 200; ++step) {
        bear_env_step(env, &actions, &rewards, &dones, &next_obs, &arena);
        
        uint8_t* done = (uint8_t*)dones.data;
        for (int i = 0; i < B; ++i) {
            if (done[i]) {
                printf("Step %d: Env %d done! len=%d, step_counter=%d, cart_x=%.4f, theta1=%.4f\n",
                       step, i, g_npole_state.episode_length[i],
                       g_npole_state.episode_step_counter[i],
                       g_npole_state.cart_x[i], g_npole_state.theta[i][0]);
            }
        }
        
        // Check if all done
        int all_done = 1;
        for (int i = 0; i < B; ++i) if (!done[i]) all_done = 0;
        if (all_done) break;
    }
    
    bear_env_close(env);
    bear_arena_destroy(&arena);
    return 0;
}
