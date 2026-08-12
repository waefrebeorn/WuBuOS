/*
 * wubu_volcanic_islands_selftest.c -- verifies AMD GCN3 routing.
 */
#include "wubu_volcanic_islands.h"
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
    printf("=== wubu_volcanic_islands_selftest ===\n");

    wubu_volcanic_islands_probe();

    int p = wubu_volcanic_islands_present();
    CHECK(p == 0 || p == 1, "volcanic_islands present is boolean");

    /* amdgpu folding. */
    CHECK(wubu_volcanic_islands_uses_amdgpu(1) == 1, "amdgpu folded");
    CHECK(wubu_volcanic_islands_uses_amdgpu(0) == 0, "not folded");

    /* RADV Vulkan. */
    CHECK(wubu_volcanic_islands_radv(1) == 1, "RADV Vulkan supported");
    CHECK(wubu_volcanic_islands_radv(0) == 0, "no RADV");

    /* Summary builds. */
    char out[160] = "";
    wubu_volcanic_islands_summary(out, sizeof(out));
    CHECK(strstr(out, "volcanic_islands[") != NULL, "summary has volcanic_islands fragment");

    printf("\n=== VOLCANIC_ISLANDS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
