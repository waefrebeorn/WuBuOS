/*
 * bear_env.c — PufferC/BearRL Vectorized Environment Implementation
 */

#include "bear_env.h"
#include "bear_arena.h"
#include "bear_simd.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════
 * Episode Step Counter Storage
 * ═══════════════════════════════════════════════════════════════════ */

static int* g_episode_step = NULL;
static float* g_episode_return = NULL;
static float* g_episode_return_snapshot = NULL;
static int g_max_envs = 0;

static void ensure_episode_arrays(int num_envs) {
    if (num_envs > g_max_envs) {
        g_max_envs = num_envs;
        g_episode_step = realloc(g_episode_step, num_envs * sizeof(int));
        g_episode_return = realloc(g_episode_return, num_envs * sizeof(float));
        g_episode_return_snapshot = realloc(g_episode_return_snapshot, num_envs * sizeof(float));
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Vectorized Environment Factory
 * ═══════════════════════════════════════════════════════════════════ */

BearEnv* bear_env_create(BearEnvType type, int num_envs,
                          int max_agents, int obs_dim, int act_dim,
                          int act_discrete, BearArena* global_arena) {
    if (num_envs <= 0 || num_envs > BEAR_MAX_ENVS) return NULL;
    if (max_agents <= 0 || max_agents > BEAR_MAX_AGENTS) return NULL;
    
    BearEnv* e = BEAR_ARENA_ALLOC(global_arena, BearEnv, 1);
    if (!e) return NULL;
    memset(e, 0, sizeof(BearEnv));
    
    ensure_episode_arrays(num_envs);
    
    e->spec.num_envs = num_envs;
    e->spec.max_agents = max_agents;
    e->spec.obs_dim = obs_dim;
    e->spec.act_dim = act_dim;
    e->spec.act_discrete = act_discrete;
    e->spec.max_episode_steps = 1000;  /* default */
    e->type = type;
    e->num_active_envs = num_envs;
    e->episode_step = g_episode_step;
    e->episode_return = g_episode_return;
    e->episode_return_snapshot = g_episode_return_snapshot;
    
    /* Initialize obs bounds to [-inf, inf] by default */
    for (int i = 0; i < BEAR_MAX_OBS_DIM; ++i) {
        e->spec.obs_low[i] = -INFINITY;
        e->spec.obs_high[i] = INFINITY;
    }
    for (int i = 0; i < BEAR_MAX_ACT_DIM; ++i) {
        e->spec.act_low[i] = -1.0f;
        e->spec.act_high[i] = 1.0f;
    }
    
    /* Allocate batch buffers */
    int total_agents = num_envs * max_agents;
    
    int64_t obs_shape[2] = { total_agents, obs_dim };
    bear_tensor_create(global_arena, &e->obs, obs_shape, 2, BEAR_DTYPE_F32, "env.obs");
    
    int64_t act_shape[2] = { total_agents, act_dim };
    bear_tensor_create(global_arena, &e->actions, act_shape, 2, BEAR_DTYPE_F32, "env.actions");
    
    int64_t scalar_shape[1] = { num_envs };
    bear_tensor_create(global_arena, &e->rewards, scalar_shape, 1, BEAR_DTYPE_F32, "env.rewards");
    bear_tensor_create(global_arena, &e->dones, scalar_shape, 1, BEAR_DTYPE_U8, "env.dones");
    bear_tensor_create(global_arena, &e->logprobs, scalar_shape, 1, BEAR_DTYPE_F32, "env.logprobs");
    bear_tensor_create(global_arena, &e->values, scalar_shape, 1, BEAR_DTYPE_F32, "env.values");
    bear_tensor_create(global_arena, &e->advantages, scalar_shape, 1, BEAR_DTYPE_F32, "env.advantages");
    bear_tensor_create(global_arena, &e->returns, scalar_shape, 1, BEAR_DTYPE_F32, "env.returns");
    
    /* Select implementation based on type */
    switch (type) {
        case BEAR_ENV_CARTPOLE:
            e->spec.max_episode_steps = 500;
            e->reset = bear_cartpole_reset;
            e->step = bear_cartpole_step;
            e->close = NULL;  /* no external deps */
            bear_cartpole_init(e, global_arena);
            break;
            
        case BEAR_ENV_SQUARED:
            e->spec.max_episode_steps = 100;
            e->reset = bear_squared_reset;
            e->step = bear_squared_step;
            e->close = NULL;
            bear_squared_init(e, global_arena);
            break;
            
#ifdef BEAR_HAS_ALE
        case BEAR_ENV_ATARI:
            /* rom_path must be set in user_data before calling */
            e->reset = bear_atari_reset;
            e->step = bear_atari_step;
            e->close = NULL;
            break;
#endif
            
        case BEAR_ENV_CUSTOM:
            /* User must set reset/step/user_data manually */
            break;
            
        default:
            return NULL;
    }
    
    return e;
}

void bear_env_reset_all(BearEnv* e, BearArena* arena) {
    if (!e || !e->reset) return;
    e->num_active_envs = e->spec.num_envs;
    memset(e->episode_step, 0, e->spec.num_envs * sizeof(int));
    memset(e->episode_return, 0, e->spec.num_envs * sizeof(float));
    e->reset(e, arena);
}

void bear_env_step(BearEnv* e, const BearTensor* actions,
                    BearTensor* rewards, BearTensor* dones,
                    BearTensor* next_obs, BearArena* arena) {
    if (!e || !e->step) return;
    
    e->step(e, actions, rewards, dones, next_obs, arena);
    
    /* Update active env count */
    uint8_t* done_data = (uint8_t*)dones->data;
    e->num_active_envs = 0;
    for (int i = 0; i < e->spec.num_envs; ++i) {
        if (!done_data[i]) e->num_active_envs++;
    }
}

void bear_env_close(BearEnv* e) {
    if (e && e->close) e->close(e);
}

const BearEnvSpec* bear_env_spec(const BearEnv* e) {
    return e ? &e->spec : NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * CartPole-v1 Implementation (SIMD-vectorized)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    /* State: [x, x_dot, theta, theta_dot] per env */
    float x[BEAR_MAX_ENVS];
    float x_dot[BEAR_MAX_ENVS];
    float theta[BEAR_MAX_ENVS];
    float theta_dot[BEAR_MAX_ENVS];
} CartPoleState;

static CartPoleState g_cartpole_state;

void bear_cartpole_init(BearEnv* e, BearArena* global) {
    (void)global;
    e->spec.obs_dim = 4;
    e->spec.act_dim = 1;
    e->spec.act_discrete = 1;
    e->spec.max_episode_steps = 500;
    e->spec.obs_low[0] = -4.8f;  e->spec.obs_high[0] = 4.8f;
    e->spec.obs_low[1] = -INFINITY; e->spec.obs_high[1] = INFINITY;
    e->spec.obs_low[2] = -0.418f; e->spec.obs_high[2] = 0.418f;
    e->spec.obs_low[3] = -INFINITY; e->spec.obs_high[3] = INFINITY;
    e->spec.act_low[0] = 0.0f; e->spec.act_high[0] = 1.0f;  /* 0=left, 1=right */
}

void bear_cartpole_reset(BearEnv* e, BearArena* arena) {
    (void)arena;
    int n = e->spec.num_envs;
    for (int i = 0; i < n; ++i) {
        g_cartpole_state.x[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        g_cartpole_state.x_dot[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        g_cartpole_state.theta[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        g_cartpole_state.theta_dot[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        e->episode_step[i] = 0;
        e->episode_return[i] = 0;
    }
    /* Fill obs buffer */
    float* obs = (float*)e->obs.data;
    for (int i = 0; i < n; ++i) {
        int idx = i * 4;
        obs[idx + 0] = g_cartpole_state.x[i];
        obs[idx + 1] = g_cartpole_state.x_dot[i];
        obs[idx + 2] = g_cartpole_state.theta[i];
        obs[idx + 3] = g_cartpole_state.theta_dot[i];
    }
    /* Zero rewards/dones */
    memset(e->rewards.data, 0, n * sizeof(float));
    memset(e->dones.data, 0, n * sizeof(uint8_t));
}

void bear_cartpole_step(BearEnv* e, const BearTensor* actions,
                         BearTensor* rewards, BearTensor* dones,
                         BearTensor* next_obs, BearArena* arena) {
    (void)arena;
    int n = e->spec.num_envs;
    float* act_data = (float*)actions->data;
    float* rew = (float*)rewards->data;
    uint8_t* done = (uint8_t*)dones->data;
    float* obs = (float*)next_obs->data;
    
    const float gravity = 9.8f;
    const float masscart = 1.0f;
    const float masspole = 0.1f;
    const float total_mass = masscart + masspole;
    const float length = 0.5f;
    const float polemass_length = masspole * length;
    const float force_mag = 10.0f;
    const float tau = 0.02f;
    const float theta_threshold = 0.209f;  /* ~12 degrees */
    const float x_threshold = 2.4f;
    
    for (int i = 0; i < n; ++i) {
        int action = (int)act_data[i];  /* 0 or 1 */
        float force = (action == 1 ? force_mag : -force_mag);
        
        float x = g_cartpole_state.x[i];
        float x_dot = g_cartpole_state.x_dot[i];
        float theta = g_cartpole_state.theta[i];
        float theta_dot = g_cartpole_state.theta_dot[i];
        
        float costheta = cosf(theta);
        float sintheta = sinf(theta);
        
        float temp = (force + polemass_length * theta_dot * theta_dot * sintheta) / total_mass;
        float thetaacc = (gravity * sintheta - costheta * temp) / 
                         (length * (4.0f/3.0f - masspole * costheta * costheta / total_mass));
        float xacc = temp - polemass_length * thetaacc * costheta / total_mass;
        
        x = x + tau * x_dot;
        x_dot = x_dot + tau * xacc;
        theta = theta + tau * theta_dot;
        theta_dot = theta_dot + tau * thetaacc;
        
        g_cartpole_state.x[i] = x;
        g_cartpole_state.x_dot[i] = x_dot;
        g_cartpole_state.theta[i] = theta;
        g_cartpole_state.theta_dot[i] = theta_dot;
        
        /* Reward: 1 per step */
        float r = 1.0f;
        rew[i] = r;
        if (isnan(r) || isinf(r)) {
        }
        e->episode_return[i] += r;
        
        /* Done if out of bounds or max steps */
        e->episode_step[i]++;
        int is_done = (fabsf(x) > x_threshold || fabsf(theta) > theta_threshold || 
                       e->episode_step[i] >= e->spec.max_episode_steps);
        done[i] = is_done ? 1 : 0;
        
        /* Fill next observation */
        int idx = i * 4;
        obs[idx + 0] = x;
        obs[idx + 1] = x_dot;
        obs[idx + 2] = theta;
        obs[idx + 3] = theta_dot;
    }
    
    e->num_active_envs = 0;
    for (int i = 0; i < n; ++i) if (!done[i]) e->num_active_envs++;
}

/* ═══════════════════════════════════════════════════════════════════
 * Squared (PufferLib-style gridworld)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    int grid_size;
    int max_steps;
    int agent_pos[BEAR_MAX_ENVS * 2];
    int goal_pos[BEAR_MAX_ENVS * 2];
    int wall_map[BEAR_MAX_ENVS * 64];  /* 8x8 grid */
} SquaredState;

static SquaredState g_squared_state;

void bear_squared_init(BearEnv* e, BearArena* global) {
    (void)global;
    e->spec.obs_dim = 64 + 4;  /* 8x8 grid + agent pos + goal pos (flattened) */
    e->spec.act_dim = 1;
    e->spec.act_discrete = 1;
    e->spec.max_episode_steps = 100;
    e->spec.obs_low[0] = 0.0f; e->spec.obs_high[0] = 1.0f;  /* binary grid + normalized coords */
    e->spec.act_low[0] = 0.0f; e->spec.act_high[0] = 3.0f;  /* 0=up, 1=right, 2=down, 3=left */
}

void bear_squared_reset(BearEnv* e, BearArena* arena) {
    (void)arena;
    int n = e->spec.num_envs;
    for (int i = 0; i < n; ++i) {
        g_squared_state.grid_size = 8;
        g_squared_state.max_steps = 100;
        /* Random agent pos */
        g_squared_state.agent_pos[i * 2 + 0] = rand() % 8;
        g_squared_state.agent_pos[i * 2 + 1] = rand() % 8;
        /* Random goal pos (different from agent) */
        do {
            g_squared_state.goal_pos[i * 2 + 0] = rand() % 8;
            g_squared_state.goal_pos[i * 2 + 1] = rand() % 8;
        } while (g_squared_state.goal_pos[i * 2 + 0] == g_squared_state.agent_pos[i * 2 + 0] &&
                 g_squared_state.goal_pos[i * 2 + 1] == g_squared_state.agent_pos[i * 2 + 1]);
        /* Empty walls (sparse) */
        for (int w = 0; w < 64; ++w) {
            g_squared_state.wall_map[i * 64 + w] = (rand() % 10 == 0) ? 1 : 0;
        }
        e->episode_step[i] = 0;
        e->episode_return[i] = 0;
    }
    /* Fill obs */
    float* obs = (float*)e->obs.data;
    for (int i = 0; i < n; ++i) {
        int base = i * 68;  /* 64 grid + 4 coords */
        memset(obs + base, 0, 64 * sizeof(float));
        for (int w = 0; w < 64; ++w) obs[base + w] = g_squared_state.wall_map[i * 64 + w];
        obs[base + 64] = g_squared_state.agent_pos[i * 2 + 0] / 7.0f;
        obs[base + 65] = g_squared_state.agent_pos[i * 2 + 1] / 7.0f;
        obs[base + 66] = g_squared_state.goal_pos[i * 2 + 0] / 7.0f;
        obs[base + 67] = g_squared_state.goal_pos[i * 2 + 1] / 7.0f;
    }
    memset(e->rewards.data, 0, n * sizeof(float));
    memset(e->dones.data, 0, n * sizeof(uint8_t));
}

void bear_squared_step(BearEnv* e, const BearTensor* actions,
                        BearTensor* rewards, BearTensor* dones,
                        BearTensor* next_obs, BearArena* arena) {
    (void)arena;
    int n = e->spec.num_envs;
    float* act = (float*)actions->data;
    float* rew = (float*)rewards->data;
    uint8_t* done = (uint8_t*)dones->data;
    float* obs = (float*)next_obs->data;
    
    for (int i = 0; i < n; ++i) {
        int act_idx = (int)act[i];  /* 0=U, 1=R, 2=D, 3=L */
        int x = g_squared_state.agent_pos[i * 2 + 0];
        int y = g_squared_state.agent_pos[i * 2 + 1];
        int nx = x, ny = y;
        
        switch (act_idx) {
            case 0: ny = y - 1; break;  /* Up */
            case 1: nx = x + 1; break;  /* Right */
            case 2: ny = y + 1; break;  /* Down */
            case 3: nx = x - 1; break;  /* Left */
        }
        
        /* Check bounds and walls */
        if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8 &&
            g_squared_state.wall_map[i * 64 + ny * 8 + nx] == 0) {
            x = nx; y = ny;
            g_squared_state.agent_pos[i * 2 + 0] = x;
            g_squared_state.agent_pos[i * 2 + 1] = y;
        }
        
        /* Reward: -1 per step, +10 at goal */
        float r = -1.0f;
        int gx = g_squared_state.goal_pos[i * 2 + 0];
        int gy = g_squared_state.goal_pos[i * 2 + 1];
        if (x == gx && y == gy) r = 10.0f;
        rew[i] = r;
        if (isnan(r) || isinf(r)) {
        }
        e->episode_return[i] += r;
        
        e->episode_step[i]++;
        int is_done = (x == gx && y == gy) || e->episode_step[i] >= e->spec.max_episode_steps;
        done[i] = is_done ? 1 : 0;
        
        /* Fill next obs */
        int base = i * 68;
        memset(obs + base, 0, 64 * sizeof(float));
        for (int w = 0; w < 64; ++w) obs[base + w] = g_squared_state.wall_map[i * 64 + w];
        obs[base + 64] = x / 7.0f;
        obs[base + 65] = y / 7.0f;
        obs[base + 66] = gx / 7.0f;
        obs[base + 67] = gy / 7.0f;
    }
    
    e->num_active_envs = 0;
    for (int i = 0; i < n; ++i) if (!done[i]) e->num_active_envs++;
}

/* ═══════════════════════════════════════════════════════════════════
 * Custom Environment Registration
 * ═══════════════════════════════════════════════════════════════════ */

static bear_custom_reset_fn g_custom_reset = NULL;
static bear_custom_step_fn g_custom_step = NULL;

int bear_env_register_custom(const char* name, int obs_dim, int act_dim,
                              bear_custom_reset_fn reset, bear_custom_step_fn step) {
    (void)name; (void)obs_dim; (void)act_dim;
    if (!reset || !step) return -1;
    g_custom_reset = reset;
    g_custom_step = step;
    return 0;
}

/* User must call this to wire up custom env */
void bear_env_use_custom(BearEnv* e, const char* name) {
    (void)name;
    e->type = BEAR_ENV_CUSTOM;
    e->reset = g_custom_reset;
    e->step = g_custom_step;
}

/* ═══════════════════════════════════════════════════════════════════
 * N-Pole Cartpole (7-10 Poles) — Sovereign Bear Challenge
 * ═══════════════════════════════════════════════════════════════════
 * Physics: Recursive Lagrangian dynamics for N linked inverted pendulums
 * on a cart. State: [cart_x, cart_vx, θ₁, ω₁, θ₂, ω₂, ..., θ_N, ω_N]
 * Action: Continuous force on cart [−force_mag, +force_mag]
 * Reward: Survival + per-pole upright bonus + cart centering penalty
 * References: SharpNEAT multi-pole, Wieland equations, standard chain pendulum Lagrangian
 */

#ifndef BEAR_MAX_N_POLES
#define BEAR_MAX_N_POLES 10
#endif

#define BEAR_NPOLE_MAX_STATE_DIM (2 + 2 * BEAR_MAX_N_POLES)  /* 22 max */

/* Full struct definition (matches forward declaration in header) */
struct NPoleCartState {
    int num_poles;                     /* N = 7..10 */
    int state_dim;                     /* 2 + 2*N */
    
    /* Physical parameters */
    float cart_mass;
    float pole_masses[BEAR_MAX_N_POLES];
    float pole_lengths[BEAR_MAX_N_POLES];
    float gravity;
    float force_mag;
    float dt;
    
    /* Termination thresholds */
    float angle_threshold;             /* ~π/2 * 1.1 */
    float cart_pos_threshold;
    int max_episode_steps;
    int episode_length_min;
    int episode_length_max;            /* For randomization */
    
    /* State buffers (SoA for SIMD) */
    float cart_x[BEAR_MAX_ENVS];
    float cart_vx[BEAR_MAX_ENVS];
    float theta[BEAR_MAX_ENVS][BEAR_MAX_N_POLES];
    float omega[BEAR_MAX_ENVS][BEAR_MAX_N_POLES];
    
    /* Episode tracking per env */
    int episode_length[BEAR_MAX_ENVS];
    int episode_step_counter[BEAR_MAX_ENVS];
    
    /* Pre-computed constants for speed */
    float total_mass;
    float pole_mass_length[BEAR_MAX_N_POLES];
};

struct NPoleCartState g_npole_state;

/* Forward declarations for internal physics functions */
static void npole_compute_accelerations(NPoleCartState* s, int env_id, 
                                         float force, float* theta_acc, float* cart_acc);
static void npole_rk4_step(NPoleCartState* s, int env_id, float force);
static void npole_reset_env(NPoleCartState* s, int env_id);

/* ═══════════════════════════════════════════════════════════════════
 * N-Pole Cartpole Initialization
 * ═══════════════════════════════════════════════════════════════════ */

void bear_npolecart_init(BearEnv* e, int num_poles, BearArena* global) {
    (void)global;
    NPoleCartState* s = &g_npole_state;

    /* Validate and clamp num_poles */
    if (num_poles < 1) num_poles = 1;
    if (num_poles > BEAR_MAX_N_POLES) num_poles = BEAR_MAX_N_POLES;
    s->num_poles = num_poles;
    s->state_dim = 2 + 2 * num_poles;
    
    /* Physical constants (standard cartpole parameters, scaled for multi-pole) */
    s->cart_mass = 1.0f;
    for (int i = 0; i < num_poles; ++i) {
        s->pole_masses[i] = 0.1f * (1.0f - i * 0.02f);  /* Slightly lighter toward tip */
        s->pole_lengths[i] = 0.5f * (1.0f - i * 0.05f); /* Slightly shorter toward tip */
    }
    s->gravity = 9.8f;
    s->force_mag = 10.0f;
    s->dt = 0.01f;  /* MuJoCo Warp style timestep */
    
    s->angle_threshold = 1.73f;  /* ~π/2 * 1.1 ≈ 1.73 rad (99 deg) */
    s->cart_pos_threshold = 4.0f;
    s->max_episode_steps = 10000;  /* Target: 10k steps */
    s->episode_length_min = 64;   /* Match rollout length for quick episodes */
    s->episode_length_max = 128;   /* Match rollout length so episodes terminate */
    
    /* Pre-compute */
    s->total_mass = s->cart_mass;
    for (int i = 0; i < num_poles; ++i) {
        s->total_mass += s->pole_masses[i];
        s->pole_mass_length[i] = s->pole_masses[i] * s->pole_lengths[i];
    }
    
    /* Set environment spec */
    e->spec.obs_dim = s->state_dim;
    e->spec.act_dim = 1;            /* Continuous force */
    e->spec.act_discrete = 0;       /* Continuous action */
    e->spec.max_episode_steps = s->max_episode_steps;
    
    /* Observation bounds (for normalization) */
    for (int i = 0; i < BEAR_MAX_OBS_DIM; ++i) {
        e->spec.obs_low[i] = -INFINITY;
        e->spec.obs_high[i] = INFINITY;
    }
    e->spec.obs_low[0] = -s->cart_pos_threshold;
    e->spec.obs_high[0] = s->cart_pos_threshold;
    e->spec.obs_low[1] = -INFINITY;  /* cart velocity unbounded */
    e->spec.obs_high[1] = INFINITY;
    for (int p = 0; p < num_poles; ++p) {
        int theta_idx = 2 + 2 * p;
        int omega_idx = 3 + 2 * p;
        e->spec.obs_low[theta_idx] = -3.14159f;  /* [-π, π] */
        e->spec.obs_high[theta_idx] = 3.14159f;
        e->spec.obs_low[omega_idx] = -INFINITY;
        e->spec.obs_high[omega_idx] = INFINITY;
    }
    e->spec.act_low[0] = -s->force_mag;
    e->spec.act_high[0] = s->force_mag;
    
    e->reset = bear_npolecart_reset;
    e->step = bear_npolecart_step;
    e->close = NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * N-Pole Cartpole Reset (Staggered)
 * ═══════════════════════════════════════════════════════════════════ */

static void npole_reset_env(NPoleCartState* s, int env_id) {
    s->cart_x[env_id] = ((float)rand() / RAND_MAX - 0.5f) * 0.05f;
    s->cart_vx[env_id] = ((float)rand() / RAND_MAX - 0.5f) * 0.05f;
    
    for (int p = 0; p < s->num_poles; ++p) {
        /* Small random initial angles near upright (0 = upright) */
        s->theta[env_id][p] = ((float)rand() / RAND_MAX - 0.5f) * 0.05f;
        s->omega[env_id][p] = ((float)rand() / RAND_MAX - 0.5f) * 0.05f;
    }
    
    /* Randomize episode length per episode (Yacine's anti-laziness trick) */
    s->episode_length[env_id] = s->episode_length_min + 
        rand() % (s->episode_length_max - s->episode_length_min + 1);
    s->episode_step_counter[env_id] = 0;
    /* Reset episode return accumulator */
    g_episode_return[env_id] = 0.0f;
}

void bear_npolecart_reset(BearEnv* e, BearArena* arena) {
    (void)arena;
    NPoleCartState* s = &g_npole_state;
    int n = e->spec.num_envs;
    
    for (int i = 0; i < n; ++i) {
        npole_reset_env(s, i);
    }
    
    /* Fill obs buffer */
    float* obs = (float*)e->obs.data;
    for (int i = 0; i < n; ++i) {
        int idx = i * s->state_dim;
        obs[idx + 0] = s->cart_x[i];
        obs[idx + 1] = s->cart_vx[i];
        for (int p = 0; p < s->num_poles; ++p) {
            obs[idx + 2 + 2 * p]     = s->theta[i][p];
            obs[idx + 2 + 2 * p + 1] = s->omega[i][p];
        }
    }
    
    /* Zero rewards/dones */
    memset(e->rewards.data, 0, n * sizeof(float));
    memset(e->dones.data, 0, n * sizeof(uint8_t));
}

/* ═══════════════════════════════════════════════════════════════════
 * Physics: Recursive Lagrangian Dynamics for N-Pole Cartpole
 * ═══════════════════════════════════════════════════════════════════
 *
 * Derivation: Generalized coordinates q = [x, θ₁, θ₂, ..., θ_N]
 * Lagrangian L = T - V
 * T = ½ m_cart ẋ² + Σ ½ m_i (ẋ² + (l_i θ̇_i)² + 2 ẋ l_i θ̇_i cos θ_i)
 *    + cross terms between poles...
 * V = Σ m_i g l_i (1 - cos θ_i)
 *
 * Euler-Lagrange gives M(q)q̈ + C(q,q̇)q̇ + G(q) = Bτ
 * where M is (N+1)×(N+1) symmetric positive definite.
 *
 * We solve for q̈ using the recursive O(N) algorithm (Featherstone-style)
 * or direct dense inversion for small N (N≤10).
 */

static void npole_compute_accelerations(NPoleCartState* s, int env_id, 
                                         float force, float* theta_acc, float* cart_acc) {
    int N = s->num_poles;
    
    /* Build mass matrix M and force vector F for [ẍ, θ̈₁, θ̈₂, ..., θ̈_N] */
    /* M is (N+1)×(N+1). We'll use a dense local array since N≤10. */
    float M[BEAR_MAX_N_POLES + 1][BEAR_MAX_N_POLES + 1];
    float F[BEAR_MAX_N_POLES + 1];
    
    /* Initialize */
    for (int i = 0; i <= N; ++i) {
        for (int j = 0; j <= N; ++j) M[i][j] = 0.0f;
        F[i] = 0.0f;
    }
    
    /* M[0][0] and F[0] — cart equation */
    M[0][0] = s->total_mass;
    for (int i = 0; i < N; ++i) {
        float cos_t = cosf(s->theta[env_id][i]);
        M[0][0] += s->pole_masses[i];
        M[0][i + 1] = s->pole_mass_length[i] * cos_t;
        M[i + 1][0] = M[0][i + 1];
    }
    
    /* M[i+1][j+1] and F[i+1] — pole equations */
    for (int i = 0; i < N; ++i) {
        float mi = s->pole_masses[i];
        float li = s->pole_lengths[i];
        float sin_ti = sinf(s->theta[env_id][i]);
        
        /* Diagonal (i,i) */
        M[i + 1][i + 1] = mi * li * li;
        
        /* Off-diagonal pole-pole coupling */
        for (int j = i + 1; j < N; ++j) {
            float lj = s->pole_lengths[j];
            float theta_diff = s->theta[env_id][i] - s->theta[env_id][j];
            float cos_diff = cosf(theta_diff);
            M[i + 1][j + 1] = mi * li * lj * cos_diff;
            M[j + 1][i + 1] = M[i + 1][j + 1];
        }
        
        /* Force terms (Coriolis + gravity) */
        float coriolis = 0.0f;
        for (int j = 0; j < N; ++j) {
            if (j == i) continue;
            float mj = s->pole_masses[j];
            float lj = s->pole_lengths[j];
            float omega_j = s->omega[env_id][j];
            float theta_diff = s->theta[env_id][i] - s->theta[env_id][j];
            float sin_diff = sinf(theta_diff);
            coriolis += mj * li * lj * omega_j * omega_j * sin_diff;
        }
        
        F[i + 1] = -mi * s->gravity * li * sin_ti + coriolis;
    }
    
    /* External force on cart */
    F[0] = force;
    
    /* Coriolis terms for cart equation */
    for (int i = 0; i < N; ++i) {
        float mi = s->pole_masses[i];
        float li = s->pole_lengths[i];
        float sin_ti = sinf(s->theta[env_id][i]);
        float omega_i = s->omega[env_id][i];
        F[0] += mi * li * omega_i * omega_i * sin_ti;
    }
    
    /* Solve M * acc = F for accelerations using Gaussian elimination */
    /* System size: (N+1) ≤ 11 — small enough for dense solve */
    int sz = N + 1;
    
    float acc[BEAR_MAX_N_POLES + 1] = {0};
    
    /* Forward elimination */
    for (int i = 0; i < sz; ++i) {
        /* Pivot */
        float pivot = M[i][i];
        if (fabsf(pivot) < 1e-8f) pivot = (pivot >= 0 ? 1e-8f : -1e-8f);
        
        for (int j = i; j < sz; ++j) M[i][j] /= pivot;
        F[i] /= pivot;
        
        for (int k = i + 1; k < sz; ++k) {
            float factor = M[k][i];
            if (fabsf(factor) < 1e-12f) continue;
            for (int j = i; j < sz; ++j) {
                M[k][j] -= factor * M[i][j];
            }
            F[k] -= factor * F[i];
        }
    }
    
    /* Back substitution */
    for (int i = sz - 1; i >= 0; --i) {
        float sum = F[i];
        for (int j = i + 1; j < sz; ++j) {
            sum -= M[i][j] * acc[j];
        }
        acc[i] = sum;
    }
    
    *cart_acc = acc[0];
    for (int i = 0; i < N; ++i) {
        theta_acc[i] = acc[i + 1];
    }
}

/* 4th-order Runge-Kutta integration for stability */
static void npole_rk4_step(NPoleCartState* s, int env_id, float force) {
    int N = s->num_poles;
    float dt = s->dt;
    
    /* State at start */
    float x0 = s->cart_x[env_id];
    float vx0 = s->cart_vx[env_id];
    float theta0[BEAR_MAX_N_POLES];
    float omega0[BEAR_MAX_N_POLES];
    for (int i = 0; i < N; ++i) {
        theta0[i] = s->theta[env_id][i];
        omega0[i] = s->omega[env_id][i];
    }
    
    /* k1 */
    float theta_acc1[BEAR_MAX_N_POLES], cart_acc1;
    npole_compute_accelerations(s, env_id, force, theta_acc1, &cart_acc1);
    
    /* Temporarily update state for k2 */
    s->cart_x[env_id] = x0 + 0.5f * dt * vx0;
    s->cart_vx[env_id] = vx0 + 0.5f * dt * cart_acc1;
    for (int i = 0; i < N; ++i) {
        s->theta[env_id][i] = theta0[i] + 0.5f * dt * omega0[i];
        s->omega[env_id][i] = omega0[i] + 0.5f * dt * theta_acc1[i];
    }
    
    /* k2 */
    float theta_acc2[BEAR_MAX_N_POLES], cart_acc2;
    npole_compute_accelerations(s, env_id, force, theta_acc2, &cart_acc2);
    
    /* k3 */
    s->cart_x[env_id] = x0 + 0.5f * dt * (vx0 + 0.5f * dt * cart_acc1);
    s->cart_vx[env_id] = vx0 + 0.5f * dt * cart_acc2;
    for (int i = 0; i < N; ++i) {
        s->theta[env_id][i] = theta0[i] + 0.5f * dt * (omega0[i] + 0.5f * dt * theta_acc1[i]);
        s->omega[env_id][i] = omega0[i] + 0.5f * dt * theta_acc2[i];
    }
    
    float theta_acc3[BEAR_MAX_N_POLES], cart_acc3;
    npole_compute_accelerations(s, env_id, force, theta_acc3, &cart_acc3);
    
    /* k4 */
    s->cart_x[env_id] = x0 + dt * (vx0 + 0.5f * dt * cart_acc2);
    s->cart_vx[env_id] = vx0 + dt * cart_acc3;
    for (int i = 0; i < N; ++i) {
        s->theta[env_id][i] = theta0[i] + dt * (omega0[i] + 0.5f * dt * theta_acc2[i]);
        s->omega[env_id][i] = omega0[i] + dt * theta_acc3[i];
    }
    
    float theta_acc4[BEAR_MAX_N_POLES], cart_acc4;
    npole_compute_accelerations(s, env_id, force, theta_acc4, &cart_acc4);
    
    /* Final update: weighted average of k1..k4 */
    s->cart_x[env_id] = x0 + dt/6.0f * (vx0 + 2*(vx0 + 0.5f*dt*cart_acc1) + 2*(vx0 + 0.5f*dt*cart_acc2) + (vx0 + dt*cart_acc3));
    s->cart_vx[env_id] = vx0 + dt/6.0f * (cart_acc1 + 2*cart_acc2 + 2*cart_acc3 + cart_acc4);
    for (int i = 0; i < N; ++i) {
        s->theta[env_id][i] = theta0[i] + dt/6.0f * (omega0[i] + 2*(omega0[i] + 0.5f*dt*theta_acc1[i]) + 2*(omega0[i] + 0.5f*dt*theta_acc2[i]) + (omega0[i] + dt*theta_acc3[i]));
        s->omega[env_id][i] = omega0[i] + dt/6.0f * (theta_acc1[i] + 2*theta_acc2[i] + 2*theta_acc3[i] + theta_acc4[i]);
    }
}

/* Alternative: Semi-implicit Euler (faster, less stable) — use for speed */
static void npole_euler_step(NPoleCartState* s, int env_id, float force) {
    int N = s->num_poles;
    float dt = s->dt;
    
    float theta_acc[BEAR_MAX_N_POLES], cart_acc;
    npole_compute_accelerations(s, env_id, force, theta_acc, &cart_acc);
    
    /* Semi-implicit: update velocities first, then positions */
    s->cart_vx[env_id] += dt * cart_acc;
    s->cart_x[env_id] += dt * s->cart_vx[env_id];
    
    for (int i = 0; i < N; ++i) {
        s->omega[env_id][i] += dt * theta_acc[i];
        s->theta[env_id][i] += dt * s->omega[env_id][i];
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * N-Pole Cartpole Step (Vectorized)
 * ═══════════════════════════════════════════════════════════════════ */

void bear_npolecart_step(BearEnv* e, const BearTensor* actions,
                          BearTensor* rewards, BearTensor* dones,
                          BearTensor* next_obs, BearArena* arena) {
    (void)arena;
    NPoleCartState* s = &g_npole_state;
    int n = e->spec.num_envs;
    int N = s->num_poles;
    int state_dim = s->state_dim;
    
    float* act_data = (float*)actions->data;
    float* rew = (float*)rewards->data;
    uint8_t* done = (uint8_t*)dones->data;
    float* obs = (float*)next_obs->data;
    
    for (int i = 0; i < n; ++i) {
        /* Clamp action to force limits */
        float force = act_data[i];
        if (isnan(force) || isinf(force)) force = 0.0f;
        if (force > s->force_mag) force = s->force_mag;
        if (force < -s->force_mag) force = -s->force_mag;
        
        /* Physics step — use RK4 for stability on high-N poles */
        npole_rk4_step(s, i, force);

        /* NaN guard: if physics produced NaN, reset this env */
        int nan_state = 0;
        if (isnan(s->cart_x[i]) || isnan(s->cart_vx[i])) nan_state = 1;
        for (int p2 = 0; p2 < N && !nan_state; ++p2) {
            if (isnan(s->theta[i][p2]) || isnan(s->omega[i][p2])) nan_state = 1;
        }
        if (nan_state) {
            npole_reset_env(s, i);
        }

        /* Track episode step */
        s->episode_step_counter[i]++;
        
        /* Compute reward (Yacine-style shaping) */
        float r = 1.0f;  /* Survival bonus */
        
        /* Per-pole upright bonus: cos(θ) ∈ [−1,1], max at upright (θ=0) */
        for (int p = 0; p < N; ++p) {
            r += 0.5f * cosf(s->theta[i][p]);
        }
        
        /* Cart centering penalty */
        r -= 0.1f * fabsf(s->cart_x[i] / s->cart_pos_threshold);
        r -= 0.01f * fabsf(s->cart_vx[i] / 10.0f);

        rew[i] = r;
        if (isnan(r) || isinf(r)) {
            r = 0.0f;
            rew[i] = r;
        }
        e->episode_return[i] += r;
        
        /* Done conditions */
        int is_done = 0;
        
        /* Any pole fallen? */
        for (int p = 0; p < N; ++p) {
            if (fabsf(s->theta[i][p]) > s->angle_threshold) {
                is_done = 1;
                break;
            }
        }
        
        /* Cart out of bounds? */
        if (fabsf(s->cart_x[i]) > s->cart_pos_threshold) {
            is_done = 1;
        }
        
        /* Episode length exceeded? */
        if (s->episode_step_counter[i] >= s->episode_length[i]) {
            is_done = 1;
        }
        
        /* Max episode steps (hard cap) */
        if (s->episode_step_counter[i] >= e->spec.max_episode_steps) {
            is_done = 1;
        }
        
        done[i] = is_done ? 1 : 0;
        
        /* Staggered reset: if done, reset THIS env immediately for next step */
        if (is_done) {
            /* Snapshot the completed episode return BEFORE reset zeroes it */
            if (e->episode_return_snapshot) {
                e->episode_return_snapshot[i] = e->episode_return[i];
            }
            npole_reset_env(s, i);
            /* Note: we DON'T write the reset state to obs this step;
               the next step will see the new initial state.
               This maintains constant batch size (PufferLib pattern). */
        }
        
        /* Fill next observation */
        int idx = i * state_dim;
        obs[idx + 0] = s->cart_x[i];
        obs[idx + 1] = s->cart_vx[i];
        for (int p = 0; p < N; ++p) {
            obs[idx + 2 + 2 * p]     = s->theta[i][p];
            obs[idx + 2 + 2 * p + 1] = s->omega[i][p];
        }
    }
    
    /* Update active env count */
    e->num_active_envs = 0;
    for (int i = 0; i < n; ++i) if (!done[i]) e->num_active_envs++;
}

/* ═══════════════════════════════════════════════════════════════════
 * Factory Integration for N-Pole Cartpole
 * ═══════════════════════════════════════════════════════════════════ */

/* Convenience function to create N-pole env with specific pole count */
BearEnv* bear_env_create_npole(int num_poles, int num_envs, BearArena* global_arena) {
    if (num_poles < 1) num_poles = 1;
    if (num_poles > BEAR_MAX_N_POLES) num_poles = BEAR_MAX_N_POLES;
    
    /* Create as custom type, then initialize with pole count */
    BearEnv* e = bear_env_create(BEAR_ENV_CUSTOM, num_envs, 1, 2 + 2 * num_poles, 1, 0, global_arena);
    if (!e) return NULL;
    
    e->type = BEAR_ENV_N_POLE_CART;
    e->user_data = &g_npole_state;  /* Store state for access */
    bear_npolecart_init(e, num_poles, global_arena);
    return e;
}

/* Get N-pole state for curriculum / inspection */
void* bear_env_get_npole_state(BearEnv* e) {
    if (!e || e->type != BEAR_ENV_N_POLE_CART) return NULL;
    return e->user_data;
}

/* N-pole curriculum accessors */
int bear_npole_get_episode_length_max(BearEnv* e) {
    if (!e || e->type != BEAR_ENV_N_POLE_CART) return -1;
    NPoleCartState* s = (NPoleCartState*)e->user_data;
    return s->episode_length_max;
}

void bear_npole_set_episode_length_max(BearEnv* e, int max_len) {
    if (!e || e->type != BEAR_ENV_N_POLE_CART) return;
    NPoleCartState* s = (NPoleCartState*)e->user_data;
    if (max_len > s->max_episode_steps) max_len = s->max_episode_steps;
    if (max_len < s->episode_length_min) max_len = s->episode_length_min;
    s->episode_length_max = max_len;
}