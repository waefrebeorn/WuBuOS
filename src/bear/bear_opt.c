/*
 * bear_opt.c  --  PufferC/BearRL Optimizer Implementation (Adam + Muon)
 */

#include "bear_opt.h"
#include "bear_arena.h"
#include "bear_simd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ===================================================================
 * Optimizer Creation / Registration
 * =================================================================== */

BearOptimizer* bear_optimizer_create(BearArena* arena, BearOptType type, float lr) {
    if (!arena) return NULL;
    
    BearOptimizer* opt = BEAR_ARENA_ALLOC(arena, BearOptimizer, 1);
    if (!opt) return NULL;
    memset(opt, 0, sizeof(BearOptimizer));
    
    opt->type = type;
    opt->lr = lr;
    
    switch (type) {
        case BEAR_OPT_ADAM:
        case BEAR_OPT_ADAMW:
            opt->beta1 = 0.9f;
            opt->beta2 = 0.999f;
            opt->eps = 1e-8f;
            opt->weight_decay = (type == BEAR_OPT_ADAMW) ? 0.01f : 0.0f;
            break;
        case BEAR_OPT_SGD:
            opt->momentum = 0.9f;
            opt->nesterov = 1;
            break;
        case BEAR_OPT_MUON:
            opt->beta1 = 0.95f;  /* Muon uses higher momentum */
            opt->lr = lr;        /* Muon often uses higher LR */
            break;
    }
    
    opt->step = 0;
    opt->num_params = 0;
    opt->adam_states = BEAR_ARENA_ALLOC(arena, BearAdamState, 64);
    opt->muon_states = BEAR_ARENA_ALLOC(arena, BearMuonState, 64);
    
    return opt;
}

int bear_optimizer_register(BearOptimizer* opt, BearParam* param) {
    if (!opt || !param) return -1;
    if (opt->num_params >= 64) return -1;

    int idx = opt->num_params++;

    if (opt->type == BEAR_OPT_ADAM || opt->type == BEAR_OPT_ADAMW) {
        BearAdamState* s = &opt->adam_states[idx];
        s->step = 0;
        /* Allocate m and v with same shape as weight, zero-initialized */
        int out_f = param->weight.shape[0];
        int in_f = param->weight.shape[1];
        int n = out_f * in_f;
        s->m.data = (float*)calloc(n, sizeof(float));
        s->m.ndim = 2;
        s->m.shape[0] = out_f;
        s->m.shape[1] = in_f;
        s->m.dtype = BEAR_DTYPE_F32;
        s->v.data = (float*)calloc(n, sizeof(float));
        s->v.ndim = 2;
        s->v.shape[0] = out_f;
        s->v.shape[1] = in_f;
        s->v.dtype = BEAR_DTYPE_F32;
    } else if (opt->type == BEAR_OPT_MUON) {
        BearMuonState* s = &opt->muon_states[idx];
        s->lr = opt->lr;
        int out_f = param->weight.shape[0];
        int in_f = param->weight.shape[1];
        int n = out_f * in_f;
        s->momentum.data = (float*)calloc(n, sizeof(float));
        s->momentum.ndim = 2;
        s->momentum.shape[0] = out_f;
        s->momentum.shape[1] = in_f;
        s->momentum.dtype = BEAR_DTYPE_F32;
    }

    return 0;
}

/* ===================================================================
 * SGD Update
 * =================================================================== */

static inline void bear_sgd_update_param(BearParam* param, float lr, float momentum, int nesterov) {
    if (param->grad.data == NULL || param->weight.data == NULL) return;
    if (param->weight.dtype != BEAR_DTYPE_F32) return;
    
    float* w = (float*)param->weight.data;
    float* g = (float*)param->grad.data;
    float* m = (float*)param->mom.data;  /* using mom field as momentum buffer */
    int n = (int)bear_tensor_numel(&param->weight);
    
    if (m == NULL) return;
    
    for (int i = 0; i < n; ++i) {
        m[i] = momentum * m[i] + g[i];
        float update = nesterov ? (momentum * m[i] + g[i]) : m[i];
        w[i] -= lr * update;
    }
}

/* ===================================================================
 * Adam / AdamW Update
 * =================================================================== */

void bear_adam_update_param(BearParam* param, BearAdamState* state,
                             float lr, float beta1, float beta2, float eps,
                             float weight_decay, int step) {
    if (param->grad.data == NULL || param->weight.data == NULL) return;
    if (param->weight.dtype != BEAR_DTYPE_F32) return;
    if (!state) return;
    
    float* w = (float*)param->weight.data;
    float* g = (float*)param->grad.data;
    float* m = (float*)state->m.data;
    float* v = (float*)state->v.data;
    int n = (int)bear_tensor_numel(&param->weight);
    
    if (m == NULL || v == NULL) return;
    
    /* Adam: m = beta1 * m + (1 - beta1) * g */
    /*       v = beta2 * v + (1 - beta2) * g^2 */
    /*       m_hat = m / (1 - beta1^step) */
    /*       v_hat = v / (1 - beta2^step) */
    /*       w = w - lr * m_hat / (sqrt(v_hat) + eps) */
    
    float bias_correction1 = 1.0f - powf(beta1, step);
    float bias_correction2 = 1.0f - powf(beta2, step);
    
    for (int i = 0; i < n; ++i) {
        float grad = g[i];
        
        m[i] = beta1 * m[i] + (1.0f - beta1) * grad;
        v[i] = beta2 * v[i] + (1.0f - beta2) * grad * grad;
        
        float m_hat = m[i] / bias_correction1;
        float v_hat = v[i] / bias_correction2;
        
        float update = m_hat / (sqrtf(v_hat) + eps);
        
        if (weight_decay > 0) {
            w[i] *= (1.0f - lr * weight_decay);  /* AdamW style */
        }
        
        w[i] -= lr * update;
    }
}

