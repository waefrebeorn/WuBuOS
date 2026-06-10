/*
 * bear_env.h — PufferC/BearRL Vectorized Environment API
 *
 * Flat obs/actions, Gym VectorEnv compatible, PufferLib-style emulation.
 * Supports: CartPole, Atari (ALE), custom envs.
 */

#ifndef BEAR_ENV_H
#define BEAR_ENV_H

#include <stdint.h>
#include <stdbool.h>
#include "bear_arena.h"
#include "bear_simd.h"

/* ═══════════════════════════════════════════════════════════════════
 * Environment Spec / Registry
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    BEAR_ENV_CARTPOLE      = 0,
    BEAR_ENV_SQUARED       = 1,    /* PufferLib Squared - simple gridworld */
    BEAR_ENV_TARGET        = 2,    /* PufferLib Target - navigation */
    BEAR_ENV_ATARI         = 3,    /* ALE Atari games */
    BEAR_ENV_N_POLE_CART   = 4,    /* N-pole cartpole (7-10 poles) - Sovereign Bear challenge */
    BEAR_ENV_CUSTOM        = 99,   /* User-defined */
} BearEnvType;

typedef struct {
    int num_envs;
    int max_agents;     /* Fixed max agents per env (for flat buffers) */
    int obs_dim;
    int act_dim;        /* Flattened: discrete = 1, continuous = dim */
    int act_discrete;   /* 1 = discrete, 0 = continuous */
    int max_episode_steps;
    float obs_low[BEAR_MAX_OBS_DIM];
    float obs_high[BEAR_MAX_OBS_DIM];
    float act_low[BEAR_MAX_ACT_DIM];
    float act_high[BEAR_MAX_ACT_DIM];
} BearEnvSpec;

/* ═══════════════════════════════════════════════════════════════════
 * Environment Instance (Vectorized)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct BearEnv BearEnv;

/* Function pointers for env implementation */
typedef void (*bear_env_reset_fn)(BearEnv* e, BearArena* arena);
typedef void (*bear_env_step_fn)(BearEnv* e, const BearTensor* actions,
                                  BearTensor* rewards, BearTensor* dones,
                                  BearTensor* next_obs, BearArena* arena);
typedef void (*bear_env_close_fn)(BearEnv* e);

struct BearEnv {
    BearEnvSpec spec;
    BearEnvType type;
    
    /* Pre-allocated batch buffers (from global arena) */
    BearTensor obs;           /* [num_envs * max_agents, obs_dim] */
    BearTensor rewards;       /* [num_envs] */
    BearTensor dones;         /* [num_envs] (uint8) */
    BearTensor actions;       /* [num_envs * max_agents, act_dim] */
    BearTensor logprobs;      /* [num_envs * max_agents] */
    BearTensor values;        /* [num_envs * max_agents] */
    BearTensor advantages;    /* [num_envs * max_agents] */
    BearTensor returns;       /* [num_envs * max_agents] */
    
    /* Episode tracking */
    int* episode_step;        /* [num_envs] */
    float* episode_return;      /* [num_envs] */
    float* episode_return_snapshot; /* [num_envs] — captured at episode end before reset */
    int  num_active_envs;     /* envs not done */
    
    /* Impl callbacks */
    bear_env_reset_fn reset;
    bear_env_step_fn step;
    bear_env_close_fn close;
    
    /* User data (for custom envs) */
    void* user_data;
};

/* ═══════════════════════════════════════════════════════════════════
 * Factory / API
 * ═══════════════════════════════════════════════════════════════════ */

/* Create vectorized environment */
BearEnv* bear_env_create(BearEnvType type, int num_envs,
                          int max_agents, int obs_dim, int act_dim,
                          int act_discrete, BearArena* global_arena);

/* Reset all environments (start episodes) */
void bear_env_reset_all(BearEnv* e, BearArena* arena);

/* Step all environments with batched actions */
void bear_env_step(BearEnv* e, const BearTensor* actions,
                    BearTensor* rewards, BearTensor* dones,
                    BearTensor* next_obs, BearArena* arena);

/* Close environment (cleanup) */
void bear_env_close(BearEnv* e);

/* Get environment specification */
const BearEnvSpec* bear_env_spec(const BearEnv* e);

/* ═══════════════════════════════════════════════════════════════════
 * Built-in Environments (implemented in bear_envs.c)
 * ═══════════════════════════════════════════════════════════════════ */

/* CartPole-v1 (Gym classic) */
void bear_cartpole_init(BearEnv* e, BearArena* global);
void bear_cartpole_reset(BearEnv* e, BearArena* arena);
void bear_cartpole_step(BearEnv* e, const BearTensor* actions,
                         BearTensor* rewards, BearTensor* dones,
                         BearTensor* next_obs, BearArena* arena);

/* Squared (PufferLib - gridworld with walls) */
void bear_squared_init(BearEnv* e, BearArena* global);
void bear_squared_reset(BearEnv* e, BearArena* arena);
void bear_squared_step(BearEnv* e, const BearTensor* actions,
                        BearTensor* rewards, BearTensor* dones,
                        BearTensor* next_obs, BearArena* arena);

/* Atari via ALE (C API) */
#ifdef BEAR_HAS_ALE
void bear_atari_init(BearEnv* e, const char* rom_path, BearArena* global);
void bear_atari_reset(BearEnv* e, BearArena* arena);
void bear_atari_step(BearEnv* e, const BearTensor* actions,
                      BearTensor* rewards, BearTensor* dones,
                      BearTensor* next_obs, BearArena* arena);
#endif

/* N-Pole Cartpole (7-10 poles) — Sovereign Bear Challenge */
void bear_npolecart_init(BearEnv* e, int num_poles, BearArena* global);
void bear_npolecart_reset(BearEnv* e, BearArena* arena);
void bear_npolecart_step(BearEnv* e, const BearTensor* actions,
                         BearTensor* rewards, BearTensor* dones,
                         BearTensor* next_obs, BearArena* arena);

/* N-pole state structure for curriculum access (opaque, defined in .c) */
struct NPoleCartState;
typedef struct NPoleCartState NPoleCartState;

/* Get N-pole state for curriculum / inspection */
void* bear_env_get_npole_state(BearEnv* e);

/* N-pole curriculum accessors */
int bear_npole_get_episode_length_max(BearEnv* e);
void bear_npole_set_episode_length_max(BearEnv* e, int max_len);

/* Custom environment registration */
typedef void (*bear_custom_reset_fn)(BearEnv* e, BearArena* arena);
typedef void (*bear_custom_step_fn)(BearEnv* e, const BearTensor* actions,
                                     BearTensor* rewards, BearTensor* dones,
                                     BearTensor* next_obs, BearArena* arena);

int bear_env_register_custom(const char* name, int obs_dim, int act_dim,
                              bear_custom_reset_fn reset, bear_custom_step_fn step);

BearEnv* bear_env_create_npole(int num_poles, int num_envs, BearArena* global_arena);

#endif /* BEAR_ENV_H */