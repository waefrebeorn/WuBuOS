
#include "bear_arena.h"
#include "bear_nn.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    uint64_t rng_state[2] = { 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull ^ (uint64_t)time(NULL) };
    
    BearArena arena;
    bear_arena_create(&arena, 64 * 1024 * 1024);
    
    // Create MinGRU policy (like 7-pole env)
    BearPolicyNet policy;
    int policy_rc = bear_policy_create_mingru(&policy, &arena, 16, 1, 0, 64);
    if (policy_rc != 0) {
        printf("Failed to create MinGRU policy: %d\n", policy_rc);
        return 1;
    }
    printf("MinGRU created: type=%d, num_layers=%d, hid_size=%d, obs_dim=%d, act_dim=%d\n",
           policy.type, policy.num_layers, policy.hid_size, policy.obs_dim, policy.act_dim);
    fflush(stdout);
    
    // Create obs tensor (64 envs, obs_dim=16)
    BearTensor obs, actions, logprobs, h_out, h_in;
    int B = 64;
    int64_t obs_shape[2] = { B, 16 };
    int64_t act_shape[2] = { B, 1 };
    int64_t scalar_shape[1] = { B };
    int64_t h_shape[2] = { B, 64 };
    
    bear_tensor_create(&arena, &obs, (int64_t[]){B, 16}, 2, BEAR_DTYPE_F32, "obs");
    bear_tensor_create(&arena, &actions, (int64_t[]){B, 1}, 2, BEAR_DTYPE_F32, "act");
    bear_tensor_create(&arena, &logprobs, (int64_t[]){B}, 1, BEAR_DTYPE_F32, "lp");
    bear_tensor_create(&arena, &h_out, (int64_t[]){B, 64}, 2, BEAR_DTYPE_F32, "h_out");
    bear_tensor_create(&arena, &h_in, (int64_t[]){B, 64}, 2, BEAR_DTYPE_F32, "h_in");
    
    // Fill obs with random data
    float* obs_data = (float*)obs.data;
    for (int i = 0; i < B * 16; ++i) obs_data[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    
    printf("Policy type: %d, GRU: %p\n", policy.type, (void*)policy.gru);
    fflush(stdout);
    
    if (policy.gru) {
        printf("GRU: cell_size=%d, input_size=%d\n", policy.gru->cell_size, policy.gru->input_size);
    }
    
    // Call forward using the same function signature
    printf("Running forward pass...\n");
    fflush(stdout);
    
    // We need a rng_state for sampling
    uint64_t rng[2] = { 0xDEADBEEFDEADBEEFull, 0xCAFEBABECAFEBABEull ^ (uint64_t)time(NULL) };
    
    // Call policy forward with all args
    // bear_policy_forward(const BearPolicyNet* net, const BearTensor* obs, const BearTensor* h_in,
    //                      BearTensor* actions, BearTensor* logprobs, BearTensor* values,
    //                      BearTensor* h_out, BearArena* temp_arena)
    // We'll pass NULL for values since it's not used in this context
    
    // Test multiple times to see if it crashes
    for (int i = 0; i < 10; ++i) {
        printf("Iteration %d...\n", i); fflush(stdout);
        // This is the actual call pattern used in the code
        BearTensor values_dummy;
        int64_t val_shape[1] = { 64 };
        bear_tensor_create(&arena, &values_dummy, (int64_t[]){64}, 1, BEAR_DTYPE_F32, "val_dummy");
        
        bear_policy_forward(&policy, &obs, &h_in, &obs /* act */, &obs /* lp */, &values_dummy, &h_out, &arena);
        printf("  Forward %d done\n", i); fflush(stdout);
    }
    
    printf("Test passed!\n");
    return 0;
}
