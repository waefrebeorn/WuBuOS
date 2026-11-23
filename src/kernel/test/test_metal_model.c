/* test_metal_model.c -- ring-0 AGI slice gate. The kernel module runs the
 * same deterministic weights and F16 GEMV as a hosted f32 baseline. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "wubu_metal_model.h"
#include "wubu_metal_model.c"

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); ++fails; } } while (0)

static float baseline_w(uint32_t m, uint32_t k, uint32_t seed)
{
    int v = (int)((m * 31u + k * 17u + seed) % 13u) - 6;
    return (float)v / 8.0f;
}

int main(void)
{
    wubu_metal_arena_t *a = wubu_metal_arena_create(64u * 1024u);
    CHECK(a != 0, "arena create");
    wubu_metal_model_t *m = wubu_metal_model_create(a, 8u, 16u, 7u);
    CHECK(m != 0, "model create");
    CHECK((wubu_metal_model_m(m) == 8u) && (wubu_metal_model_k(m) == 16u),
          "model dimensions");
    CHECK((((uintptr_t)a->aligned) & 63u) == 0, "arena 64-byte alignment");
    CHECK((wubu_metal_arena_used(a) & 63u) == 0, "slab alignment preserved");

    float x[16], got[8], want[8];
    for (uint32_t j = 0; j < 16; j++) x[j] = 1.0f / (float)(j + 1u);
    CHECK(wubu_metal_model_forward(m, x, got) == 0, "forward succeeded");
    for (uint32_t i = 0; i < 8; i++) {
        float s = 0.0f;
        for (uint32_t j = 0; j < 16; j++)
            s += baseline_w(i, j, 7u) * x[j];
        want[i] = s;
        if (fabsf(got[i] - want[i]) > 1e-3f) {
            printf("FAIL: row %u got=%f want=%f\n", i, got[i], want[i]);
            fails++;
        }
    }
    CHECK(wubu_metal_model_forward(0, x, got) != 0, "null model refused");
    wubu_metal_model_free(m);
    wubu_metal_arena_free(a);
    if (!fails) printf("test_metal_model: ALL PASS\n");
    else printf("test_metal_model: %d FAILURES\n", fails);
    return fails ? 1 : 0;
}
