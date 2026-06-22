/*
 * bear_gaad.c  --  WuBu GAAD Optimizer (GAAD + TGT + QLearner)
 *
 * Unified optimizer combining:
 * - GAAD: Golden Aspect Adaptive Decomposition (φ-tiling)
 * - TGT: Toroidal Gradient Transformation (mod 2π decomposition)
 * - QLearner: Adaptive strain engineering (HAKMEMQController)
 * - WuBu Math: Poincare ball / hyperbolic geometry
 */

#include "bear_gaad.h"
#include "bear_arena.h"
#include "wubu_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ===================================================================
 * Internal State
 * =================================================================== */

struct BearGAADOptimizer {
    BearGAADConfig config;
    
    /* Adam state */
    float* mom;           /* 1st moment */
    float* var;           /* 2nd moment */
    int step;             /* global step */
    
    /* TGT state - gradient decomposition */
    float* quotients;     /* floor((g + π) / 2π) */
    float* remainders;    /* mod(g + π, 2π) - π */
    float* raw_grads;     /* original gradients */
    
    /* Anisotropic state (GAAD tiling) */
    float* anis_ema;      /* EMA of per-dimension gradients */
    float* anis_sq;       /* EMA of squared gradients */
    float* anis_scales;   /* per-dimension learning rate scales */
    
    /* Resonant state */
    float* res_hist;      /* gradient variance history */
    int res_idx;          /* history index */
    
    /* Poincare state */
    float* tangent_grad;  /* gradient in tangent space */
    float* poincare_pos;  /* current position in Poincare ball */
    
    /* Q-controller state */
    int q_step;
    int q_last_state;
    int q_last_action;
    
    /* Capacity */
    int capacity;
    int initialized;
};

/* ===================================================================
 * Constants
 * =================================================================== */

#define GAAD_PI           3.14159265358979323846f
#define GAAD_2PI          6.28318530717958647692f
#define GAAD_PHI          1.61803398874989484820f
#define GAAD_INV_PHI      0.61803398874989484820f  /* 1/φ = φ-1 */
#define GAAD_EPS          1e-7f

/* ===================================================================
 * Utility Functions
 * =================================================================== */

static inline float gaad_clip(float x, float lo, float hi) {
    return fminf(fmaxf(x, lo), hi);
}

static inline float gaad_phi_pow(float x) {
    return powf(GAAD_PHI, x);
}

static inline float gaad_norm(const float* v, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += (double)v[i] * v[i];
    return (float)sqrt(sum + GAAD_EPS);
}

/* ===================================================================
 * TGT: Toroidal Gradient Transformation
 * =================================================================== */

/*
 * Decompose gradient g into quotient and remainder:
 *   g = (2π) * quotient + remainder
 * where remainder ∈ [-π, π]
 */
static void gaad_tgt_decompose(const float* grad, float* quotient, float* remainder, int n) {
    for (int i = 0; i < n; ++i) {
        float g = grad[i];
        /* Modulo 2π: floor((g + π) / 2π) */
        float twopi = GAAD_2PI;
        float q = floorf((g + GAAD_PI) / twopi);
        float r = g - q * twopi;
        if (r < -GAAD_PI) r += GAAD_2PI;
        if (r > GAAD_PI) r -= GAAD_2PI;
        quotient[i] = q;
        remainder[i] = r;
    }
}

/*
 * Reconstruct gradient from TGT components
 */
static void gaad_tgt_reconstruct(const float* quotient, const float* remainder, float* grad, int n) {
    for (int i = 0; i < n; ++i) {
        grad[i] = quotient[i] * GAAD_2PI + remainder[i];
    }
}

/* ===================================================================
 * GAAD: Golden Aspect Adaptive Decomposition (φ-tiling)
 * =================================================================== */

/*
 * Initialize anisotropic scales using golden ratio tiling
 * Divides parameter space into φ-proportional regions
 */
