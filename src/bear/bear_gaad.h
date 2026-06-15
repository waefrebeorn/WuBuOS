/*
 * bear_gaad.h  --  WuBu GAAD Optimizer
 *
 * Gradient-Adaptive Adaptive Descent (GAAD) for BearRL.
 * Based on WuBu Nesting Theory (bytropix/THEORY/):
 * - Log(g) complexity-dependent scaling
 * - Anisotropic gradient processing (band nesting analogy)
 * - Resonant gradient amplification
 * - Poincare ball curvature-aware updates
 * - Meta-control Q-learner for adaptive strain engineering
 */

#ifndef BEAR_GAAD_H
#define BEAR_GAAD_H

#include "bear_arena.h"
#include "bear_opt.h"
#include "wubu_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * GAAD Configuration
 * =================================================================== */

typedef struct {
    /* Base optimizer config */
    float base_lr;
    float beta1;
    float beta2;
    float eps;
    float weight_decay;
    
    /* GAAD-specific: complexity-dependent scaling */
    int   use_log_g_scaling;        /* 1 = enable log(g) LR scaling */
    float log_g_factor;             /* multiplier for log(g) term */
    int   model_complexity;         /* total params / 1e6 (approx "genus") */
    
    /* Anisotropic processing (band nesting) */
    int   use_anisotropic;          /* 1 = enable per-dimension LR */
    int   anis_dim;                 /* dimension for anisotropy */
    float* anis_scales;             /* per-dimension scale factors [anis_dim] */
    float anis_momentum;            /* momentum for anis scale updates */
    float anis_min_scale;           /* minimum anis scale */
    float anis_max_scale;           /* maximum anis scale */
    
    /* Resonant amplification */
    int   use_resonant;             /* 1 = enable resonant amplification */
    float resonant_threshold;       /* gradient variance threshold for resonance */
    float resonant_boost;           /* amplification factor when resonant */
    int   resonant_window;          /* window for resonance detection */
    float* grad_variance_history;   /* circular buffer [resonant_window * anis_dim] */
    int   resonant_pos;             /* current position in history */
    
    /* Poincare ball curvature-aware updates */
    int   use_poincare;             /* 1 = use hyperbolic updates */
    float curvature;                /* Poincare ball curvature c */
    float poincare_scale;           /* scale factor for tangent space */
    
    /* Meta-control / Q-learner */
    int   use_q_controller;         /* 1 = enable Q-controller */
    float q_lr;                     /* Q-learning rate */
    float q_gamma;                  /* discount factor */
    float q_epsilon;                /* exploration rate */
    float q_epsilon_decay;          /* epsilon decay per step */
    float q_epsilon_min;            /* minimum epsilon */
    int   q_state_dim;              /* state dimension */
    float* q_table;                 /* Q-table [q_state_dim * q_action_dim] */
    int   q_action_dim;             /* number of discrete actions */
    int   q_last_state;             /* last observed state */
    int   q_last_action;            /* last taken action */
    
    /* Strain engineering hooks */
    int   strain_hook_id;           /* identifier for strain hooks */
    void (*on_strain_trigger)(int hook_id, const char* reason, float severity);
    void* strain_user_data;
    
} BearGAADConfig;

static inline BearGAADConfig bear_gaad_default_config(void) {
    return (BearGAADConfig){
        .base_lr = 3e-4f,
        .beta1 = 0.9f,
        .beta2 = 0.999f,
        .eps = 1e-8f,
        .weight_decay = 0.01f,
        
        .use_log_g_scaling = 1,
        .log_g_factor = 0.1f,
        .model_complexity = 1,
        
        .use_anisotropic = 1,
        .anis_dim = 0,              /* auto = full gradient dim */
        .anis_scales = NULL,
        .anis_momentum = 0.9f,
        .anis_min_scale = 0.1f,
        .anis_max_scale = 10.0f,
        
        .use_resonant = 1,
        .resonant_threshold = 1.0f,
        .resonant_boost = 2.0f,
        .resonant_window = 100,
        .grad_variance_history = NULL,
        .resonant_pos = 0,
        
        .use_poincare = 1,
        .curvature = 1.0f,
        .poincare_scale = 1.0f,
        
        .use_q_controller = 0,
        .q_lr = 0.1f,
        .q_gamma = 0.99f,
        .q_epsilon = 0.1f,
        .q_epsilon_decay = 0.999f,
        .q_epsilon_min = 0.01f,
        .q_state_dim = 0,
        .q_table = NULL,
        .q_action_dim = 0,
        .q_last_state = -1,
        .q_last_action = -1,
        
        .strain_hook_id = 0,
        .on_strain_trigger = NULL,
        .strain_user_data = NULL,
    };
}

