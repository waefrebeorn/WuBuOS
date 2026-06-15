/*
 * test_knockover_standalone.c — Test knock-over reset without PPO
 */
#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_mujoco.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <string.h>

#define MAX_POLES 10
#define MAX_ENVS 16

typedef struct {
    int nenv, npoles;
    float* cart_x; float* cart_vx;
    float* theta_abs; float* omega_abs;
    float* theta_rel_in; float* omega_rel_in;
    float* theta_rel_out; float* omega_rel_out;
    float force_mag, angle_thresh, cart_thresh;
} ChainedEnv;

static ChainedEnv g_env;
#define ATH(e,p) g_env.theta_abs[(e)*g_env.npoles+(p)]
#define AOM(e,p) g_env.omega_abs[(e)*g_env.npoles+(p)]

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static inline float norm_angle(float a) {
    while (a > M_PI) a -= 2.0f * M_PI;
    while (a < -M_PI) a += 2.0f * M_PI;
    return a;
}

static inline void abs_to_rel(const float* a, float* r, int n) {
    r[0] = norm_angle(a[0]);
    for (int p = 1; p < n; ++p) r[p] = norm_angle(a[p] - a[p-1]);
}
static inline void rel_to_abs(const float* r, float* a, int n) {
    a[0] = norm_angle(r[0]);
    for (int p = 1; p < n; ++p) a[p] = norm_angle(a[p-1] + r[p]);
}

void chained_env_step(const float* forces, float* rewards, uint8_t* dones);

static void simulate_fall(int env_id, int steps, float kick_force) {
    float force = kick_force;
    for (int step = 0; step < steps; ++step) {
        float forces[MAX_ENVS] = {0};
        float rewards[MAX_ENVS] = {0};
        uint8_t dones[MAX_ENVS] = {0};
        forces[env_id] = force;
        force = 0.0f;
        chained_env_step(forces, rewards, dones);
    }
}

int chained_env_init(int nenv, int npoles) {
    if (nenv > MAX_ENVS) return -1;
    if (npoles > MAX_POLES) return -1;
    memset(&g_env, 0, sizeof(g_env));
    g_env.nenv = nenv; g_env.npoles = npoles;
    int N = nenv * npoles;
    g_env.cart_x = calloc(nenv, sizeof(float));
    g_env.cart_vx = calloc(nenv, sizeof(float));
    g_env.theta_abs = calloc(N, sizeof(float));
    g_env.omega_abs = calloc(N, sizeof(float));
    g_env.theta_rel_in = calloc(N, sizeof(float));
    g_env.omega_rel_in = calloc(N, sizeof(float));
    g_env.theta_rel_out = calloc(N, sizeof(float));
    g_env.omega_rel_out = calloc(N, sizeof(float));
    g_env.force_mag = 80.0f; g_env.angle_thresh = 1.5708f; g_env.cart_thresh = 2.5f;
    int r = bear_mujoco_init(npoles, nenv);
    if (r != 0) return -1;
    printf("[MuJoCo] %d-pole, %d envs\n", npoles, nenv);
    return 0;
}

void chained_env_close(void) {
    bear_mujoco_shutdown();
    free(g_env.cart_x); free(g_env.cart_vx);
    free(g_env.theta_abs); free(g_env.omega_abs);
    free(g_env.theta_rel_in); free(g_env.omega_rel_in);
    free(g_env.theta_rel_out); free(g_env.omega_rel_out);
    memset(&g_env, 0, sizeof(g_env));
}

