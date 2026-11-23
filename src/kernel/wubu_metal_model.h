#ifndef WUBU_METAL_MODEL_H
#define WUBU_METAL_MODEL_H

/* Ring-0 AGI execution seam: one arena + one F16 GEMV forward slice.
 * Opaque types, no libc in the hot path, no third-party deps. */
#include <stddef.h>
#include <stdint.h>

typedef struct wubu_metal_arena wubu_metal_arena_t;
typedef struct wubu_metal_model wubu_metal_model_t;

wubu_metal_arena_t *wubu_metal_arena_create(size_t bytes);
size_t wubu_metal_arena_used(const wubu_metal_arena_t *a);
void wubu_metal_arena_free(wubu_metal_arena_t *a);

/* Weights are generated deterministically from `seed` and quantized to
 * F16 inside the arena. Nothing is loaded from host services. */
wubu_metal_model_t *wubu_metal_model_create(wubu_metal_arena_t *arena,
                                            uint32_t m, uint32_t k,
                                            uint32_t seed);
int wubu_metal_model_forward(const wubu_metal_model_t *model,
                             const float *x, float *y);
uint32_t wubu_metal_model_m(const wubu_metal_model_t *model);
uint32_t wubu_metal_model_k(const wubu_metal_model_t *model);
void wubu_metal_model_free(wubu_metal_model_t *model);

wubu_metal_model_t *wubu_metal_model_global(void);
int wubu_metal_model_global_init(void);

#endif
