/* Test if GAAD updates weights after backward pass */
#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_arena.h"
#include "src/bear/bear_nn.h"
#include "src/bear/bear_ppo.h"
#include "src/bear/bear_gaad.h"
#include "src/bear/bear_opt.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

int main() {
    BearArena arena;
    bear_arena_create(&arena, 1024*1024);

    /* Create simple policy: input=8, hidden=32, output=1 (continuous) */
    BearPolicyNet policy;
    int ph[] = {32};
    if (bear_policy_create_mlp(&policy, &arena, 8, 1, 0, ph, 1)) {
        printf("policy create failed\n"); return 1;
    }
    bear_orthogonal_init_params(&policy, 1.0f);
    policy.logstd = NULL;
    policy.logstd_fixed = 0.0f;

    /* Create value net: input=8, hidden=32, output=1 */
    BearValueNet value;
    int vh[] = {32};
    if (bear_value_create(&value, &arena, 8, vh, 1)) {
        printf("value create failed\n"); return 1;
    }
    bear_value_orthogonal_init(&value, 1.0f);

    /* Save initial weights */
    float* w0 = (float*)policy.layers[0].param->weight.data;
    float w0_first = w0[0];
    printf("Initial policy layer0 weight[0]: %.6f\n", w0_first);

    /* Create GAAD */
    BearGAADConfig gc = bear_gaad_default_config();
    gc.base_lr = 1e-3f;
    gc.use_log_g_scaling = 1; gc.use_anisotropic = 1; gc.use_resonant = 1; gc.use_poincare = 1;
    int pc = 0;
    for (int i = 0; i < policy.num_layers; ++i) if (policy.layers[i].param && policy.layers[i].param->weight.data)
        pc += policy.layers[i].param->weight.shape[0] * policy.layers[i].param->weight.shape[1];
    for (int i = 0; i < value.num_layers; ++i) if (value.layers[i].param && value.layers[i].param->weight.data)
        pc += value.layers[i].param->weight.shape[0] * value.layers[i].param->weight.shape[1];
    BearGAADOptimizer* gaad = bear_gaad_create(&arena, &gc, pc);
    if (!gaad) { printf("gaad create failed\n"); return 1; }

    /* Dummy forward/backward */
    for (int iter = 0; iter < 10; ++iter) {
        BearArena temp_s;
        bear_arena_create(&temp_s, 1024*1024);
        
        /* Forward */
        BearTensor obs, ac, lp, vl, ho;
        bear_tensor_create(&temp_s, &obs, (int64_t[]){4, 8}, 2, BEAR_DTYPE_F32, "obs");
        bear_tensor_create(&temp_s, &ac, (int64_t[]){4, 1}, 2, BEAR_DTYPE_F32, "ac");
        bear_tensor_create(&temp_s, &lp, (int64_t[]){4}, 1, BEAR_DTYPE_F32, "lp");
        bear_tensor_create(&temp_s, &vl, (int64_t[]){4}, 1, BEAR_DTYPE_F32, "vl");
        bear_tensor_create(&temp_s, &ho, (int64_t[]){4, 32}, 2, BEAR_DTYPE_F32, "ho");
        
        /* Random obs */
        float* o = (float*)obs.data;
        for (int i = 0; i < 32; ++i) o[i] = ((float)rand() / RAND_MAX - 0.5f);
        
        bear_policy_forward(&policy, &obs, NULL, &ac, &lp, &vl, &ho, &temp_s);
        bear_value_forward(&value, &obs, &vl, &temp_s);
        
        /* Dummy advantages and returns */
        BearTensor adv, ret;
        bear_tensor_create(&temp_s, &adv, (int64_t[]){4}, 1, BEAR_DTYPE_F32, "adv");
        bear_tensor_create(&temp_s, &ret, (int64_t[]){4}, 1, BEAR_DTYPE_F32, "ret");
        float* a = (float*)adv.data; float* r = (float*)ret.data;
        for (int i = 0; i < 4; ++i) { a[i] = 1.0f; r[i] = 1.0f; }
        
        /* Policy backward */
        BearPPOConfig cfg = bear_ppo_default_config();
        cfg.clip_coef = 0.2f;
        bear_policy_backward(&policy, &obs, &ac, &lp, &adv, cfg.clip_coef, 1.0f, &temp_s);
        bear_value_backward(&value, &obs, &vl, &ret, cfg.vf_coef, &temp_s);
        
        /* Collect gradients */
        int tp = 0;
        for (int i = 0; i < policy.num_layers; ++i) {
            BearParam* p = policy.layers[i].param;
            if (p && p->grad.data && p->weight.data) tp += (int)bear_tensor_numel(&p->grad);
        }
        for (int i = 0; i < value.num_layers; ++i) {
            BearParam* p = value.layers[i].param;
            if (p && p->grad.data && p->weight.data) tp += (int)bear_tensor_numel(&p->grad);
        }
        
        float* ag = (float*)bear_arena_alloc(&temp_s, tp * sizeof(float), 16);
        float* ap = (float*)bear_arena_alloc(&temp_s, tp * sizeof(float), 16);
        int pi = 0;
        for (int i = 0; i < policy.num_layers; ++i) {
            BearParam* p = policy.layers[i].param;
            if (p && p->grad.data && p->weight.data) {
                int n = (int)bear_tensor_numel(&p->grad);
                memcpy(ag + pi, p->grad.data, n * sizeof(float));
                memcpy(ap + pi, p->weight.data, n * sizeof(float));
                pi += n;
            }
        }
        for (int i = 0; i < value.num_layers; ++i) {
            BearParam* p = value.layers[i].param;
            if (p && p->grad.data && p->weight.data) {
                int n = (int)bear_tensor_numel(&p->grad);
                memcpy(ag + pi, p->grad.data, n * sizeof(float));
                memcpy(ap + pi, p->weight.data, n * sizeof(float));
                pi += n;
            }
        }
        
        printf("Iter %d: grad norm = %.6f\n", iter, sqrtf(ag[0]*ag[0] + ag[1]*ag[1] + ag[2]*ag[2]));
        printf("  weight before: %.6f\n", ap[0]);
        
        /* GAAD step */
        bear_gaad_step(gaad, ap, ag, pi, &temp_s);
        
        /* Copy back */
        int xi = 0;
        for (int i = 0; i < policy.num_layers; ++i) {
            BearParam* p = policy.layers[i].param;
            if (p && p->weight.data) {
                int n = (int)bear_tensor_numel(&p->weight);
                memcpy(p->weight.data, ap + xi, n * sizeof(float));
                xi += n;
            }
        }
        for (int i = 0; i < value.num_layers; ++i) {
            BearParam* p = value.layers[i].param;
            if (p && p->weight.data) {
                int n = (int)bear_tensor_numel(&p->weight);
                memcpy(p->weight.data, ap + xi, n * sizeof(float));
                xi += n;
            }
        }
        
        printf("  weight after:  %.6f\n", w0[0]);
        float diff = w0[0] - w0_first;
        printf("  total change:  %.6f\n", diff);
        
        bear_arena_destroy(&temp_s);
    }
    
    bear_arena_destroy(&arena);
    return 0;
}