void chained_env_reset(int env_id) {
    int N = g_env.npoles;
    
    /* Phase 1: Upright */
    g_env.cart_x[env_id] = ((float)rand()/RAND_MAX-0.5f)*0.1f;
    g_env.cart_vx[env_id] = 0.0f;
    for (int p = 0; p < N; ++p) { ATH(env_id,p) = 0.0f; AOM(env_id,p) = 0.0f; }
    
    /* Phase 2: Kick */
    float dir = (rand() % 2) ? 1.0f : -1.0f;
    float kick = g_env.force_mag * (0.3f + ((float)rand()/RAND_MAX)*0.4f);
    float force = dir * kick;
    
    /* Phase 3: Fall naturally */
    int fall = 50 + rand() % 50;
    simulate_fall(env_id, fall, force);
    
    /* Phase 4: Noise */
    g_env.cart_x[env_id] += ((float)rand()/RAND_MAX-0.5f)*0.2f;
    g_env.cart_vx[env_id] += ((float)rand()/RAND_MAX-0.5f)*0.5f;
    for (int p = 0; p < N; ++p) {
        ATH(env_id,p) += ((float)rand()/RAND_MAX-0.5f)*0.1f;
        AOM(env_id,p) += ((float)rand()/RAND_MAX-0.5f)*0.2f;
    }
    
    abs_to_rel(&ATH(env_id,0), &g_env.theta_rel_in[env_id*N], N);
    g_env.omega_rel_in[env_id*N] = AOM(env_id,0);
    for (int p = 1; p < N; ++p)
        g_env.omega_rel_in[env_id*N+p] = AOM(env_id,p) - AOM(env_id,p-1);
    
    bear_mujoco_reset_env(env_id, N, g_env.cart_x[env_id], g_env.cart_vx[env_id],
        &g_env.theta_rel_in[env_id*N], &g_env.omega_rel_in[env_id*N]);
    
    printf("[RESET] env=%d fallen_theta1=%.3f (%.1f°) kick=%.1f steps=%d\n",
           env_id, ATH(env_id,0), ATH(env_id,0)*180/M_PI, force, fall);
}

void chained_env_reset_all(void) {
    for (int i = 0; i < g_env.nenv; ++i) chained_env_reset(i);
}

void chained_env_step(const float* forces, float* rewards, uint8_t* dones) {
    int N = g_env.npoles, B = g_env.nenv;
    for (int e = 0; e < B; ++e) {
        abs_to_rel(&ATH(e,0), &g_env.theta_rel_in[e*N], N);
        g_env.omega_rel_in[e*N] = AOM(e,0);
        for (int p = 1; p < N; ++p)
            g_env.omega_rel_in[e*N+p] = AOM(e,p)-AOM(e,p-1);
    }
    bear_mujoco_step(B, N, g_env.cart_x, g_env.cart_vx,
        g_env.theta_rel_in, g_env.omega_rel_in,
        forces, 0.005f,
        g_env.cart_x, g_env.cart_vx,
        g_env.theta_rel_out, g_env.omega_rel_out);
    for (int e = 0; e < B; ++e) {
        rel_to_abs(&g_env.theta_rel_out[e*N], &ATH(e,0), N);
        AOM(e,0) = g_env.omega_rel_out[e*N];
        for (int p = 1; p < N; ++p)
            AOM(e,p) = AOM(e,p-1) + g_env.omega_rel_out[e*N+p];
    }
    for (int i = 0; i < B; ++i) {
        rewards[i] = 1.0f;
        int done = 0;
        for (int p = 0; p < N && !done; ++p)
            if (fabsf(norm_angle(ATH(i,p))) > 1.5708f) done = 1;
        if (fabsf(g_env.cart_x[i]) > g_env.cart_thresh) done = 1;
        dones[i] = done;
        if (done) chained_env_reset(i);
    }
}

int main(void) {
    srand(time(NULL));
    if (chained_env_init(4, 3) != 0) return 1;
    
    printf("\n=== Testing Knock-Over Reset ===\n\n");
    
    for (int test = 0; test < 3; ++test) {
        printf("\n--- Reset Batch %d ---\n", test+1);
        chained_env_reset_all();
        
        float forces[MAX_ENVS] = {0};
        float rewards[MAX_ENVS];
        uint8_t dones[MAX_ENVS];
        
        for (int step = 0; step < 20; ++step) {
            chained_env_step(forces, rewards, dones);
            printf("  Step %d: thetas=[%.3f, %.3f, %.3f, %.3f] deg=[%.1f, %.1f, %.1f, %.1f]\n", step,
                ATH(0,0), ATH(1,0), ATH(2,0), ATH(3,0),
                ATH(0,0)*180/M_PI, ATH(1,0)*180/M_PI, ATH(2,0)*180/M_PI, ATH(3,0)*180/M_PI);
        }
    }
    
    chained_env_close();
    printf("\n=== Test Complete ===\n");
    return 0;
}