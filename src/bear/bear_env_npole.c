/*
 * bear_env_npole.c -- WuBuOS BearRL N-Pole Cartpole (7-10 poles)
 *
 * Extracted from bear_env.c (monolith split, C11, opaque-safe).
 * Self-contained physics: Recursive Lagrangian dynamics + RK4 integrator for
 * N linked inverted pendulums on a cart (Sovereign Bear Challenge).  Depends
 * only on the public BearEnv API (bear_env.h) and the shared episode state
 * declared in bear_env_internal.h.  No god headers.
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
 * N-Pole Cartpole (7-10 Poles)  --  Sovereign Bear Challenge
 * ===================================================================
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

/* ===================================================================
 * N-Pole Cartpole Initialization
 * =================================================================== */

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

/* ===================================================================
 * N-Pole Cartpole Reset (Staggered)
 * =================================================================== */

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

/* ===================================================================
 * Physics: Recursive Lagrangian Dynamics for N-Pole Cartpole
 * ===================================================================
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
    
    /* M[0][0] and F[0]  --  cart equation */
    M[0][0] = s->total_mass;
    for (int i = 0; i < N; ++i) {
        float cos_t = cosf(s->theta[env_id][i]);
        M[0][0] += s->pole_masses[i];
        M[0][i + 1] = s->pole_mass_length[i] * cos_t;
        M[i + 1][0] = M[0][i + 1];
    }
    
    /* M[i+1][j+1] and F[i+1]  --  pole equations */
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
    
    /* Add diagonal regularization to mass matrix for numerical stability
     * when poles are near horizontal (cos θ ≈ 0) making M ill-conditioned */
    const float REG_EPS = 1e-4f;
    for (int i = 0; i <= N; ++i) {
        M[i][i] += REG_EPS;
    }
    
    /* Solve M * acc = F for accelerations using Gaussian elimination */
    /* System size: (N+1) ≤ 11  --  small enough for dense solve */
    int sz = N + 1;
    
    float acc[BEAR_MAX_N_POLES + 1] = {0};
    
    /* Forward elimination */
    for (int i = 0; i < sz; ++i) {
        /* Pivot with safety */
        float pivot = M[i][i];
        if (fabsf(pivot) < 1e-10f) pivot = (pivot >= 0 ? 1e-10f : -1e-10f);
        
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

/* Alternative: Semi-implicit Euler (faster, less stable)  --  use for speed */
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

/* ===================================================================
 * N-Pole Cartpole Step (Vectorized)
 * =================================================================== */

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
        
        /* Physics step  --  use RK4 for stability on high-N poles */
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

/* ===================================================================
 * Factory Integration for N-Pole Cartpole
 * =================================================================== */

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
