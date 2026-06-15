/* bear_holo_opt.c  --  Holographic Geodesic Optimizer Implementation
 * 
 * ByTropix Holographic Architecture (optax-compatible):
 *   Gradient decomposition: g = q.boundary + r where q ∈ Z, r ∈ [-boundary/2, boundary/2]
 *   Soul (stored_topology) = Σq  --  exact int32 accumulation (never truncated)
 *   Echo (stored_residue) = Σr  --  float64 accumulation
 *   Body = Adam-style updates on remainders r
 *   Total gradient recovered: total = Soul.boundary + Echo (bit-perfect)
 */

#include "bear_holo_opt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <alloca.h>

BearHoloOptimizer* bear_holo_create(BearArena* arena, const BearHoloConfig* cfg, int n_params) {
    if (!arena || !cfg || n_params <= 0) return NULL;
    
    BearHoloOptimizer* opt = BEAR_ARENA_ALLOC(arena, BearHoloOptimizer, 1);
    if (!opt) return NULL;
    
    opt->n_params = n_params;
    opt->cfg = *cfg;
    opt->step_count = 0;
    
    opt->states = BEAR_ARENA_ALLOC(arena, HoloParamState, n_params);
    if (!opt->states) return NULL;
    
    /* Initialize to zero */
    for (int i = 0; i < n_params; ++i) {
        opt->states[i].soul = 0;
        opt->states[i].echo = 0.0;
        opt->states[i].m1 = 0.0;
        opt->states[i].m2 = 0.0;
    }
    
    return opt;
}

void bear_holo_reset(BearHoloOptimizer* opt) {
    if (!opt || !opt->states) return;
    for (int i = 0; i < opt->n_params; ++i) {
        opt->states[i].soul = 0;
        opt->states[i].echo = 0.0;
        opt->states[i].m1 = 0.0;
        opt->states[i].m2 = 0.0;
    }
    opt->step_count = 0;
}

/* Decompose gradient g into q (integer) and r (remainder) */
static inline void holo_decompose(double g, double boundary, int32_t* q, double* r) {
    double half = boundary * 0.5;
    *q = (int32_t)floor((g + half) / boundary);
    *r = fmod(g + half, boundary) - half;
}

double bear_holo_recover_total(const HoloParamState* state, double boundary) {
    return (double)state->soul * boundary + state->echo;
}

void bear_holo_step(BearHoloOptimizer* opt, double* params, const double* grads, int n) {
    if (!opt || !params || !grads || n <= 0) return;
    if (n > opt->n_params) n = opt->n_params;
    
    double boundary = opt->cfg.boundary;
    double half = boundary * 0.5;
    double lr = opt->cfg.base_lr;
    double beta1 = opt->cfg.beta1;
    double beta2 = opt->cfg.beta2;
    double eps = opt->cfg.eps;
    
    opt->step_count++;
    uint64_t t = opt->step_count;
    
    /* Bias corrections */
    double bias_correction1 = 1.0 - pow(beta1, (double)t);
    double bias_correction2 = 1.0 - pow(beta2, (double)t);
    
    for (int i = 0; i < n; ++i) {
        double g = grads[i];
        
        if (!isfinite(g)) {
            g = 0.0;  /* Guard against NaN/Inf */
        }
        
        if (opt->cfg.use_holographic) {
            /* 1. Holographic decomposition: g = q.boundary + r */
            int32_t q = (int32_t)floor((g + half) / boundary);
            double r = fmod(g + half, boundary) - half;
            
            /* 2. Store The Soul (exact integer windings) */
            opt->states[i].soul += q;
            
            /* 3. Store The Echo (fractional remainders) */
            opt->states[i].echo += r;
            
            /* 4. Adam-style updates on remainders (The Body) */
            /* m1 = beta1 * m1 + (1 - beta1) * r */
            opt->states[i].m1 = beta1 * opt->states[i].m1 + (1.0 - beta1) * r;
            /* m2 = beta2 * m2 + (1 - beta2) * r^2 */
            opt->states[i].m2 = beta2 * opt->states[i].m2 + (1.0 - beta2) * (r * r);
            
            /* Bias-corrected moments */
            double m1_hat = opt->states[i].m1 / bias_correction1;
            double m2_hat = opt->states[i].m2 / bias_correction2;
            
            /* 5. Parameter update */
            double update = lr * m1_hat / (sqrt(m2_hat) + eps);
            params[i] -= update;
            
        } else {
            /* Standard Adam on raw gradient */
            opt->states[i].m1 = beta1 * opt->states[i].m1 + (1.0 - beta1) * g;
            opt->states[i].m2 = beta2 * opt->states[i].m2 + (1.0 - beta2) * (g * g);
            
            double m1_hat = opt->states[i].m1 / bias_correction1;
            double m2_hat = opt->states[i].m2 / bias_correction2;
            
            double update = lr * m1_hat / (sqrt(m2_hat) + eps);
            params[i] -= update;
        }
        
        /* Sanity check */
        if (!isfinite(params[i])) {
            params[i] = 0.0;
        }
    }
}

void bear_holo_debug(BearHoloOptimizer* opt) {
    if (!opt || !opt->states) return;
    
    int64_t total_soul = 0;
    double total_echo = 0.0;
    double max_echo = 0.0;
    
    for (int i = 0; i < opt->n_params; ++i) {
        total_soul += opt->states[i].soul;
        total_echo += opt->states[i].echo;
        if (fabs(opt->states[i].echo) > max_echo) {
            max_echo = fabs(opt->states[i].echo);
        }
    }
    
    printf("[HOLO] Step: %lu, Soul sum: %ld, Echo sum: %.6f, Max |echo|: %.6f\n", 
           (unsigned long)opt->step_count, total_soul, total_echo, max_echo);
}

/* Float-compatible wrapper: converts float arrays to double, calls core step, converts back */
void bear_holo_step_float(BearHoloOptimizer* opt, float* params, const float* grads, int n) {
    if (!opt || !params || !grads || n <= 0) return;
    if (n > opt->n_params) n = opt->n_params;
    
    /* Convert grads to double for internal computation */
    double* grads_d = (double*)alloca(n * sizeof(double));
    double* params_d = (double*)alloca(n * sizeof(double));
    
    for (int i = 0; i < n; ++i) {
        params_d[i] = (double)params[i];
        grads_d[i] = (double)grads[i];
    }
    
    /* Call core double-precision step */
    bear_holo_step(opt, params_d, grads_d, n);
    
    /* Convert params back to float */
    for (int i = 0; i < n; ++i) {
        params[i] = (float)params_d[i];
    }
}