static void gaad_init_anisotropic(BearGAADOptimizer* opt, int n) {
    if (!opt->config.use_anisotropic) return;
    
    int d = opt->config.anis_dim > 0 ? opt->config.anis_dim : n;
    
    opt->anis_scales = (float*)calloc(d, sizeof(float));
    opt->anis_ema = (float*)calloc(d, sizeof(float));
    opt->anis_sq = (float*)calloc(d, sizeof(float));
    
    if (!opt->anis_scales || !opt->anis_ema || !opt->anis_sq) return;
    
    /* Initialize with φ-proportional scales */
    for (int i = 0; i < d; ++i) {
        float phi_factor = gaad_phi_pow((float)i / d - 0.5f);
        opt->anis_scales[i] = gaad_clip(phi_factor, opt->config.anis_min_scale, opt->config.anis_max_scale);
        opt->anis_ema[i] = 0.0f;
        opt->anis_sq[i] = 0.0f;
    }
}

/*
 * Update anisotropic scales based on gradient statistics
 * Uses φ-weighted EMA for per-dimension adaptation
 */
static void gaad_update_anisotropic(BearGAADOptimizer* opt, const float* grad, int n) {
    if (!opt->config.use_anisotropic || !opt->anis_scales) return;
    
    int d = opt->config.anis_dim > 0 ? opt->config.anis_dim : n;
    float beta = opt->config.anis_momentum;
    
    for (int i = 0; i < n && i < d; ++i) {
        /* EMA of gradient and squared gradient */
        opt->anis_ema[i] = beta * opt->anis_ema[i] + (1.0f - beta) * fabsf(grad[i]);
        opt->anis_sq[i] = beta * opt->anis_sq[i] + (1.0f - beta) * grad[i] * grad[i];
        
        /* Update scale: proportional to 1/sqrt(ema_sq + ε) with φ-weighting */
        float scale = 1.0f / (sqrtf(opt->anis_sq[i] + 1e-8f) + 1e-4f);
        
        /* Apply φ-weighting for resonance detection */
        float phi_weight = gaad_phi_pow(opt->anis_ema[i] * 10.0f);
        scale *= phi_weight;
        
        opt->anis_scales[i] = gaad_clip(scale, opt->config.anis_min_scale, opt->config.anis_max_scale);
    }
}

/* ===================================================================
 * Poincare Ball Operations
 * =================================================================== */

static void gaad_poincare_project(float* x, int n, float c) {
    if (c <= 0.0f) return;
    float norm = gaad_norm(x, n);
    float max_norm = (1.0f / sqrtf(fmaxf(c, GAAD_EPS))) * (1.0f - GAAD_EPS);
    if (norm > max_norm && norm > 0.0f) {
        float scale = max_norm / norm;
        for (int i = 0; i < n; ++i) x[i] *= scale;
    }
}

static void gaad_exp_map(const float* v, float* y, int n, float c, float scale) {
    if (c <= 0.0f) {
        for (int i = 0; i < n; ++i) y[i] = v[i];
        return;
    }
    float norm = gaad_norm(v, n);
    if (norm < GAAD_EPS) {
        for (int i = 0; i < n; ++i) y[i] = 0.0f;
        return;
    }
    float sqrt_c = sqrtf(fmaxf(c, GAAD_EPS));
    float scaled_radius = scale * sqrt_c * norm;
    float tanh_term = tanhf(scaled_radius);
    float lambda = tanh_term / (sqrt_c * norm + GAAD_EPS);
    for (int i = 0; i < n; ++i) y[i] = lambda * v[i];
    gaad_poincare_project(y, n, c);
}

static void gaad_log_map(const float* y_in, float* v, int n, float c, float scale) {
    if (c <= 0.0f) {
        for (int i = 0; i < n; ++i) v[i] = y_in[i];
        return;
    }
    
    float* y = (float*)malloc(n * sizeof(float));
    for (int i = 0; i < n; ++i) y[i] = y_in[i];
    gaad_poincare_project(y, n, c);
    
    float norm = gaad_norm(y, n);
    if (norm < GAAD_EPS) {
        for (int i = 0; i < n; ++i) v[i] = 0.0f;
        free(y);
        return;
    }
    
    float sqrt_c = sqrtf(fmaxf(c, GAAD_EPS));
    float arctanh_input = fmaxf(-1.0f + GAAD_EPS, fminf(1.0f - GAAD_EPS, sqrt_c * norm));
    float atanh_term = atanhf(arctanh_input) / scale;
    float lambda = atanh_term / (sqrt_c * norm + GAAD_EPS);
    for (int i = 0; i < n; ++i) v[i] = lambda * y[i];
    
    free(y);
}

