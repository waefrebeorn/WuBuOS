/* Debug training: print rewards from chained_env_step */
#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_arena.h"
#include "src/bear/bear_nn.h"
#include "src/bear/bear_ppo.h"
#include "src/bear/bear_mujoco.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>

#define MAX_POLES 10
#define MAX_ENVS 16

static float norm_angle(float a) {
    while (a > 3.14159265f) a -= 6.28318531f;
    while (a < -3.14159265f) a += 6.28318531f;
    return a;
}

typedef struct {
    int nenv, npoles;
    float* cart_x;      float* cart_vx;
    float* theta_abs;   float* omega_abs;
    float* theta_rel_in;   float* omega_rel_in;
    float* theta_rel_out;  float* omega_rel_out;
    float* ep_step;     float* ep_ret;
    float* upright_time;
    float ep_len, ep_len_min, ep_len_max;
    float force_mag, angle_thresh, cart_thresh;
} ChainedEnv;

static ChainedEnv g_env;
#define ATH(e,p) g_env.theta_abs[(e)*g_env.npoles+(p)]
#define AOM(e,p) g_env.omega_abs[(e)*g_env.npoles+(p)]

static int chained_env_init(int nenv, int npoles) {
    if (nenv > MAX_ENVS) { fprintf(stderr,"Max envs %d\n",MAX_ENVS); return -1; }
    if (npoles > MAX_POLES) { fprintf(stderr,"Max poles %d\n",MAX_POLES); return -1; }
    memset(&g_env, 0, sizeof(g_env));
    g_env.nenv = nenv; g_env.npoles = npoles;
    int N = nenv * npoles;
    g_env.cart_x  = calloc(nenv, sizeof(float));
    g_env.cart_vx = calloc(nenv, sizeof(float));
    g_env.theta_abs  = calloc(N, sizeof(float));
    g_env.omega_abs  = calloc(N, sizeof(float));
    g_env.theta_rel_in  = calloc(N, sizeof(float));
    g_env.omega_rel_in  = calloc(N, sizeof(float));
    g_env.theta_rel_out = calloc(N, sizeof(float));
    g_env.omega_rel_out = calloc(N, sizeof(float));
    g_env.ep_step = calloc(nenv, sizeof(float));
    g_env.ep_ret  = calloc(nenv, sizeof(float));
    g_env.upright_time = calloc(nenv, sizeof(float));
    g_env.ep_len = 500.0f;
    g_env.ep_len_min = 50.0f;
    g_env.ep_len_max = 500.0f;
    g_env.force_mag = 80.0f; g_env.angle_thresh = 1.5708f; g_env.cart_thresh = 2.5f;
    int r = bear_mujoco_init(npoles, nenv);
    if (r != 0) return -1;
    return 0;
}

static void abs_to_rel(const float* a, float* r, int n) {
    r[0] = norm_angle(a[0]);
    for (int p = 1; p < n; ++p) r[p] = norm_angle(a[p] - a[p-1]);
}
static void rel_to_abs(const float* r, float* a, int n) {
    a[0] = norm_angle(r[0]);
    for (int p = 1; p < n; ++p) a[p] = norm_angle(a[p-1] + r[p]);
}

