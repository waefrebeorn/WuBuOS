/*
 * wubu_mali_g77_selftest.c -- verifies ARM Mali G77 routing.
 */
#include "wubu_mali_g77.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_mali_g77_selftest ===\n");

    wubu_mali_g77_probe();

    int p = wubu_mali_g77_present();
    CHECK(p == 0 || p == 1, "mali_g77 present is boolean");

    /* Panfrost routing. */
    CHECK(wubu_mali_g77_uses_panfrost(1) == 1, "panfrost available = uses panfrost");
    CHECK(wubu_mali_g77_uses_panfrost(0) == 0, "no panfrost = not used");

    /* PanVK Vulkan. */
    CHECK(wubu_mali_g77_has_panvk(1) == 1, "panvk available = has vulkan");
    CHECK(wubu_mali_g77_has_panvk(0) == 0, "no panvk = no vulkan");

    /* Summary builds. */
    char out[160] = "";
    wubu_mali_g77_summary(out, sizeof(out));
    CHECK(strstr(out, "mali_g77[") != NULL, "summary has mali_g77 fragment");

    printf("\n=== MALI_G77 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
