/*
 * wubu_navi10_selftest.c -- verifies AMD Navi10 RDNA1 routing.
 */
#include "wubu_navi10.h"
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
    printf("=== wubu_navi10_selftest ===\n");

    wubu_navi10_probe();

    int p = wubu_navi10_present();
    CHECK(p == 0 || p == 1, "navi10 present is boolean");

    /* RADV Vulkan. */
    CHECK(wubu_navi10_uses_radv(1) == 1, "Vulkan = uses RADV");
    CHECK(wubu_navi10_uses_radv(0) == 0, "no Vulkan = no RADV");

    /* Kernel minimum. */
    CHECK(wubu_navi10_kernel_min(503) == 1, "kernel 5.3 = ok");
    CHECK(wubu_navi10_kernel_min(502) == 0, "kernel <5.3 = no");

    /* Summary builds. */
    char out[160] = "";
    wubu_navi10_summary(out, sizeof(out));
    CHECK(strstr(out, "navi10[") != NULL, "summary has navi10 fragment");

    printf("\n=== NAVI10 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