static void chained_env_step(const float* forces, float* rewards, uint8_t* dones) {
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
        float upright_sum = 0, progress_sum = 0;
        for (int p = 0; p < N; ++p) {
            float th = ATH(i, p);
            float d_from_up = fabsf(norm_angle(th));
            upright_sum += cosf(d_from_up);
            progress_sum += (3.14159265f - d_from_up) / 3.14159265f;
        }
        float upright = (upright_sum / N + 1.0f) * 0.5f;
        float progress = progress_sum / N;
        
        if (upright > 0.9f) g_env.upright_time[i] += 1.0f;
        else g_env.upright_time[i] = 0.0f;
        
        float cp = fabsf(g_env.cart_x[i]);
        float centered = (cp > 2.0f) ? fmaxf(0, 1-(cp-2)/(g_env.cart_thresh-2)) : 1.0f;
        centered = (1.0f + centered) * 0.5f;
        
        float vp = 0;
        for (int p = 0; p < N; ++p) {
            float w = fabsf(AOM(i, p));
            if (w > 5.0f) vp += (w - 5.0f) * 0.01f;
        }
        float sv = 1.0f / (1.0f + vp);
        
        float af = fabsf(forces[i]);
        float sc = (af > 1.0f) ? fmaxf(0, 1-(af-1)/(g_env.force_mag-1)) : 1.0f;
        sc = (4.0f + sc) / 5.0f;
        
        float base_progress = progress * 0.05f;
        float upright_bonus = upright * 0.5f;
        float time_bonus = sqrtf(g_env.upright_time[i]) * 0.01f * upright;
        float vel_toward_up = 0;
        for (int p = 0; p < N; ++p) {
            float th = ATH(i, p);
            float d = fabsf(norm_angle(th));
            float w = AOM(i, p);
            if (d > 0.1f) {
                float target_w = (norm_angle(th) > 0) ? -0.5f : 0.5f;
                vel_toward_up += 1.0f - fabsf(w - target_w);
            }
        }
        vel_toward_up = vel_toward_up / N * 0.1f;
        
        float r = (base_progress + upright_bonus + time_bonus + vel_toward_up) * centered * sv * sc;
        rewards[i] = (isnan(r)||isinf(r)) ? 0.0f : r;
        g_env.ep_ret[i] += rewards[i];
        g_env.ep_step[i] += 1;
        
        int done = 0;
        for (int p = 0; p < N && !done; ++p) {
            float d_from_up = fabsf(norm_angle(ATH(i, p)));
            if (d_from_up > 1.5708f) done = 1;
        }
        if (fabsf(g_env.cart_x[i]) > g_env.cart_thresh) done = 1;
        if (g_env.ep_step[i] >= g_env.ep_len) done = 1;
        dones[i] = done ? 1 : 0;
        
        if (done) {
            printf("[ENV %d] done: ret=%.2f, steps=%.0f, upright=%.4f, progress=%.4f\n", 
                i, g_env.ep_ret[i], g_env.ep_step[i], upright, progress);
            // reset
            g_env.cart_x[i]  = ((float)rand()/RAND_MAX-0.5f)*0.5f;
            g_env.cart_vx[i] = ((float)rand()/RAND_MAX-0.5f)*0.5f;
            for (int p = 0; p < N; ++p) {
                ATH(i, p) = 3.14159265f + ((float)rand()/RAND_MAX-0.5f)*0.52f;
                AOM(i, p) = ((float)rand()/RAND_MAX-0.5f)*0.5f;
            }
            g_env.ep_step[i] = 0; g_env.ep_ret[i] = 0;
            g_env.upright_time[i] = 0;
        }
    }
}

int main() {
    srand(42);
    if (chained_env_init(4, 1) != 0) return 1;  // 4 envs, 1 pole
    
    float forces[4];
    float rewards[4];
    uint8_t dones[4];
    
    // Run 1000 steps with random forces
    for (int step = 0; step < 1000; ++step) {
        for (int i = 0; i < 4; ++i) {
            forces[i] = ((float)rand() / RAND_MAX - 0.5f) * 160.0f;
        }
        chained_env_step(forces, rewards, dones);
        float avg_r = 0;
        for (int i = 0; i < 4; ++i) avg_r += rewards[i];
        avg_r /= 4;
        if (step % 100 == 0) {
            printf("Step %d: avg_reward=%.4f\n", step, avg_r);
        }
    }
    
    printf("Done. Final avg return: ");
    float sum = 0;
    for (int i = 0; i < 4; ++i) sum += g_env.ep_ret[i];
    printf("%.2f\n", sum / 4);
    
    return 0;
}