static void gaad_riemannian_grad(const float* euclid_grad, const float* param, float* riemannian_grad, int n, float c) {
    if (c <= 0.0f) {
        for (int i = 0; i < n; ++i) riemannian_grad[i] = euclid_grad[i];
        return;
    }
    
    float norm_sq = 0.0f;
    for (int i = 0; i < n; ++i) norm_sq += param[i] * param[i];
    float max_norm = 1.0f / sqrtf(fmaxf(c, GAAD_EPS));
    norm_sq = fminf(norm_sq, max_norm * max_norm * 0.999f);
    
    float lambda = (1.0f - c * norm_sq) / 2.0f;
    float factor = lambda * lambda;
    
    for (int i = 0; i < n; ++i) {
        riemannian_grad[i] = euclid_grad[i] * factor;
    }
}

/* ===================================================================
 * Resonant Detection
 * =================================================================== */

static void gaad_update_resonant(BearGAADOptimizer* opt, const float* grad, int n) {
    if (!opt->config.use_resonant) return;
    
    int w = opt->config.resonant_window;
    float* hist = opt->res_hist;
    int idx = opt->res_idx;
    
    /* Compute gradient variance over window */
    float mean = 0.0f, var = 0.0f;
    int count = 0;
    
    for (int j = 0; j < w; ++j) {
        int h_idx = (idx - j + w) % w;
        float* window_grad = &hist[h_idx * opt->capacity];
        if (window_grad[0] != -1e30f) {  /* Check if valid */
            float norm = gaad_norm(window_grad, n);
            mean += norm;
            var += norm * norm;
            count++;
        }
    }
    
    if (count > 2) {
        mean /= count;
        var = var / count - mean * mean;
        if (var > opt->config.resonant_threshold * mean * mean) {
            /* Resonant: amplify - would be applied to learning rate or step */
            (void)opt->config.resonant_boost;  /* Suppress unused warning */
        }
    }
    
    /* Store current gradient in history */
    for (int i = 0; i < n; ++i) {
        hist[idx * opt->capacity + i] = grad[i];
    }
    opt->res_idx = (idx + 1) % w;
}

/* ===================================================================
 * Log(g) Complexity Scaling
 * =================================================================== */

static float gaad_complexity_scale(int model_complexity, float factor) {
    if (model_complexity <= 0) return 1.0f;
    return 1.0f + factor * logf(1.0f + model_complexity / 1e6f);
}

/* ===================================================================
 * Q-Learner: Adaptive Strain Engineering
 * =================================================================== */

typedef struct {
    int state_dim;
    int action_dim;
    float* q_table;
    float lr;
    float gamma;
    float epsilon;
    float epsilon_decay;
    float epsilon_min;
    int last_state;
    int last_action;
    float reward_history[100];
    int reward_idx;
} GAADQController;

static int gaad_q_get_state(const float* metrics, int n, int state_dim) {
    /* Simple state encoding from metrics */
    int state = 0;
    for (int i = 0; i < n && i < state_dim; ++i) {
        float v = fmaxf(0.0f, fminf(1.0f, metrics[i] * 10.0f));
        state = state * 10 + (int)v;
    }
    return state % 1000;  /* Max 1000 states */
}

static int gaad_q_choose_action(GAADQController* qc, int state) {
    if ((float)rand() / RAND_MAX < qc->epsilon) {
        return rand() % qc->action_dim;
    }
    
    /* Exploit: choose best action for this state */
    int best_action = 0;
    float best_q = -1e30f;
    for (int a = 0; a < qc->action_dim; ++a) {
        int idx = state * qc->action_dim + a;
        if (qc->q_table[idx] > best_q) {
            best_q = qc->q_table[idx];
            best_action = a;
        }
    }
    return best_action;
}

