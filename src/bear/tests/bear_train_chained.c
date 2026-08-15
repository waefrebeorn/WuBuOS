/*
 * bear_train_chained.c — N-pole CHAINED cartpole training with knock-over reset
 * Progressive curriculum: 1→2→...→10 poles
 * Uses official BearRL PPO API with Q-controller LR scheduling
 */
#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_arena.h"
#include "src/bear/bear_nn.h"
#include "src/bear/bear_ppo.h"
#include "src/bear/bear_opt.h"
#include "src/bear/bear_qcontroller.h"
#include "src/bear/bear_mujoco.h"
#include "src/bear/bear_holo_opt.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <alloca.h>
#include <sys/stat.h>

#define MAX_STEPS 750
#define ROLLOUT_LEN 1024
#define MAX_POLES 20
#define MAX_ENVS 32

typedef struct {
    int nenv, npoles;
    float* cart_x;      float* cart_vx;
    float* theta_abs;   float* omega_abs;
    float* theta_rel_in;   float* omega_rel_in;
    float* theta_rel_out;  float* omega_rel_out;
    float* ep_step;     float* ep_ret;
    float* last_ep_ret;
    float* upright_time;
    float ep_len;
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

/* ═══════════════════════════════════════════════════════════════════
 * Energy-Based Swing-Up Controller (for BC expert demos)
 * ═══════════════════════════════════════════════════════════════════ */
static inline float compute_energy(int env_id, int npoles) {
    float E = 0.5f * 1.0f * g_env.cart_vx[env_id] * g_env.cart_vx[env_id];
    for (int p = 0; p < npoles; ++p) {
        float mi = 0.30f * powf(0.82f, p);
        float li = 0.40f * powf(0.90f, p);
        float lc = li * 0.5f;
        float I = mi * li * li / 3.0f;
        
        float th_sum = 0.0f, thd_sum = 0.0f;
        for (int k = 0; k <= p; ++k) {
            th_sum += ATH(env_id, k);
            thd_sum += AOM(env_id, k);
        }
        
        float x_com = g_env.cart_x[env_id];
        float y_com = 0.0f;
        for (int j = 0; j <= p; ++j) {
            float th = 0.0f;
            for (int k = 0; k <= j; ++k) th += ATH(env_id, k);
            if (j < p) {
                x_com += (j == 0 ? 0.0f : 0.40f * powf(0.90f, j-1)) * sinf(th);
                y_com -= (j == 0 ? 0.0f : 0.40f * powf(0.90f, j-1)) * cosf(th);
            } else {
                x_com += lc * sinf(th);
                y_com -= lc * cosf(th);
            }
        }
        
        float vx = g_env.cart_vx[env_id];
        float vy = 0.0f;
        thd_sum = 0.0f;
        for (int j = 0; j <= p; ++j) {
            float th = 0.0f;
            for (int k = 0; k <= j; ++k) { th += ATH(env_id, k); thd_sum += AOM(env_id, k); }
            float factor = (j < p) ? (j == 0 ? 0.0f : 0.40f * powf(0.90f, j-1)) : lc;
            vx += factor * cosf(th) * thd_sum;
            vy += factor * sinf(th) * thd_sum;
        }
        
        E += 0.5f * mi * (vx*vx + vy*vy) + 0.5f * I * thd_sum * thd_sum;
        E += mi * 9.81f * y_com;
    }
    return E;
}

static inline int near_upright(int env_id, int npoles, float thresh) {
    for (int p = 0; p < npoles; ++p) {
        if (fabsf(norm_angle(ATH(env_id, p))) > thresh) return 0;
    }
    return fabsf(g_env.cart_x[env_id]) < 2.5f * 0.5f;
}

/* LQR gains (tuned for multi-pole) */
static inline double lqr_ctrl(int env_id, int npoles) {
    double u = 0.0;
    u -= 10.0 * g_env.cart_x[env_id];
    for (int p = 0; p < npoles; ++p) u -= (150.0 + p * 10.0) * ATH(env_id, p);
    u -= 15.0 * g_env.cart_vx[env_id];
    for (int p = 0; p < npoles; ++p) u -= (5.0 + p * 2.0) * AOM(env_id, p);
    if (u > 80.0) u = 80.0;
    if (u < -80.0) u = -80.0;
    return u;
}

/* Energy-based swing-up controller */
static inline double swing_ctrl(int env_id, int npoles, double Et) {
    float E = compute_energy(env_id, npoles);
    double dE = E - Et;
    float th1 = ATH(env_id, 0);
    float thd1 = AOM(env_id, 0);
    float x = g_env.cart_x[env_id];
    float xd = g_env.cart_vx[env_id];
    
    if (fabsf(thd1) < 1e-6f) return (x > 0) ? -5.0 : 5.0;
    double sgn = (thd1 >= 0) ? 1.0 : -1.0;
    double k = 15.0;
    double u = k * dE * cosf(th1) * sgn - 10.0 * x - 2.0 * xd;
    if (u > 80.0) u = 80.0;
    if (u < -80.0) u = -80.0;
    return u;
}

/* Expert swing-up controller: energy pump → LQR handoff */
static float swingup_control(int env_id, int npoles) {
    static double Et = 5.0;
    Et = 5.0 * npoles;
    
    if (near_upright(env_id, npoles, 0.08f)) {
        return (float)lqr_ctrl(env_id, npoles);
    } else {
        return (float)swing_ctrl(env_id, npoles, Et);
    }
}

static void chained_env_step(const float* forces, float* rewards, uint8_t* dones);

static void simulate_fall(int env_id, int steps, float kick_force) {
    float force = kick_force;
    for (int step = 0; step < steps; ++step) {
        float forces[MAX_ENVS] = {0};
        float rewards[MAX_ENVS] = {0};
        uint8_t dones[MAX_ENVS] = {0};
        forces[env_id] = force;
        force = 0.0f;
        int N = g_env.npoles;
        for (int e = 0; e < g_env.nenv; ++e) {
            abs_to_rel(&ATH(e,0), &g_env.theta_rel_in[e*N], N);
            g_env.omega_rel_in[e*N] = AOM(e,0);
            for (int p = 1; p < N; ++p)
                g_env.omega_rel_in[e*N+p] = AOM(e,p)-AOM(e,p-1);
        }
        bear_mujoco_step(g_env.nenv, N, g_env.cart_x, g_env.cart_vx,
            g_env.theta_rel_in, g_env.omega_rel_in,
            forces, 0.005f,
            g_env.cart_x, g_env.cart_vx,
            g_env.theta_rel_out, g_env.omega_rel_out);
        for (int e = 0; e < g_env.nenv; ++e) {
            rel_to_abs(&g_env.theta_rel_out[e*N], &ATH(e,0), N);
            AOM(e,0) = g_env.omega_rel_out[e*N];
            for (int p = 1; p < N; ++p)
                AOM(e,p) = AOM(e,p-1) + g_env.omega_rel_out[e*N+p];
        }
        g_env.ep_step[env_id] += 1;
    }
}

int chained_env_init(int nenv, int npoles) {
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
    g_env.last_ep_ret = calloc(nenv, sizeof(float));
    g_env.upright_time = calloc(nenv, sizeof(float));
    g_env.ep_len = MAX_STEPS;
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
    free(g_env.ep_step); free(g_env.ep_ret);
    free(g_env.last_ep_ret);
    free(g_env.upright_time);
    memset(&g_env, 0, sizeof(g_env));
}

/* Knock-over reset: start upright -> random kick -> fall naturally => fallen state */
void chained_env_reset(int env_id) {
    int N = g_env.npoles;
    
    g_env.cart_x[env_id] = ((float)rand()/RAND_MAX-0.5f)*0.1f;
    g_env.cart_vx[env_id] = 0.0f;
    for (int p = 0; p < N; ++p) { ATH(env_id, p) = 0.0f; AOM(env_id, p) = 0.0f; }
    
    float kick_direction = (rand() % 2) ? 1.0f : -1.0f;
    float kick_magnitude = g_env.force_mag * (0.3f + ((float)rand()/RAND_MAX) * 0.4f);
    float kick_force = kick_direction * kick_magnitude;
    
    int fall_steps = 50 + rand() % 50;
    simulate_fall(env_id, fall_steps, kick_force);
    
    g_env.cart_x[env_id]  += ((float)rand()/RAND_MAX-0.5f)*0.2f;
    g_env.cart_vx[env_id] += ((float)rand()/RAND_MAX-0.5f)*0.5f;
    for (int p = 0; p < N; ++p) {
        ATH(env_id, p) += ((float)rand()/RAND_MAX-0.5f)*0.1f;
        AOM(env_id, p) += ((float)rand()/RAND_MAX-0.5f)*0.2f;
    }
    
    g_env.ep_step[env_id] = 0; g_env.ep_ret[env_id] = 0;
    g_env.upright_time[env_id] = 0;
    
    abs_to_rel(&ATH(env_id,0), &g_env.theta_rel_in[env_id*N], N);
    g_env.omega_rel_in[env_id*N] = AOM(env_id,0);
    for (int p = 1; p < N; ++p)
        g_env.omega_rel_in[env_id*N+p] = AOM(env_id,p) - AOM(env_id,p-1);
    
    bear_mujoco_reset_env(env_id, N, g_env.cart_x[env_id], g_env.cart_vx[env_id],
        &g_env.theta_rel_in[env_id*N], &g_env.omega_rel_in[env_id*N]);
    
    printf("[RESET] env=%d fallen_theta1=%.3f (%.1f°) kick=%.1f steps=%d\n", 
           env_id, ATH(env_id,0), ATH(env_id,0)*180.0f/M_PI, kick_force, fall_steps);
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
        
        float base_progress = progress * 0.5f;
        float upright_bonus = upright * 2.0f;
        float time_bonus = sqrtf(g_env.upright_time[i]) * 0.1f * upright;
        float vel_toward_up = 0;
        for (int p = 0; p < N; ++p) {
            float th = ATH(i, p);
            float d = fabsf(norm_angle(th));
            float w = AOM(i, p);
            if (d > 0.1f) {
                float target_w = (norm_angle(th) > 0) ? -0.5f : 0.5f;
                float vel_err = fabsf(w - target_w);
                if (vel_err > 5.0f) vel_err = 5.0f;  /* Clamp to prevent huge penalties */
                vel_toward_up += 1.0f - vel_err;
            }
        }
        vel_toward_up = vel_toward_up / N * 0.2f;  /* Reduce weight from 0.5 to 0.2 */
        
        float survival_bonus = 0.1f;
        
        float r = (base_progress + upright_bonus + time_bonus + vel_toward_up + survival_bonus) * centered * sv * sc;
        /* Manual NaN/Inf check since isnan/isinf may not work reliably with float */
        if (r != r || r > 1e30f || r < -1e30f) r = 0.0f;
        rewards[i] = r;
        g_env.ep_ret[i] += rewards[i];
        g_env.ep_step[i] += 1;
        
        /* Debug: print reward for first env */
        if (i == 0 && g_env.ep_step[i] <= 3) {
            printf("[REWARD] ep=%.0f step=%.0f r=%.3f base=%.3f up=%.3f time=%.3f vel=%.3f surv=%.3f cent=%.3f sv=%.3f sc=%.3f\\n",
                   g_env.ep_ret[i], g_env.ep_step[i], r, base_progress, upright_bonus, time_bonus, vel_toward_up, survival_bonus, centered, sv, sc);
        }
        
        int done = 0;
        /* For swing-up: do NOT terminate on pole angle > pi/2.
         * Knock-over reset starts episodes in fallen state (angle > pi/2).
         * Policy needs time to swing up. Only terminate on cart OOB or max steps. */
        if (fabsf(g_env.cart_x[i]) > g_env.cart_thresh) done = 1;
        if (g_env.ep_step[i] >= g_env.ep_len) done = 1;
        dones[i] = done ? 1 : 0;
        
        /* Capture episode return BEFORE potential reset */
        if (done) {
            g_env.last_ep_ret[i] = g_env.ep_ret[i];
            chained_env_reset(i);
        }
    }
}

