/*
 * wubu_vk_effect_test.c -- the GPU effect pass vs the CPU engine.
 *
 * The whole point of putting the effects on the GPU is that they are the SAME
 * effects.  So this test runs both implementations over the same surfaces and
 * requires byte-identical results -- a close-but-not-equal GPU pass is a bug,
 * not a rounding difference.
 *
 * If this host has no usable Vulkan device the test reports SKIP and passes:
 * the CPU engine is the documented fallback.  When a device IS present, a
 * mismatch or a dispatch failure is a hard failure.
 */
#include "wubu_vk_effect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_fails, g_skipped;

#define CHECK(cond, msg) do { \
    g_checks++; \
    if (!(cond)) { g_fails++; printf("  FAIL %s (line %d)\n", msg, __LINE__); } \
} while (0)

static uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) {
    return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

/* A deterministic surface: a gradient plus some structure, so a pass has
 * something non-uniform to act on. */
static void fill(uint32_t *px, int w, int h) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            px[y * w + x] = rgb((uint32_t)(x * 255 / (w > 1 ? w - 1 : 1)),
                                (uint32_t)(y * 255 / (h > 1 ? h - 1 : 1)),
                                (uint32_t)((x + y) * 255 / (w + h > 2 ? w + h - 2 : 1)));
}

int main(int argc, char **argv) {
    printf("=== vk effect pass (GPU vs CPU) ===\n");
    const char *spirv = (argc > 1) ? argv[1] : "src/hosted/wubu_vk_effect.spv";

    WubuVkEffect *e = wubu_vk_effect_new(spirv);
    if (!e) {
        printf("SKIP: no Vulkan device or shader (%s unavailable)\n", spirv);
        g_skipped = 1;
        printf("=== vk effect: SKIP (CPU engine is the fallback) ===\n");
        return 0;
    }
    printf("device: %s\n", wubu_vk_effect_device_name(e));
    CHECK(wubu_vk_effect_available(e), "pipeline is live");

    struct { WubuEffectKind kind; const char *name; uint32_t color; int strength; } cases[] = {
        { WUBU_EFFECT_TINT,     "tint",     rgb(255, 255, 255),  50 },
        { WUBU_EFFECT_TINT,     "tint",     rgb(0, 0, 255),      25 },
        { WUBU_EFFECT_VIGNETTE, "vignette", 0,                    60 },
        { WUBU_EFFECT_GLOW,     "glow",     rgb(0, 0, 255),      50 },
        { WUBU_EFFECT_SCANLINE, "scanline", 0,                    50 },
    };

    int sizes[][2] = { {8, 8}, {64, 64}, {100, 37} };   /* includes non-multiples of 64 */

    for (unsigned c = 0; c < sizeof cases / sizeof cases[0]; c++) {
        for (unsigned s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
            int w = sizes[s][0], h = sizes[s][1];
            uint32_t *cpu = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
            uint32_t *gpu = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
            if (!cpu || !gpu) { free(cpu); free(gpu); CHECK(false, "alloc"); continue; }
            fill(cpu, w, h);
            memcpy(gpu, cpu, (size_t)w * h * sizeof(uint32_t));

            /* CPU: run the real engine. */
            WubuEffectStack *stack = wubu_effect_stack_new();
            if (!stack) { free(cpu); free(gpu); CHECK(false, "stack"); continue; }
            wubu_effect_stack_add(stack, cases[c].kind, cases[c].color, cases[c].strength);
            wubu_effect_apply(stack, cpu, w, h);

            /* GPU: the same pass. */
            const char *err = NULL;
            int rc = wubu_vk_effect_apply(e, gpu, w, h, cases[c].kind,
                                          cases[c].color, cases[c].strength, &err);
            if (rc != 0) {
                printf("  FAIL %s %dx%d: GPU pass failed: %s\n",
                       cases[c].name, w, h, err ? err : "?");
                g_fails++; g_checks++;
            } else {
                int diff = 0;
                for (int i = 0; i < w * h; i++) if (cpu[i] != gpu[i]) { diff++; if (diff <= 2) {
                    printf("    first diff at %d: cpu=%06x gpu=%06x\n", i, cpu[i], gpu[i]);
                } }
                if (diff == 0) {
                    printf("  %-9s %3dx%-3d  identical (%d px)\n",
                           cases[c].name, w, h, w * h);
                }
                CHECK(diff == 0, "GPU matches the CPU engine");
            }
            wubu_effect_stack_free(stack);
            free(cpu); free(gpu);
        }
    }

    wubu_vk_effect_free(e);

    if (g_fails == 0) {
        printf("=== vk effect: %d checks, 0 failed ===\n", g_checks);
        return 0;
    }
    printf("=== vk effect: %d checks, %d failed ===\n", g_checks, g_fails);
    return 1;
}