static void gaad_q_update(GAADQController* qc, int state, int action, float reward, int next_state) {
    (void)action;  /* Suppress unused warning */
    int idx = state * qc->action_dim + qc->action_dim;
    float max_next = -1e30f;
    for (int a = 0; a < qc->action_dim; ++a) {
        int nidx = next_state * qc->action_dim + a;
        if (qc->q_table[nidx] > max_next) max_next = qc->q_table[nidx];
    }
    
    qc->q_table[idx] += qc->lr * (reward + qc->gamma * max_next - qc->q_table[idx]);
    qc->epsilon = fmaxf(qc->epsilon_min, qc->epsilon * qc->epsilon_decay);
}

/* ===================================================================
 * Strain Engineering Hooks
 * =================================================================== */

static void gaad_trigger_strain_hook(BearGAADOptimizer* opt, const char* reason, float severity) {
    if (opt->config.on_strain_trigger) {
        opt->config.on_strain_trigger(opt->config.strain_hook_id, reason, severity);
    }
}

static void gaad_check_strain_conditions(BearGAADOptimizer* opt, const float* grad, int n, float loss) {
    float grad_norm = gaad_norm(grad, n);
    
    /* Gradient explosion */
    if (grad_norm > 1e3f) {
        gaad_trigger_strain_hook(opt, "gradient_explosion", grad_norm / 1e3f);
    }
    
    /* Loss divergence */
    if (!isfinite(loss) || loss > 1e6f) {
        gaad_trigger_strain_hook(opt, "loss_divergence", loss / 1e6f);
    }
    
    /* Vanishing gradients */
    if (grad_norm < 1e-6f) {
        gaad_trigger_strain_hook(opt, "vanishing_gradients", 1e-6f / grad_norm);
    }
}

/* ===================================================================
 * Public API
 * =================================================================== */

BearGAADOptimizer* bear_gaad_create(BearArena* arena, const BearGAADConfig* cfg, int param_count) {
    if (!arena || !cfg || param_count <= 0) return NULL;
    
    BearGAADOptimizer* opt = (BearGAADOptimizer*)BEAR_ARENA_ALLOC(arena, BearGAADOptimizer, 1);
    if (!opt) return NULL;
    
    BearGAADConfig config = *cfg;
    if (config.anis_dim <= 0) config.anis_dim = param_count;
    
    opt->config = config;
    opt->capacity = param_count;
    opt->initialized = 1;
    opt->step = 0;
    opt->res_idx = 0;
    opt->q_step = 0;
    opt->q_last_state = -1;
    opt->q_last_action = -1;
    
    /* Allocate Adam state */
    opt->mom = (float*)BEAR_ARENA_ALLOC(arena, float, param_count);
    opt->var = (float*)BEAR_ARENA_ALLOC(arena, float, param_count);
    
    /* TGT state */
    opt->quotients = (float*)BEAR_ARENA_ALLOC(arena, float, param_count);
    opt->remainders = (float*)BEAR_ARENA_ALLOC(arena, float, param_count);
    opt->raw_grads = (float*)BEAR_ARENA_ALLOC(arena, float, param_count);
    
    /* Poincare state */
    opt->tangent_grad = (float*)BEAR_ARENA_ALLOC(arena, float, param_count);
    opt->poincare_pos = (float*)BEAR_ARENA_ALLOC(arena, float, param_count);
    
    /* Resonant history */
    if (config.use_resonant && config.resonant_window > 0) {
        opt->res_hist = (float*)BEAR_ARENA_ALLOC(arena, float, config.resonant_window * param_count);
        if (opt->res_hist) {
            /* Initialize with sentinel */
            for (int i = 0; i < config.resonant_window * param_count; ++i) {
                opt->res_hist[i] = -1e30f;
            }
        }
    }
    
    /* Initialize Adam state to zero */
    if (opt->mom) memset(opt->mom, 0, param_count * sizeof(float));
    if (opt->var) memset(opt->var, 0, param_count * sizeof(float));
    if (opt->quotients) memset(opt->quotients, 0, param_count * sizeof(float));
    if (opt->remainders) memset(opt->remainders, 0, param_count * sizeof(float));
    if (opt->raw_grads) memset(opt->raw_grads, 0, param_count * sizeof(float));
    if (opt->tangent_grad) memset(opt->tangent_grad, 0, param_count * sizeof(float));
    if (opt->poincare_pos) memset(opt->poincare_pos, 0, param_count * sizeof(float));
    
    /* Initialize Poincare position (all zeros = origin) */
    if (config.use_poincare) {
        gaad_poincare_project(opt->poincare_pos, param_count, config.curvature);
    }
    
    /* Initialize anisotropic scales */
    gaad_init_anisotropic(opt, param_count);
    
    return opt;
}

