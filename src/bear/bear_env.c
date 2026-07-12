/*
 * bear_env.c  --  PufferC/BearRL Vectorized Environment Implementation
 */

#include "bear_env.h"
#include "bear_env_internal.h"
#include "bear_arena.h"
#include "bear_simd.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ===================================================================
 * Episode Step Counter Storage
 * =================================================================== */

/* Episode step/return scratch buffers (shared with subsystems via
 * bear_env_internal.h; canonical definitions here). */
int    *g_episode_step = NULL;
float  *g_episode_return = NULL;
float  *g_episode_return_snapshot = NULL;
int     g_max_envs = 0;

void ensure_episode_arrays(int num_envs) {
    if (num_envs > g_max_envs) {
        g_max_envs = num_envs;
        g_episode_step = realloc(g_episode_step, num_envs * sizeof(int));
        g_episode_return = realloc(g_episode_return, num_envs * sizeof(float));
        g_episode_return_snapshot = realloc(g_episode_return_snapshot, num_envs * sizeof(float));
    }
}

/* ===================================================================
 * Vectorized Environment Factory
 * =================================================================== */

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

/* ===================================================================
 * CartPole-v1 Implementation (SIMD-vectorized)
 * =================================================================== */

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
        
        /* Snapshot episode return when done (for trainer logging) */
        if (is_done && e->episode_return_snapshot) {
            e->episode_return_snapshot[i] = e->episode_return[i];
        }
        
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

/* ===================================================================
 * Squared (PufferLib-style gridworld)
 * =================================================================== */

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

/* ===================================================================
 * Custom Environment Registration
 * =================================================================== */

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
