/* Quick test: GAAD Poincaré projection is O(n²) - inside the per-param loop */
#define _POSIX_C_SOURCE 199309L
#include "src/bear/bear_gaad.h"
#include "src/bear/bear_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    BearArena arena;
    bear_arena_create(&arena, 1024*1024);
    
    BearGAADConfig cfg = bear_gaad_default_config();
    cfg.use_poincare = 1;
    cfg.base_lr = 3e-4f;
    
    int param_count = 41152;
    BearGAADOptimizer* opt = bear_gaad_create(&arena, &cfg, param_count);
    
    float* params = (float*)bear_arena_alloc(&arena, param_count * sizeof(float), 16);
    float* grads = (float*)bear_arena_alloc(&arena, param_count * sizeof(float), 16);
    for (int i = 0; i < param_count; ++i) {
        params[i] = 0.01f;
        grads[i] = 0.001f;
    }
    
    clock_t t0 = clock();
    bear_gaad_step(opt, params, grads, param_count, &arena);
    clock_t t1 = clock();
    
    double elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;
    printf("GAAD step for %d params: %.3f seconds\n", param_count, elapsed);
    printf("Estimated 100 iters * 32 minibatches * 4 epochs: %.1f hours\n", 
           elapsed * 100 * 32 * 4 / 3600);
    
    bear_gaad_destroy(opt);
    bear_arena_destroy(&arena);
    return 0;
}
