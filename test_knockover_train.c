/*
 * test_knockover_train.c — Minimal PPO training with knock-over reset
 * Uses the working BearRL API
 */
#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_arena.h"
#include "src/bear/bear_nn.h"
#include "src/bear/bear_ppo.h"
#include "src/bear/bear_opt.h"
#include "src/bear/bear_mujoco.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <alloca.h>

#define MAX_POLES 20
#define MAX_ENVS 32
#define ROLLOUT_LEN 1024
#define MAX_STEPS 750

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ── Chained Env (knock-over reset) ─────────────────────────── */
typedef struct {
    int nenv, npoles;
    float* cart_x; float* cart_vx;
    float* theta_abs; float* omega_abs;
    float* theta_rel_in; float* omega_rel_in;
    float* theta_rel_out; float* omega_rel_out;
    float* ep_step; float* ep_ret;
    float* upright_time;
    float ep_len, force_mag, angle_thresh, cart_thresh;
} ChainedEnv;

static ChainedEnv g_env;
#define ATH(e,p) g_env.theta_abs[(e)*g_env.npoles+(p)]
#define AOM(e,p) g_env.omega_abs[(e)*g_env.npoles+(p)]

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

static void chained_env_step(const float* forces, float* rewards, uint8_t* dones);

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
    g_env.ep_step = calloc(nenv, sizeof(float));
    g_env.ep_ret = calloc(nenv, sizeof(float));
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
    free(g_env.upright_time);
    memset(&g_env, 0, sizeof(g_env));
}

/* Knock-over reset: start upright -> kick -> fall -> noise */
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
    
    g_env.ep_step[env_id] = 0; g_env.ep_ret[env_id] = 0;
    g_env.upright_time[env_id] = 0;
    
    abs_to_rel(&ATH(env_id,0), &g_env.theta_rel_in[env_id*N], N);
    g_env.omega_rel_in[env_id*N] = AOM(env_id,0);
    for (int p = 1; p < N; ++p)
        g_env.omega_rel_in[env_id*N+p] = AOM(env_id,p) - AOM(env_id,p-1);
    
    bear_mujoco_reset_env(env_id, N, g_env.cart_x[env_id], g_env.cart_vx[env_id],
        &g_env.theta_rel_in[env_id*N], &g_env.omega_rel_in[env_id*N]);
    
    printf("[RESET] env=%d fallen_theta1=%.3f (%.1f°) kick=%.1f steps=%d\n",
           env_id, ATH(env_id,0), ATH(env_id,0)*180/M_PI, force, fall);
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
            float th = ATH(i,p);
            float d = fabsf(norm_angle(th));
            upright_sum += cosf(d);
            progress_sum += (3.14159265f - d) / 3.14159265f;
        }
        float upright = (upright_sum/N + 1.0f) * 0.5f;
        float progress = progress_sum/N;
        
        if (upright > 0.9f) g_env.upright_time[i] += 1.0f;
        else g_env.upright_time[i] = 0.0f;
        
        float cp = fabsf(g_env.cart_x[i]);
        float centered = (cp > 2.0f) ? fmaxf(0, 1-(cp-2)/(g_env.cart_thresh-2)) : 1.0f;
        centered = (1.0f + centered) * 0.5f;
        
        float vp = 0;
        for (int p = 0; p < N; ++p) {
            float w = fabsf(AOM(i,p));
            if (w > 5.0f) vp += (w - 5.0f) * 0.01f;
        }
        float sv = 1.0f / (1.0f + vp);
        
        float af = fabsf(forces[i]);
        float sc = (af > 1.0f) ? fmaxf(0, 1-(af-1)/(g_env.force_mag-1)) : 1.0f;
        sc = (4.0f + sc) / 5.0f;
        
        float r = (progress*0.05f + upright*0.5f + sqrtf(g_env.upright_time[i])*0.01f*upright) * centered * sv * sc;
        rewards[i] = isnan(r)||isinf(r) ? 0.0f : r;
        g_env.ep_ret[i] += rewards[i];
        g_env.ep_step[i] += 1;
        
        int done = 0;
        for (int p = 0; p < N && !done; ++p)
            if (fabsf(norm_angle(ATH(i,p))) > 1.5708f) done = 1;
        if (fabsf(g_env.cart_x[i]) > g_env.cart_thresh) done = 1;
        if (g_env.ep_step[i] >= g_env.ep_len) done = 1;
        dones[i] = done;
        if (done) chained_env_reset(i);
    }
}