/* ===================================================================
 * GAAD Optimizer State
 * =================================================================== */

typedef struct {
    BearGAADConfig config;
    
    /* Adam state */
    float* mom;      /* first moment */
    float* var;      /* second moment */
    int step;        /* global step counter */
    
    /* TGT state - gradient decomposition */
    float* quotients;    /* floor((g + π) / 2π) */
    float* remainders;   /* mod(g + π, 2π) - π */
    float* raw_grads;    /* original gradients */
    
    /* Anisotropic state (GAAD tiling) */
    float* anis_ema;   /* EMA of per-dimension gradients */
    float* anis_sq;    /* EMA of squared gradients */
    float* anis_scales; /* per-dimension learning rate scales */
    
    /* Resonant state */
    float* res_hist;  /* gradient variance history */
    int res_idx;      /* current index in history */
    
    /* Poincare state */
    float* tangent_grad; /* gradient in tangent space */
    float* poincare_pos; /* current position in Poincare ball */
    
    /* Q-controller state */
    int q_step;
    int q_last_state;
    int q_last_action;
    
    /* Capacity */
    int capacity;
    int initialized;
    
} BearGAADOptimizer;

/* Create GAAD optimizer */
BearGAADOptimizer* bear_gaad_create(BearArena* arena, const BearGAADConfig* cfg, int param_count);

/* Destroy GAAD optimizer */
void bear_gaad_destroy(BearGAADOptimizer* opt);

/* GAAD step: applies update to params */
void bear_gaad_step(
    BearGAADOptimizer* opt,
    float* params,
    const float* grads,
    int param_count,
    BearArena* temp_arena
);

/* Set learning rate */
void bear_gaad_set_lr(BearGAADOptimizer* opt, float lr);

/* Get current learning rate */
float bear_gaad_get_lr(const BearGAADOptimizer* opt);

/* Trigger strain engineering hook */
void bear_gaad_trigger_strain(BearGAADOptimizer* opt, const char* reason, float severity);

/* ===================================================================
 * GAAD + Q-Controller for Adaptive Strain Engineering
 * ================================================================== */

typedef struct {
    float loss_recon;
    float loss_adv;
    float loss_vf;
    float entropy;
    float approx_kl;
    float clip_frac;
    float reward;
} BearGAADMetrics;

/* Update Q-controller with current metrics */
void bear_gaad_q_update(
    BearGAADOptimizer* opt,
    const BearGAADMetrics* metrics,
    BearArena* temp_arena
);

/* Get current strain level (0-1) based on Q-values */
float bear_gaad_strain_level(const BearGAADOptimizer* opt);

/* ==================================================================
 * Utility: Gradient statistics
 * ================================================================== */

typedef struct {
    float mean;
    float std;
    float max;
    float min;
    float variance;
    float skewness;
    float kurtosis;
    float spectral_norm;  /* max singular value estimate */
} BearGAADGradStats;

/* Compute gradient statistics */
BearGAADGradStats bear_gaad_grad_stats(const float* grad, int n, BearArena* temp_arena);

/* Check if gradients are resonant */
int bear_gaad_is_resonant(const BearGAADOptimizer* opt, const float* grad, int n);

#ifdef __cplusplus
}
#endif

#endif /* BEAR_GAAD_H */