void bear_gaad_destroy(BearGAADOptimizer* opt) {
    if (!opt) return;
    if (opt->anis_scales) free(opt->anis_scales);
    if (opt->anis_ema) free(opt->anis_ema);
    if (opt->anis_sq) free(opt->anis_sq);
    if (opt->res_hist) free(opt->res_hist);
    opt->initialized = 0;
}

void bear_gaad_step(
    BearGAADOptimizer* opt,
    float* params,
    const float* grads,
    int param_count,
    BearArena* temp_arena
) {
    (void)temp_arena;  /* Suppress unused warning */
    if (!opt || !opt->initialized || !params || !grads || param_count != opt->capacity) return;
    
    BearGAADConfig* cfg = &opt->config;
    
    /* Copy gradients */
    if (opt->raw_grads) memcpy(opt->raw_grads, grads, param_count * sizeof(float));
    
    /* HARD GRADIENT CLIPPING - prevent NaN explosion */
    const float MAX_GRAD_NORM = 1.0f;  /* clip at 1.0 */
    float grad_norm = 0.0f;
    for (int i = 0; i < param_count; ++i) {
        float g = opt->raw_grads[i];
        if (!isfinite(g)) { opt->raw_grads[i] = 0.0f; g = 0.0f; }
        grad_norm += g * g;
    }
    grad_norm = sqrtf(grad_norm);
    if (grad_norm > MAX_GRAD_NORM && grad_norm > 0.0f) {
        float scale = MAX_GRAD_NORM / grad_norm;
        for (int i = 0; i < param_count; ++i) {
            opt->raw_grads[i] *= scale;
        }
    }
    
    /* TGT Decomposition - use clipped gradients */
    gaad_tgt_decompose(opt->raw_grads, opt->quotients, opt->remainders, param_count);
    
    /* Update anisotropic scales */
    gaad_update_anisotropic(opt, opt->remainders, param_count);
    
    /* Update resonant detection */
    gaad_update_resonant(opt, grads, param_count);
    
    /* Complexity scaling */
    float complexity_scale = 1.0f;
    if (cfg->use_log_g_scaling) {
        complexity_scale = gaad_complexity_scale(cfg->model_complexity, cfg->log_g_factor);
    }
    
    /* Poincare ball operations */
    if (cfg->use_poincare) {
        /* Convert Euclidean gradient to Riemannian gradient */
        gaad_riemannian_grad(grads, params, opt->tangent_grad, param_count, cfg->curvature);
    } else {
        memcpy(opt->tangent_grad, opt->remainders, param_count * sizeof(float));
    }
    
    /* HARD CLIP AFTER POINCARÉ/ANISOTROPIC - these can amplify gradients */
    float tg_norm = 0.0f;
    for (int i = 0; i < param_count; ++i) {
        float g = opt->tangent_grad[i];
        if (!isfinite(g)) { opt->tangent_grad[i] = 0.0f; g = 0.0f; }
        tg_norm += g * g;
    }
    tg_norm = sqrtf(tg_norm);
    if (tg_norm > MAX_GRAD_NORM && tg_norm > 0.0f) {
        float scale = MAX_GRAD_NORM / tg_norm;
        for (int i = 0; i < param_count; ++i) opt->tangent_grad[i] *= scale;
    }
    
    /* Apply anisotropic scaling */
    if (cfg->use_anisotropic && opt->anis_scales) {
        int d = cfg->anis_dim > 0 ? cfg->anis_dim : param_count;
        for (int i = 0; i < param_count && i < d; ++i) {
            opt->tangent_grad[i] *= opt->anis_scales[i];
        }
    }
    
    /* Adam update with all modifications */
    float lr = cfg->base_lr * complexity_scale;
    float beta1 = cfg->beta1;
    float beta2 = cfg->beta2;
    float eps = cfg->eps;
    float wd = cfg->weight_decay;
    int step = ++opt->step;
    
    float bc1 = 1.0f - powf(beta1, step);
    float bc2 = 1.0f - powf(beta2, step);
    
    for (int i = 0; i < param_count; ++i) {
        float grad = opt->tangent_grad[i];
        float m = beta1 * opt->mom[i] + (1.0f - beta1) * grad;
        float v = beta2 * opt->var[i] + (1.0f - beta2) * grad * grad;

        opt->mom[i] = m;
        opt->var[i] = v;

        float m_hat = m / bc1;
        float v_hat = v / bc2;

        float update = lr * m_hat / (sqrtf(v_hat) + eps);
        if (wd > 0.0f) {
            update += lr * wd * params[i];
        }

        params[i] -= update;
        if (!isfinite(params[i])) params[i] = 0.0f;  /* safety */
    }
    
    /* Poincare projection if enabled - ONCE per step, not per param! */
    if (cfg->use_poincare) {
        float norm = gaad_norm(params, param_count);
        float max_norm = (1.0f / sqrtf(fmaxf(cfg->curvature, GAAD_EPS))) * (1.0f - GAAD_EPS);
        if (norm > max_norm && norm > 0.0f) {
            float scale = max_norm / norm;
            for (int j = 0; j < param_count; ++j) params[j] *= scale;
        }
    }
    
    /* Update Q-controller if enabled */
    if (cfg->use_q_controller && cfg->q_table && cfg->q_state_dim > 0 && cfg->q_action_dim > 0) {
        /* Build metrics vector from available optimizer state */
        float metrics[8] = {0};
        int metric_count = 0;
        if (opt->step > 0 && param_count > 0) {
            /* Use gradient statistics as state input */
            float grad_mean = 0.0f, grad_var = 0.0f;
            for (int j = 0; j < param_count && metric_count < 4; ++j) {
                grad_mean += grads[j];
            }
            grad_mean /= (float)param_count;
            for (int j = 0; j < param_count; ++j) {
                float d = grads[j] - grad_mean;
                grad_var += d * d;
            }
            grad_var /= (float)param_count;
            metrics[0] = grad_mean;
            metrics[1] = sqrtf(fmaxf(grad_var, 0.0f));
            metrics[2] = cfg->base_lr;
            metrics[3] = (float)opt->step;
            metric_count = 4;
        }

        /* Encode state from metrics */
        int state = gaad_q_get_state(metrics, metric_count, cfg->q_state_dim);

        /* Compute reward from previous step (negative strain = good) */
        float reward = 0.0f;
        gaad_check_strain_conditions(opt, grads, param_count, 0.0f);
        int strain_level = opt->strain_hook_id;  /* Use strain_hook_id as proxy for strain severity */
        if (opt->q_last_state >= 0 && opt->q_last_action >= 0) {
            /* Reward: lower strain is better, stable LR is better */
            reward = -((float)strain_level * 0.1f);
            /* Small positive reward for not triggering extreme strain */
            if (strain_level == 0) reward += 0.1f;
            /* Update Q-table */
            int idx = opt->q_last_state * cfg->q_action_dim + opt->q_last_action;
            int next_idx = state * cfg->q_action_dim + 0;
            float max_next_q = cfg->q_table[next_idx];
            for (int a = 1; a < cfg->q_action_dim; ++a) {
                int ni = state * cfg->q_action_dim + a;
                if (cfg->q_table[ni] > max_next_q) max_next_q = cfg->q_table[ni];
            }
            cfg->q_table[idx] += cfg->q_lr * (reward + cfg->q_gamma * max_next_q - cfg->q_table[idx]);
        }

        /* Choose action: epsilon-greedy */
        int action = 0;
        float rand_val = (float)rand() / (float)RAND_MAX;
        if (rand_val < cfg->q_epsilon) {
            action = rand() % cfg->q_action_dim;
        } else {
            float best_q = -1e30f;
            for (int a = 0; a < cfg->q_action_dim; ++a) {
                int idx = state * cfg->q_action_dim + a;
                if (cfg->q_table[idx] > best_q) {
                    best_q = cfg->q_table[idx];
                    action = a;
                }
            }
        }

        /* Apply action: map discrete action to LR adjustment (3 action types) */
        /* action 0 = no change, action 1 = increase LR, action 2 = decrease LR */
        if (action == 1) {
            cfg->base_lr *= 1.1f;
        } else if (action == 2) {
            cfg->base_lr *= 0.9f;
        }
        /* Clamp LR to reasonable range */
        cfg->base_lr = fmaxf(1e-6f, fminf(1e-1f, cfg->base_lr));

        /* Decay epsilon */
        cfg->q_epsilon = fmaxf(cfg->q_epsilon_min, cfg->q_epsilon * cfg->q_epsilon_decay);

        /* Save state for next step */
        opt->q_last_state = state;
        opt->q_last_action = action;
        opt->q_step++;
    }
    
    /* Check strain conditions */
    gaad_check_strain_conditions(opt, grads, param_count, 0.0f);  /* loss not available here */
}