/* Build obs: [x, vx, sinθ1, cosθ1, ω1, ...] */
void build_obs(int e, float* o, int np) {
    o[0]=g_env.cart_x[e]; o[1]=g_env.cart_vx[e];
    for(int p=0;p<np;++p){o[2+4*p]=sinf(ATH(e,p));o[3+4*p]=cosf(ATH(e,p));o[4+4*p]=AOM(e,p);}
}

/* Geometric Encoder */
typedef struct { int num_layers; int* ls; float* w; float* b; int in_dim, out_dim; } GeoEnc;
GeoEnc* geo_create(BearArena* a, int id, int od, int nl) {
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
void geo_fwd(const GeoEnc* g, const float* in, float* out) {
    int mx=0;for(int i=0;i<=g->num_layers;++i)if(g->ls[i]>mx)mx=g->ls[i];
    float* p=(float*)alloca(mx*sizeof(float));float* c=(float*)alloca(mx*sizeof(float));
    memcpy(p,in,g->in_dim*sizeof(float));int wi=0,bi=0;
    for(int L=0;L<g->num_layers;++L){int id=g->ls[L],od=g->ls[L+1];
        for(int j=0;j<od;++j){float s=g->b[bi++];for(int k=0;k<id;++k)s+=p[k]*g->w[wi++];float gg=0.5f*s*(1+tanhf(0.79788456f*(s+0.044715f*s*s*s)));c[j]=gg*((L%2)?0.618f:1.618f);}
        memcpy(p,c,od*sizeof(float));}
    memcpy(out,p,g->out_dim*sizeof(float));
}

/* Main: Curriculum training with PPO */
int main(int argc, char** argv) {
    int nenv = 16, max_poles = 10, iters = 80, seed = (int)time(NULL);
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
    GeoEnc *pe = NULL, *ve = NULL;
    int first = 1;

    for (int np = 1; np <= max_poles; ++np) {
        printf("\n════════════════════════════════════════\n");
        printf(" CURRICULUM: %d POLES (%d iters)\n", np, iters);
        printf("════════════════════════════════════════\n");

        int od = 2 + 4*np;

        if (first) {
            if (bear_arena_create(&g_arena, 256*1024*1024) ||
                bear_arena_create(&r_arena, 64*1024*1024) ||
                bear_arena_create(&s_arena, 16*1024*1024)) return 1;
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

        BearOptimizer opt_policy, opt_value;
        bear_opt_create(&g_arena, &opt_policy, BEAR_OPT_ADAM, 3e-4f);
        bear_opt_create(&g_arena, &opt_value, BEAR_OPT_ADAM, 1e-3f);

        BearTrajectory traj;
        if (bear_traj_init(&traj, &r_arena, ROLLOUT_LEN, nenv, 1, od, 1, 0) != 0) return 1;

        float* obs = malloc(nenv * od * sizeof(float));
        float* forces = malloc(nenv * sizeof(float));
        float* rewards = malloc(nenv * sizeof(float));
        uint8_t* dones = malloc(nenv * sizeof(uint8_t));
        float best = -1e9, total_eps = 0;

        for (int iter = 0; iter < iters; ++iter) {
            bear_arena_reset(&s_arena);
            
            /* Rollout */
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

                float* f = (float*)ac.data;
                for (int i = 0; i < nenv; ++i) forces[i] = f[i];
                
                chained_env_step(forces, rewards, dones);
                
                BearTensor rw_t, dn_t;
                bear_tensor_create(&s_arena, &rw_t, lp_shape, 1, BEAR_DTYPE_F32, "rw");
                bear_tensor_create(&s_arena, &dn_t, lp_shape, 1, BEAR_DTYPE_U8, "dn");
                memcpy(rw_t.data, rewards, nenv*sizeof(float));
                memcpy(dn_t.data, dones, nenv*sizeof(uint8_t));
                
                bear_traj_store(&traj, step, &obs_t, &ac, &lp, &rw_t, &dn_t, &vl);
                
                for (int i = 0; i < nenv; ++i) if (dones[i]) {
                    float ep_ret = g_env.ep_ret[i];
                    total_eps++;
                    if (ep_ret > best) best = ep_ret;
                    printf("[ITER %d] ep=%d ret=%.1f best=%.1f\n", iter, (int)total_eps, ep_ret, best);
                }
            }
            
            bear_compute_advantages(&traj, &cfg, &s_arena);
            
            for (int epoch = 0; epoch < cfg.epochs_per_iter; ++epoch) {
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
                    bear_ppo_apply_gradients(&pl, &cr, &opt_policy, &opt_value);
                }
            }
        }

        bear_traj_destroy(&traj);
        free(obs); free(forces); free(rewards); free(dones);
    }

    bear_arena_destroy(&s_arena); bear_arena_destroy(&r_arena); bear_arena_destroy(&g_arena);
    chained_env_close();
    return 0;
}