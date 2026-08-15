/* Quick test: do random weights produce non-zero actions? */
#define _POSIX_C_SOURCE 200809L
#include "src/bear/bear_arena.h"
#include "src/bear/bear_nn.h"
#include "src/bear/bear_env.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

int main() {
    BearArena arena;
    bear_arena_create(&arena, 1024*1024);

    /* Create 1-pole env */
    BearEnv* env = bear_env_create_npole(1, 1, &arena);
    if (!env) { printf("env create failed\n"); return 1; }
    bear_env_reset_all(env, &arena);

    /* Create policy: input=128 (pe enc dim), output=1 (continuous) */
    BearPolicyNet policy;
    int ph[] = {128, 128};
    if (bear_policy_create_mlp(&policy, &arena, 128, 1, 0, ph, 2)) {
        printf("policy create failed\n"); return 1;
    }
    bear_orthogonal_init_params(&policy, 1.0f);
    policy.logstd = NULL;
    policy.logstd_fixed = 0.0f;

    /* Test forward pass with dummy observation */
    float dummy_obs[6] = {0.1f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f};  /* x, vx, sin, cos, omega */
    float enc_out[128];
    
    /* Simple identity encoder - just use obs as encoding for test */
    for (int i = 0; i < 128; ++i) enc_out[i] = (i < 6) ? dummy_obs[i] : 0.0f;

    BearTensor enc_t, act, lp, val, h_out;
    int64_t enc_shape[2] = {1, 128};
    int64_t act_shape[2] = {1, 1};
    bear_tensor_create(&arena, &enc_t, enc_shape, 2, BEAR_DTYPE_F32, "enc");
    bear_tensor_create(&arena, &act, act_shape, 2, BEAR_DTYPE_F32, "act");
    bear_tensor_create(&arena, &lp, (int64_t[]){1}, 1, BEAR_DTYPE_F32, "lp");
    bear_tensor_create(&arena, &val, (int64_t[]){1}, 1, BEAR_DTYPE_F32, "val");
    bear_tensor_create(&arena, &h_out, enc_shape, 2, BEAR_DTYPE_F32, "h");
    memcpy(enc_t.data, enc_out, 128*sizeof(float));

    bear_policy_forward(&policy, &enc_t, NULL, &act, &lp, &val, &h_out, &arena);
    bear_policy_deterministic(&policy, &act);

    float action = ((float*)act.data)[0];
    float logprob = ((float*)lp.data)[0];
    printf("Action: %f, logprob: %f\n", action, logprob);

    /* Sample stochastic */
    bear_arena_reset(&arena);
    bear_tensor_create(&arena, &enc_t, enc_shape, 2, BEAR_DTYPE_F32, "enc");
    bear_tensor_create(&arena, &act, act_shape, 2, BEAR_DTYPE_F32, "act");
    bear_tensor_create(&arena, &lp, (int64_t[]){1}, 1, BEAR_DTYPE_F32, "lp");
    memcpy(enc_t.data, enc_out, 128*sizeof(float));
    bear_policy_forward(&policy, &enc_t, NULL, &act, &lp, NULL, NULL, &arena);
    bear_policy_sample(&policy, &act, &lp, NULL);
    printf("Sampled action: %f, logprob: %f\n", ((float*)act.data)[0], ((float*)lp.data)[0]);

    bear_arena_destroy(&arena);
    return 0;
}