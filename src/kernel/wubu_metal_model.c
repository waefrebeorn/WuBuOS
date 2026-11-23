/* wubu_metal_model.c — ring-0 AGI execution slice. One concern: a byte-
 * budgeted arena and one F16 GEMV forward. Derived from THEORY/07, not a
 * copy of the hosted brain: this is the smallest honest kernel seam. */
#include "wubu_metal_model.h"

#ifdef MYSEED_METAL
extern void *mem_alloc(size_t bytes);
extern void mem_free(void *p);
#else
#include <stdlib.h>
#define mem_alloc malloc
#define mem_free free
#endif

struct wubu_metal_arena {
    uint8_t *base;
    uint8_t *aligned;
    size_t   cap;
    size_t   used;
};

struct wubu_metal_model {
    uint32_t m, k;
    uint16_t *w;  /* F16 bits, row-major */
};

static wubu_metal_model_t *g_model;

wubu_metal_arena_t *wubu_metal_arena_create(size_t bytes)
{
    if (!bytes) return NULL;
    uint8_t *raw = (uint8_t *)mem_alloc(bytes + 64u);
    if (!raw) return NULL;
    wubu_metal_arena_t *a = (wubu_metal_arena_t *)raw;
    uintptr_t p = ((uintptr_t)raw + sizeof(*a) + 63u) & ~(uintptr_t)63u;
    a->base = raw;
    a->aligned = (uint8_t *)p;
    a->cap = bytes;
    a->used = 0;
    return a;
}

size_t wubu_metal_arena_used(const wubu_metal_arena_t *a)
{
    return a ? a->used : 0;
}

void wubu_metal_arena_free(wubu_metal_arena_t *a)
{
    if (a) mem_free(a->base);
}

static uint32_t f32_bits(float f)
{
    uint32_t b;
    unsigned char *p = (unsigned char *)&b;
    const unsigned char *s = (const unsigned char *)&f;
    for (int i = 0; i < 4; i++) p[i] = s[i];
    return b;
}

static float bits_f32(uint32_t b)
{
    float f;
    unsigned char *p = (unsigned char *)&f;
    for (int i = 0; i < 4; i++) p[i] = (unsigned char)(b >> (8 * i));
    return f;
}

static uint16_t f32_to_f16(float f)
{
    uint32_t b = f32_bits(f);
    uint32_t sign = (b >> 16) & 0x8000u;
    int e = (int)((b >> 23) & 0xffu) - 127 + 15;
    uint32_t mant = b & 0x7fffffu;
    if (((b >> 23) & 0xffu) == 0) return (uint16_t)sign;
    if (e <= 0) return (uint16_t)sign;
    if (e >= 31) return (uint16_t)(sign | 0x7c00u);
    uint32_t rounded = (mant + 0x1000u) >> 13;
    if (rounded >= 0x400u) { rounded >>= 1; e++; }
    return (uint16_t)(sign | ((uint32_t)e << 10) | (rounded & 0x3ffu));
}

static float f16_to_f32(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t e = (h >> 10) & 0x1fu;
    uint32_t mant = h & 0x3ffu;
    if (!e) return bits_f32(sign);
    return bits_f32(sign | ((e + 127u - 15u) << 23) | (mant << 13));
}

static float weight_f32(uint32_t m, uint32_t k, uint32_t seed)
{
    int v = (int)((m * 31u + k * 17u + seed) % 13u) - 6;
    return (float)v / 8.0f;
}

wubu_metal_model_t *wubu_metal_model_create(wubu_metal_arena_t *a,
                                            uint32_t m, uint32_t k,
                                            uint32_t seed)
{
    if (!a || !m || !k || (size_t)m * k * 2u > a->cap - a->used) return NULL;
    wubu_metal_model_t *model = (wubu_metal_model_t *)(a->aligned + a->used);
    a->used += (sizeof(*model) + 63u) & ~(size_t)63u;
    model->m = m;
    model->k = k;
    model->w = (uint16_t *)(a->aligned + a->used);
    a->used += (size_t)m * k * 2u;
    for (uint32_t i = 0; i < m; i++)
        for (uint32_t j = 0; j < k; j++)
            model->w[i * k + j] = f32_to_f16(weight_f32(i, j, seed));
    return model;
}

int wubu_metal_model_forward(const wubu_metal_model_t *model,
                             const float *x, float *y)
{
    if (!model || !x || !y) return -1;
    for (uint32_t i = 0; i < model->m; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < model->k; j++)
            sum += f16_to_f32(model->w[i * model->k + j]) * x[j];
        y[i] = sum;
    }
    return 0;
}

uint32_t wubu_metal_model_m(const wubu_metal_model_t *m) { return m ? m->m : 0; }
uint32_t wubu_metal_model_k(const wubu_metal_model_t *m) { return m ? m->k : 0; }
void wubu_metal_model_free(wubu_metal_model_t *m) { (void)m; } /* arena-owned */

wubu_metal_model_t *wubu_metal_model_global(void) { return g_model; }

int wubu_metal_model_global_init(void)
{
    if (g_model) return 0;
    wubu_metal_arena_t *a = wubu_metal_arena_create(64u * 1024u);
    if (!a) return -1;
    g_model = wubu_metal_model_create(a, 8u, 16u, 7u);
    if (!g_model) { wubu_metal_arena_free(a); return -1; }
    return 0;
}