void bear_gaad_set_lr(BearGAADOptimizer* opt, float lr) {
    if (opt) opt->config.base_lr = lr;
}

float bear_gaad_get_lr(const BearGAADOptimizer* opt) {
    return opt ? opt->config.base_lr : 0.0f;
}

void bear_gaad_trigger_strain(BearGAADOptimizer* opt, const char* reason, float severity) {
    gaad_trigger_strain_hook(opt, reason, severity);
}

/* ===================================================================
 * Gradient Statistics
 * =================================================================== */

BearGAADGradStats bear_gaad_grad_stats(const float* grad, int n, BearArena* temp_arena) {
    (void)temp_arena;  /* Suppress unused warning */
    BearGAADGradStats stats = {0};
    if (!grad || n <= 0) return stats;
    
    double sum = 0, sum2 = 0, sum3 = 0, sum4 = 0;
    float max_v = -1e30f, min_v = 1e30f;
    
    for (int i = 0; i < n; ++i) {
        float g = grad[i];
        if (!isfinite(g)) continue;
        double d = g;
        sum += d;
        sum2 += d * d;
        sum3 += d * d * d;
        sum4 += d * d * d * d;
        if (g > max_v) max_v = g;
        if (g < min_v) min_v = g;
    }
    
    int count = n;
    stats.mean = (float)(sum / count);
    stats.std = (float)sqrt(sum2 / count - (sum / count) * (sum / count));
    stats.max = max_v;
    stats.min = min_v;
    stats.variance = stats.std * stats.std;
    stats.skewness = (float)(sum3 / count / pow(stats.std, 3));
    stats.kurtosis = (float)(sum4 / count / pow(stats.std, 4)) - 3.0f;
    stats.spectral_norm = stats.max - stats.min;  /* Simplified */
    
    return stats;
}

int bear_gaad_is_resonant(const BearGAADOptimizer* opt, const float* grad, int n) {
    if (!opt || !opt->config.use_resonant) return 0;
    
    float norm = gaad_norm(grad, n);
    float mean_norm = 0.0f;
    int count = 0;
    
    if (opt->res_hist) {
        int w = opt->config.resonant_window;
        for (int j = 0; j < w; ++j) {
            int idx = (opt->res_idx - j + w) % w;
            float* h = &opt->res_hist[idx * opt->capacity];
            if (h[0] != -1e30f) {
                mean_norm += gaad_norm(h, n);
                count++;
            }
        }
        if (count > 0) mean_norm /= count;
    }
    
    return norm > opt->config.resonant_threshold * fmaxf(mean_norm, 1e-6f);
}

#ifdef __cplusplus
}
#endif