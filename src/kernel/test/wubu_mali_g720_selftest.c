/*
 * wubu_mali_g720_selftest.c -- verifies ARM Mali G720 routing.
 */
#include "wubu_mali_g720.h"
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
    printf("=== wubu_mali_g720_selftest ===\n");

    wubu_mali_g720_probe();

    int p = wubu_mali_g720_present();
    CHECK(p == 0 || p == 1, "mali_g720 present is boolean");

    /* Panthor routing. */
    CHECK(wubu_mali_g720_uses_panthor(1) == 1, "panthor available = uses panthor");
    CHECK(wubu_mali_g720_uses_panthor(0) == 0, "no panthor = not used");

    /* Vulkan support. */
    CHECK(wubu_mali_g720_vulkan(14) == 1, "Vulkan 1.4 supported on G720");
    CHECK(wubu_mali_g720_vulkan(13) == 0, "Vulkan 1.3 not the G720 level");

    /* Summary builds. */
    char out[160] = "";
    wubu_mali_g720_summary(out, sizeof(out));
    CHECK(strstr(out, "mali_g720[") != NULL, "summary has mali_g720 fragment");

    printf("\n=== MALI_G720 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
