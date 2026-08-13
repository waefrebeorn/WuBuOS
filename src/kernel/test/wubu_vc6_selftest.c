/*
 * wubu_vc6_selftest.c -- verifies Broadcom VideoCore VI routing.
 */
#include "wubu_vc6.h"
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
    printf("=== wubu_vc6_selftest ===\n");

    wubu_vc6_probe();

    int p = wubu_vc6_present();
    CHECK(p == 0 || p == 1, "vc6 present is boolean");

    /* v3d routing. */
    CHECK(wubu_vc6_uses_v3d(1) == 1, "v3d available = uses v3d");
    CHECK(wubu_vc6_uses_v3d(0) == 0, "no v3d = not used");

    /* Vulkan. */
    CHECK(wubu_vc6_has_vulkan(1) == 1, "vulkan available = has vulkan");
    CHECK(wubu_vc6_has_vulkan(0) == 0, "no vulkan = no vulkan");

    /* Summary builds. */
    char out[160] = "";
    wubu_vc6_summary(out, sizeof(out));
    CHECK(strstr(out, "vc6[") != NULL, "summary has vc6 fragment");

    printf("\n=== VC6 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