/* Geometric Encoder */
typedef struct { int num_layers; int* ls; float* w; float* b; int in_dim, out_dim; } GeoEnc;
static GeoEnc* geo_create(BearArena* a, int id, int od, int nl) {
    GeoEnc* g = (GeoEnc*)bear_arena_alloc(a,sizeof(GeoEnc),1);
    if(!g)return NULL; g->in_dim=id; g->out_dim=od; g->num_layers=nl;
    g->ls=(int*)bear_arena_alloc(a,sizeof(int)*(nl+1),1);
    g->ls[0]=id; float p=1.618f;
    for(int i=1;i<nl;++i){float s=(i%2)?(1/p):p;float sz=g->ls[i-1]*s;g->ls[i]=(int)(sz+0.5f);if(g->ls[i]<16)g->ls[i]=16;if(g->ls[i]>1024)g->ls[i]=1024;}
    g->ls[nl]=od; int tw=0,tb=0;
    for(int i=0;i<nl;++i){tw+=g->ls[i]*g->ls[i+1];tb+=g->ls[i+1];}
    g->w=(float*)bear_arena_alloc(a,tw*sizeof(float),1);
    g->b=(float*)bear_arena_alloc(a,tb*sizeof(float),1);
    int wi=0,bi=0; uint32_t s=0xDEADBEEF;
    for(int L=0;L<nl;++L){int fi=g->ls[L],fo=g->ls[L+1];float std=sqrtf(2.0f/fi)*((L%2)?0.618f:1.618f);
        for(int j=0;j<fi*fo;++j){s=s*1664525u+1013904223u;float rr=(float)(s&0x7FFFFFFF)/2147483647.0f*2.0f-1.0f;g->w[wi++]=rr*std;}
        for(int j=0;j<fo;++j)g->b[bi++]=0;}
    return g;
}
static void geo_fwd(const GeoEnc* g, const float* in, float* out) {
    int mx=0;for(int i=0;i<=g->num_layers;++i)if(g->ls[i]>mx)mx=g->ls[i];
    float* p=(float*)alloca(mx*sizeof(float));float* c=(float*)alloca(mx*sizeof(float));
    memcpy(p,in,g->in_dim*sizeof(float));int wi=0,bi=0;
    for(int L=0;L<g->num_layers;++L){int id=g->ls[L],od=g->ls[L+1];
        for(int j=0;j<od;++j){float s=g->b[bi++];for(int k=0;k<id;++k)s+=p[k]*g->w[wi++];float gg=0.5f*s*(1+tanhf(0.79788456f*(s+0.044715f*s*s*s)));c[j]=gg*((L%2)?0.618f:1.618f);}
        memcpy(p,c,od*sizeof(float));}
    memcpy(out,p,g->out_dim*sizeof(float));
}

