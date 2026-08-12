/*
 * wubu_arctic_islands_selftest.c -- verifies AMD GCN4 routing.
 */
#include "wubu_arctic_islands.h"
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
    printf("=== wubu_arctic_islands_selftest ===\n");

    wubu_arctic_islands_probe();

    int p = wubu_arctic_islands_present();
    CHECK(p == 0 || p == 1, "arctic_islands present is boolean");

    /* RADV. */
    CHECK(wubu_arctic_islands_uses_radv(1) == 1, "RADV Vulkan supported");
    CHECK(wubu_arctic_islands_uses_radv(0) == 0, "no RADV");

    /* Vulkan level. */
    CHECK(wubu_arctic_islands_vulkan_level(4) == 14, "GCN4 = Vulkan 1.4");
    CHECK(wubu_arctic_islands_vulkan_level(3) == 13, "GCN3 = Vulkan 1.3");

    /* Summary builds. */
    char out[160] = "";
    wubu_arctic_islands_summary(out, sizeof(out));
    CHECK(strstr(out, "arctic_islands[") != NULL, "summary has arctic_islands fragment");

    printf("\n=== ARCTIC_ISLANDS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