void bear_adamw_update_param(BearParam* param, BearAdamState* state,
                              float lr, float beta1, float beta2, float eps,
                              float weight_decay, int step) {
    /* AdamW: weight decay applied before gradient step (decoupled) */
    if (param->grad.data == NULL || param->weight.data == NULL) return;
    if (param->weight.dtype != BEAR_DTYPE_F32) return;
    if (!state) return;
    
    float* w = (float*)param->weight.data;
    float* g = (float*)param->grad.data;
    float* m = (float*)state->m.data;
    float* v = (float*)state->v.data;
    int n = (int)bear_tensor_numel(&param->weight);
    
    if (m == NULL || v == NULL) return;
    
    float bias_correction1 = 1.0f - powf(beta1, step);
    float bias_correction2 = 1.0f - powf(beta2, step);
    
    for (int i = 0; i < n; ++i) {
        float grad = g[i];
        
        m[i] = beta1 * m[i] + (1.0f - beta1) * grad;
        v[i] = beta2 * v[i] + (1.0f - beta2) * grad * grad;
        
        float m_hat = m[i] / bias_correction1;
        float v_hat = v[i] / bias_correction2;
        
        float update = m_hat / (sqrtf(v_hat) + eps);
        
        /* Decoupled weight decay (AdamW) */
        w[i] = w[i] * (1.0f - lr * weight_decay) - lr * update;
    }
}

/* ===================================================================
 * Muon Update (PufferLib)
 * 
 * Muon = Momentum + Orthogonal Normalization
 * For weight matrices [out, in], normalize momentum per OUTPUT ROW
 * =================================================================== */

void bear_muon_update_param(BearParam* param, BearMuonState* state,
                             float lr, float beta, float weight_decay) {
    if (param->grad.data == NULL || param->weight.data == NULL) return;
    if (param->weight.dtype != BEAR_DTYPE_F32) return;
    if (!state) return;
    
    float* w = (float*)param->weight.data;
    float* g = (float*)param->grad.data;
    float* m = (float*)state->momentum.data;
    int n = (int)bear_tensor_numel(&param->weight);
    
    if (m == NULL) return;
    
    int rows = param->weight.shape[0];
    int cols = param->weight.shape[1];
    
    for (int r = 0; r < rows; ++r) {
        /* m_row = beta * m_row + (1 - beta) * g_row */
        float norm2 = 0.0f;
        for (int c = 0; c < cols; ++c) {
            int idx = r * cols + c;
            m[idx] = beta * m[idx] + (1.0f - beta) * g[idx];
            norm2 += m[idx] * m[idx];
        }
        
        /* Normalize: m_row = m_row / ||m_row||_2 */
        if (norm2 > 1e-8f) {
            float inv_norm = 1.0f / sqrtf(norm2);
            for (int c = 0; c < cols; ++c) {
                m[r * cols + c] *= inv_norm;
            }
        }
    }
    
    /* Update weights: w = w - lr * m */
    if (weight_decay > 0) {
        for (int i = 0; i < n; ++i) {
            w[i] = w[i] * (1.0f - lr * weight_decay) - lr * m[i];
        }
    } else {
        for (int i = 0; i < n; ++i) {
            w[i] -= lr * m[i];
        }
    }
}

/* ===================================================================
 * Optimizer Step / Zero Grad
 * =================================================================== */

void bear_optimizer_step(BearOptimizer* opt) {
    if (!opt) return;

    opt->step++;

    for (int i = 0; i < opt->num_params; ++i) {
        /* We need to find the param for this index.
         * Since we don't store a back-pointer, we use a different approach:
         * the caller must iterate params directly.
         * This function is a no-op; actual updates happen in bear_ppo_apply_gradients. */
    }
}

/* Apply Adam update to a single param using its grad, m, v, and step.
 * This is the actual per-parameter update used by bear_ppo_apply_gradients. */
void bear_adam_step_param(float* w, float* g, float* m, float* v, int n,
                           float lr, float beta1, float beta2, float eps,
                           float weight_decay, int step) {
    float bc1 = 1.0f - powf(beta1, step);
    float bc2 = 1.0f - powf(beta2, step);
    for (int i = 0; i < n; ++i) {
        float grad = g[i];
        m[i] = beta1 * m[i] + (1.0f - beta1) * grad;
        v[i] = beta2 * v[i] + (1.0f - beta2) * grad * grad;
        float m_hat = m[i] / bc1;
        float v_hat = v[i] / bc2;
        if (weight_decay > 0.0f) {
            w[i] = w[i] * (1.0f - lr * weight_decay) - lr * m_hat / (sqrtf(v_hat) + eps);
        } else {
            w[i] -= lr * m_hat / (sqrtf(v_hat) + eps);
        }
    }
}

void bear_optimizer_zero_grad(BearOptimizer* opt) {
    if (!opt) return;
    /* Zero grads of registered params - stub */
    (void)opt;
}

void bear_optimizer_set_lr(BearOptimizer* opt, float lr) {
    if (!opt) return;
    opt->lr = lr;
}

float bear_optimizer_get_lr(const BearOptimizer* opt) {
    return opt ? opt->lr : 0.0f;
}