/* Build obs: [x, vx, sinθ1, cosθ1, ω1, ...] */
static void build_obs(int e, float* o, int np) {
    o[0]=g_env.cart_x[e]; o[1]=g_env.cart_vx[e];
    for(int p=0;p<np;++p){o[2+4*p]=sinf(ATH(e,p));o[3+4*p]=cosf(ATH(e,p));o[4+4*p]=AOM(e,p);}
}

int main(int argc, char** argv) {
    int nenv = 16; int max_poles = 10; int iters = 80; int seed = (int)time(NULL);
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i],"--envs")) nenv = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--curriculum")) max_poles = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--iters")) iters = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--seed")) seed = atoi(argv[++i]);
        else if (!strcmp(argv[i],"--help")) { printf("Usage: %s [--envs N] [--curriculum N] [--iters N] [--seed N]\n",argv[0]);return 0;}
    }
    srand(seed);
    printf("Seed: %d\n", seed);

    BearArena g_arena, r_arena, s_arena;
    BearPolicyNet pl = {0}; BearValueNet cr = {0};
    BearPPOConfig cfg = bear_ppo_default_config();
    /* Increase entropy for exploration, anneal over curriculum */
    cfg.ent_coef = 0.05f;
    GeoEnc *pe = NULL, *ve = NULL;
    int first = 1;

    for (int np = 1; np <= max_poles; ++np) {
        printf("\n===== CURRICULUM: %d POLES (%d iters) =====\n", np, iters);

        int od = 2 + 4*np;
        
        /* Anneal entropy: high for early poles (exploration), lower for later (exploitation) */
        cfg.ent_coef = 0.05f * (1.0f - 0.5f * (float)(np - 1) / (float)max_poles);

        if (first) {
            if (bear_arena_create(&g_arena, 256*1024*1024) ||
                bear_arena_create(&r_arena, 128*1024*1024) ||
                bear_arena_create(&s_arena, 256*1024*1024)) return 1;
            if (chained_env_init(nenv, np) != 0) return 1;
            first = 0;
        } else {
            chained_env_close();
            if (chained_env_init(nenv, np) != 0) return 1;
        }

        pe = geo_create(&g_arena, od, 128, 4);
        ve = geo_create(&g_arena, od, 64, 3);
        if (!pe || !ve) return 1;

        if (!pl.layers) {
            int ph[] = {128,128};
            if (bear_policy_create_mlp(&pl, &g_arena, 128, 1, 0, ph, 2)) return 1;
            bear_orthogonal_init_params(&pl, 1.0f); pl.logstd = NULL; pl.logstd_fixed = 0.0f;
        }
        if (!cr.layers) {
            int vh[] = {64,64};
            if (bear_value_create(&cr, &g_arena, 64, vh, 2)) return 1;
            bear_value_orthogonal_init(&cr, 1.0f);
        }

        BearOptimizer* opt_policy = bear_optimizer_create(&g_arena, BEAR_OPT_ADAM, 3e-4f);
        BearOptimizer* opt_value = bear_optimizer_create(&g_arena, BEAR_OPT_ADAM, 1e-3f);

        /* Q-Controller for adaptive LR scheduling */
        BearQControllerConfig qc_cfg = bear_qcontroller_default_config();
        BearQController qc_policy, qc_value;
        bear_qcontroller_init(&qc_policy, &qc_cfg, 3e-4f, (uint64_t)seed ^ 0x12345678);
        bear_qcontroller_init(&qc_value, &qc_cfg, 1e-3f, (uint64_t)seed ^ 0x87654321);
        /* Use smaller minibatch for stability */
        cfg.minibatch_size = 128;
        
        /* Allocate buffers for BC pretraining (reused for main training) */
        float* obs = malloc(nenv * od * sizeof(float));
        float* forces = malloc(nenv * sizeof(float));
        float* rewards = malloc(nenv * sizeof(float));
        uint8_t* dones = malloc(nenv * sizeof(uint8_t));
        
        /* Create HoloOpt for BC pretraining (faster convergence) */
        BearHoloConfig holo_cfg = bear_holo_default_config();
        holo_cfg.base_lr = 1e-3;
        /* Count policy parameters manually (since GAAD removed) */
        int n_policy_params = 0;
        for (int i = 0; i < pl.num_layers; ++i) {
            if (pl.layers[i].param && pl.layers[i].param->weight.data) {
                n_policy_params += (int)bear_tensor_numel(&pl.layers[i].param->weight);
            }
        }
        BearHoloOptimizer* holo_bc = bear_holo_create(&g_arena, &holo_cfg, n_policy_params);
        if (!holo_bc) {
            printf("[BC] Warning: HoloOpt creation failed, falling back to Adam\n");
        }
        
        /* Allocate flat buffers for BC HoloOpt */
        float* bc_flat_w = (float*)malloc(n_policy_params * sizeof(float));
        float* bc_flat_g = (float*)malloc(n_policy_params * sizeof(float));
        if (!bc_flat_w || !bc_flat_g) {
            printf("[BC] Warning: Failed to allocate flat buffers\n");
        }
        
        /* ── BC Pretraining with Expert Swing-Up ── */
        printf("[BC] Pretraining with expert swing-up (HoloOpt)...\n");
        BearTrajectory bc_traj;
        if (bear_traj_init(&bc_traj, &r_arena, 2048, nenv, 1, od, 1, 0) != 0) return 1;
        
        for (int bc_iter = 0; bc_iter < 20; ++bc_iter) {
            bear_arena_reset(&s_arena);
            
            /* Collect expert demos */
            for (int step = 0; step < 256; ++step) {
                bear_arena_reset(&s_arena);
                for (int e = 0; e < nenv; ++e) build_obs(e, &obs[e*od], np);
                
                for (int i = 0; i < nenv; ++i) forces[i] = swingup_control(i, np);
                
                chained_env_step(forces, rewards, dones);
                
                BearTensor obs_t, pe_enc, ve_enc;
                int64_t obs_shape[2] = {nenv, od}; 
                bear_tensor_create(&s_arena, &obs_t, obs_shape, 2, BEAR_DTYPE_F32, "obs");
                memcpy(obs_t.data, obs, nenv*od*sizeof(float));
                
                int64_t enc_shape[2] = {nenv, 128};
                bear_tensor_create(&s_arena, &pe_enc, enc_shape, 2, BEAR_DTYPE_F32, "pe");
                int64_t venc_shape[2] = {nenv, 64};
                bear_tensor_create(&s_arena, &ve_enc, venc_shape, 2, BEAR_DTYPE_F32, "ve");
                
                float* peo = (float*)pe_enc.data; float* veo = (float*)ve_enc.data;
                for (int e = 0; e < nenv; ++e) { geo_fwd(pe, &obs[e*od], peo+e*128); geo_fwd(ve, &obs[e*od], veo+e*64); }
                
                BearTensor ac, lp, vl, ho;
                int64_t ac_shape[2] = {nenv, 1};
                bear_tensor_create(&s_arena, &ac, ac_shape, 2, BEAR_DTYPE_F32, "ac");
                int64_t lp_shape[1] = {nenv};
                bear_tensor_create(&s_arena, &lp, lp_shape, 1, BEAR_DTYPE_F32, "lp");
                bear_tensor_create(&s_arena, &vl, lp_shape, 1, BEAR_DTYPE_F32, "vl");
                int64_t ho_shape[2] = {nenv,128};
                bear_tensor_create(&s_arena, &ho, ho_shape, 2, BEAR_DTYPE_F32, "ho");
                
                for (int i = 0; i < nenv; ++i) ((float*)ac.data)[i] = forces[i];
                
                BearTensor rw_t, dn_t;
                bear_tensor_create(&s_arena, &rw_t, lp_shape, 1, BEAR_DTYPE_F32, "rw");
                bear_tensor_create(&s_arena, &dn_t, lp_shape, 1, BEAR_DTYPE_U8, "dn");
                memcpy(rw_t.data, rewards, nenv*sizeof(float));
                memcpy(dn_t.data, dones, nenv*sizeof(uint8_t));
                
                bear_traj_store(&bc_traj, step, &obs_t, &ac, &lp, &rw_t, &dn_t, &vl);
            }
            
            /* BC training epoch with MSE loss + HoloOpt */
            for (int bc_epoch = 0; bc_epoch < 5; ++bc_epoch) {
                bear_arena_reset(&s_arena);
                BearMinibatchSampler sampler;
                bear_sampler_init(&sampler, &bc_traj, cfg.minibatch_size, (uint64_t[2]){rand(), rand()});
                
                BearTensor mb_obs, mb_act, mb_lp, mb_adv, mb_ret, mb_val, mb_oldlp;
                while (bear_sampler_next(&sampler, &bc_traj, &mb_obs, &mb_act, &mb_lp, &mb_adv, &mb_ret, &mb_val, &mb_oldlp, &s_arena)) {
                    BearTensor mb_pe_enc;
                    int64_t mb_enc_shape[2] = {cfg.minibatch_size, 128};
                    if (bear_tensor_create(&s_arena, &mb_pe_enc, mb_enc_shape, 2, BEAR_DTYPE_F32, "pe") != 0) { fprintf(stderr, "[BC] OOM: mb_pe_enc\n"); return 1; }
                    float* mpo = (float*)mb_obs.data;
                    for (int i = 0; i < cfg.minibatch_size; ++i) geo_fwd(pe, &mpo[i*od], &((float*)mb_pe_enc.data)[i*128]);
                    
                    if (!mb_pe_enc.data) { fprintf(stderr, "[BC] NULL mb_pe_enc.data\n"); return 1; }
                    
                    /* MSE loss for BC: compute policy forward, get mu, compare with expert actions */
                    BearTensor bc_new_actions, bc_new_logprobs, bc_new_values, bc_h_out;
                    int64_t ac_shape2[2] = {cfg.minibatch_size, 1};
                    int64_t lp_shape2[1] = {cfg.minibatch_size};
                    int64_t ho_shape2[2] = {cfg.minibatch_size, 128};
                    if (bear_tensor_create(&s_arena, &bc_new_actions, ac_shape2, 2, BEAR_DTYPE_F32, "bc_act") != 0) { fprintf(stderr, "[BC] OOM: bc_new_actions\n"); return 1; }
                    if (bear_tensor_create(&s_arena, &bc_new_logprobs, lp_shape2, 1, BEAR_DTYPE_F32, "bc_lp") != 0) { fprintf(stderr, "[BC] OOM: bc_new_logprobs\n"); return 1; }
                    if (bear_tensor_create(&s_arena, &bc_new_values, lp_shape2, 1, BEAR_DTYPE_F32, "bc_val") != 0) { fprintf(stderr, "[BC] OOM: bc_new_values\n"); return 1; }
                    if (bear_tensor_create(&s_arena, &bc_h_out, ho_shape2, 2, BEAR_DTYPE_F32, "bc_ho") != 0) { fprintf(stderr, "[BC] OOM: bc_h_out\n"); return 1; }
                    if (!bc_new_actions.data || !bc_new_logprobs.data || !bc_new_values.data || !bc_h_out.data) {
                        fprintf(stderr, "[BC] NULL output tensor data\n");
                        return 1;
                    }
                    
                    /* Forward pass to get mu (mean of Gaussian policy) */
                    bear_policy_forward(&pl, &mb_pe_enc, 0, &bc_new_actions, &bc_new_logprobs, &bc_new_values, &bc_h_out, &s_arena);
                    
                    /* Compute MSE gradient: dL/dmu = 2/mb * (mu - expert_action) / var */
                    int mb = cfg.minibatch_size;
                    int act_dim = 1;
                    float ls = pl.logstd ? 0.0f : pl.logstd_fixed;
                    float var = expf(2.0f * ls);
                    float* mu = (float*)pl.layers[pl.num_layers - 1].z_pre.data;
                    float* expert = (float*)mb_act.data;
                    
                    /* Zero policy grads */
                    bear_policy_zero_grad(&pl);
                    
                    /* Backward: dL/dmu = 2 * (mu - expert) / var / mb */
                    float* dmu = (float*)BEAR_ARENA_ALLOC(&s_arena, float, mb * act_dim);
                    for (int i = 0; i < mb; ++i) {
                        for (int a = 0; a < act_dim; ++a) {
                            dmu[i * act_dim + a] = 2.0f * (mu[i * act_dim + a] - expert[i * act_dim + a]) / var / (float)mb;
                        }
                    }
                    
                    /* Backprop through policy network layers */
                    bear_policy_backward_continuous_custom(&pl, &mb_pe_enc, &mb_act, dmu, &s_arena);
                    
                    if (holo_bc && bc_flat_w && bc_flat_g) {
                        /* Pack weights and grads into flat arrays for HoloOpt */
                        int offset = 0;
                        for (int i = 0; i < pl.num_layers && offset < n_policy_params; ++i) {
                            BearParam* p = pl.layers[i].param;
                            if (!p) continue;
                            int nw = (int)bear_tensor_numel(&p->weight);
                            if (offset + nw > n_policy_params) break;
                            memcpy(bc_flat_w + offset, p->weight.data, nw * sizeof(float));
                            memcpy(bc_flat_g + offset, p->grad.data, nw * sizeof(float));
                            offset += nw;
                        }
                        
                        /* HoloOpt step */
                        bear_holo_step_float(holo_bc, bc_flat_w, bc_flat_g, n_policy_params);
                        
                        /* Scatter updated weights back */
                        offset = 0;
                        for (int i = 0; i < pl.num_layers && offset < n_policy_params; ++i) {
                            BearParam* p = pl.layers[i].param;
                            if (!p) continue;
                            int nw = (int)bear_tensor_numel(&p->weight);
                            if (offset + nw > n_policy_params) break;
                            memcpy(p->weight.data, bc_flat_w + offset, nw * sizeof(float));
                            offset += nw;
                        }
                    } else {
                        /* Fallback: Adam via bear_ppo_apply_gradients */
                        BearOptimizer* opt_bc = bear_optimizer_create(&g_arena, BEAR_OPT_ADAM, 1e-3f);
                        bear_ppo_apply_gradients(&pl, &cr, opt_bc, opt_bc);
                    }
                }
            }
            
            printf("[BC] iter %d complete\n", bc_iter);
        }
        
        /* Cleanup BC HoloOpt */
        if (holo_bc) { /* arena freed with g_arena */ }
        if (bc_flat_w) free(bc_flat_w);
        if (bc_flat_g) free(bc_flat_g);
        
        /* Note: bear_traj_destroy not implemented - trajectory memory in r_arena */
        
        BearTrajectory traj;
        if (bear_traj_init(&traj, &r_arena, ROLLOUT_LEN, nenv, 1, od, 1, 0) != 0) return 1;

        float best = -1e9; int eps = 0;

        /* Best model tracking */
        int max_policy_params = 0;
        for (int i = 0; i < pl.num_layers; ++i) {
            if (pl.layers[i].param && pl.layers[i].param->weight.data) {
                max_policy_params += (int)bear_tensor_numel(&pl.layers[i].param->weight);
            }
        }
        float* best_policy_params = (float*)malloc(max_policy_params * sizeof(float));
        float* best_value_params = NULL;
        int max_value_params = 0;
        if (cr.layers) {
            for (int i = 0; i < cr.num_layers; ++i) {
                if (cr.layers[i].param && cr.layers[i].param->weight.data) {
                    max_value_params += (int)bear_tensor_numel(&cr.layers[i].param->weight);
                }
            }
            best_value_params = (float*)malloc(max_value_params * sizeof(float));
        }
        int best_iter = -1;

        for (int iter = 0; iter < iters; ++iter) {
            bear_arena_reset(&s_arena);

            /* Q-controller: choose LR for this iteration */
            bear_qcontroller_choose_action(&qc_policy, &qc_cfg, 3e-4f);
            bear_qcontroller_choose_action(&qc_value, &qc_cfg, 1e-3f);
            bear_optimizer_set_lr(opt_policy, qc_policy.current_lr);
            bear_optimizer_set_lr(opt_value, qc_value.current_lr);

            for (int step = 0; step < ROLLOUT_LEN; ++step) {
                for (int e = 0; e < nenv; ++e) build_obs(e, &obs[e*od], np);
                
                BearTensor obs_t, pe_enc, ve_enc;
                int64_t obs_shape[2] = {nenv, od}; 
                bear_tensor_create(&s_arena, &obs_t, obs_shape, 2, BEAR_DTYPE_F32, "obs");
                memcpy(obs_t.data, obs, nenv*od*sizeof(float));
                
                int64_t enc_shape[2] = {nenv, 128};
                bear_tensor_create(&s_arena, &pe_enc, enc_shape, 2, BEAR_DTYPE_F32, "pe");
                int64_t venc_shape[2] = {nenv, 64};
                bear_tensor_create(&s_arena, &ve_enc, venc_shape, 2, BEAR_DTYPE_F32, "ve");
                
                float* peo = (float*)pe_enc.data; float* veo = (float*)ve_enc.data;
                for (int e = 0; e < nenv; ++e) { geo_fwd(pe, &obs[e*od], peo+e*128); geo_fwd(ve, &obs[e*od], veo+e*64); }

                BearTensor ac, lp, vl, ho;
                int64_t ac_shape[2] = {nenv,1};
                bear_tensor_create(&s_arena, &ac, ac_shape, 2, BEAR_DTYPE_F32, "ac");
                int64_t lp_shape[1] = {nenv};
                bear_tensor_create(&s_arena, &lp, lp_shape, 1, BEAR_DTYPE_F32, "lp");
                bear_tensor_create(&s_arena, &vl, lp_shape, 1, BEAR_DTYPE_F32, "vl");
                int64_t ho_shape[2] = {nenv,128};
                bear_tensor_create(&s_arena, &ho, ho_shape, 2, BEAR_DTYPE_F32, "ho");
                
                bear_policy_forward(&pl, &pe_enc, 0, &ac, &lp, &vl, &ho, &s_arena);
                
                uint64_t rng[2] = {(uint64_t)rand(), (uint64_t)rand()};
                bear_policy_sample(&pl, &ac, &lp, rng);
                
                /* Safety: clamp actions to prevent NaN/Inf from destabilizing MuJoCo */
                float* f = (float*)ac.data;
                for (int i = 0; i < nenv; ++i) {
                    if (f[i] != f[i] || f[i] > 1e6f || f[i] < -1e6f) f[i] = 0.0f;  // NaN/Inf check
                    if (f[i] > 80.0f) f[i] = 80.0f;
                    if (f[i] < -80.0f) f[i] = -80.0f;
                    forces[i] = f[i];
                }
                
                chained_env_step(forces, rewards, dones);
                
                BearTensor rw_t, dn_t;
                bear_tensor_create(&s_arena, &rw_t, lp_shape, 1, BEAR_DTYPE_F32, "rw");
                bear_tensor_create(&s_arena, &dn_t, lp_shape, 1, BEAR_DTYPE_U8, "dn");
                memcpy(rw_t.data, rewards, nenv*sizeof(float));
                memcpy(dn_t.data, dones, nenv*sizeof(uint8_t));
                
                bear_traj_store(&traj, step, &obs_t, &ac, &lp, &rw_t, &dn_t, &vl);
                
                for (int i = 0; i < nenv; ++i) if (dones[i]) {
                    float ep_ret = g_env.last_ep_ret[i];
                    eps++;
                    if (ep_ret > best) {
                        best = ep_ret;
                        best_iter = iter;
                        /* Copy current policy params as best */
                        int idx = 0;
                        for (int li = 0; li < pl.num_layers; ++li) {
                            if (pl.layers[li].param && pl.layers[li].param->weight.data) {
                                int nw = (int)bear_tensor_numel(&pl.layers[li].param->weight);
                                memcpy(best_policy_params + idx, pl.layers[li].param->weight.data, nw * sizeof(float));
                                idx += nw;
                            }
                        }
                        if (best_value_params) {
                            idx = 0;
                            for (int li = 0; li < cr.num_layers; ++li) {
                                if (cr.layers[li].param && cr.layers[li].param->weight.data) {
                                    int nw = (int)bear_tensor_numel(&cr.layers[li].param->weight);
                                    memcpy(best_value_params + idx, cr.layers[li].param->weight.data, nw * sizeof(float));
                                    idx += nw;
                                }
                            }
                        }
                    }
                    printf("[ITER %d] ep=%d ret=%.1f best=%.1f\n", iter, eps, ep_ret, best);
                }
            }
            
            bear_compute_advantages(&traj, &cfg, &s_arena);
            
            for (int epoch = 0; epoch < cfg.epochs_per_iter; ++epoch) {
                bear_arena_reset(&s_arena);
                BearMinibatchSampler sampler;
                bear_sampler_init(&sampler, &traj, cfg.minibatch_size, (uint64_t[2]){rand(), rand()});
                
                BearTensor mb_obs, mb_act, mb_lp, mb_adv, mb_ret, mb_val, mb_oldlp;
                while (bear_sampler_next(&sampler, &traj, &mb_obs, &mb_act, &mb_lp, &mb_adv, &mb_ret, &mb_val, &mb_oldlp, &s_arena)) {
                    BearTensor mb_pe_enc;
                    int64_t mb_enc_shape[2] = {cfg.minibatch_size, 128};
                    bear_tensor_create(&s_arena, &mb_pe_enc, mb_enc_shape, 2, BEAR_DTYPE_F32, "pe");
                    float* mpo = (float*)mb_obs.data;
                    for (int i = 0; i < cfg.minibatch_size; ++i) geo_fwd(pe, &mpo[i*od], &((float*)mb_pe_enc.data)[i*128]);
                    
                    bear_ppo_loss(&pl, &cr, &mb_pe_enc, &mb_act, &mb_oldlp, &mb_adv, &mb_ret, &mb_val, &cfg, &s_arena);
                    bear_ppo_apply_gradients(&pl, &cr, opt_policy, opt_value);
                }
            }
            
            printf("[ITER %d] complete eps=%d best=%.1f | LR policy=%.2e value=%.2e | Q-status=%d/%d\n",
                   iter, eps, best,
                   bear_optimizer_get_lr(opt_policy), bear_optimizer_get_lr(opt_value),
                   bear_qcontroller_get_status(&qc_policy), bear_qcontroller_get_status(&qc_value));

            /* Q-controller: update with best return as metric */
            bear_qcontroller_update(&qc_policy, &qc_cfg, best);
            bear_qcontroller_update(&qc_value, &qc_cfg, best);
        }

        /* Restore best model params before saving */
        printf("[BEST] Restoring best model from iter %d (ret=%.1f)\n", best_iter, best);
        int idx = 0;
        for (int li = 0; li < pl.num_layers; ++li) {
            if (pl.layers[li].param && pl.layers[li].param->weight.data) {
                int nw = (int)bear_tensor_numel(&pl.layers[li].param->weight);
                memcpy(pl.layers[li].param->weight.data, best_policy_params + idx, nw * sizeof(float));
                idx += nw;
            }
        }
        if (best_value_params) {
            idx = 0;
            for (int li = 0; li < cr.num_layers; ++li) {
                if (cr.layers[li].param && cr.layers[li].param->weight.data) {
                    int nw = (int)bear_tensor_numel(&cr.layers[li].param->weight);
                    memcpy(cr.layers[li].param->weight.data, best_value_params + idx, nw * sizeof(float));
                    idx += nw;
                }
            }
        }

        /* Save policy and value for this curriculum stage - SAVE BEST MODEL */
        {
            char policy_path[256], value_path[256];
            snprintf(policy_path, sizeof(policy_path), "policy_%dpole.bear", np);
            snprintf(value_path, sizeof(value_path), "value_%dpole.bear", np);
            if (bear_policy_save(&pl, policy_path) == 0) {
                printf("[SAVE] Policy saved to %s\n", policy_path);
            }
            if (bear_value_save(&cr, value_path) == 0) {
                printf("[SAVE] Value saved to %s\n", value_path);
            }
        }

        /* Free best model buffers */
        free(best_policy_params);
        if (best_value_params) free(best_value_params);

        free(obs); free(forces); free(rewards); free(dones);
    }

    bear_arena_destroy(&s_arena); bear_arena_destroy(&r_arena); bear_arena_destroy(&g_arena);
    chained_env_close();
    return 0;
}