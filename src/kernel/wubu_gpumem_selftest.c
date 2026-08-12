/*
 * wubu_gpumem_selftest.c -- verifies GPU memory bandwidth routing.
 */
#include "wubu_gpumem.h"
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
    printf("=== wubu_gpumem_selftest ===\n");

    wubu_gpumem_probe();

    int p = wubu_gpumem_present();
    CHECK(p == 0 || p == 1, "gpumem present is boolean");

    /* Bandwidth tiers. */
    CHECK(wubu_gpumem_tier(100) == 0, "100GB/s = entry");
    CHECK(wubu_gpumem_tier(300) == 1, "300GB/s = mid");
    CHECK(wubu_gpumem_tier(600) == 2, "600GB/s = high");
    CHECK(wubu_gpumem_tier(1000) == 3, "1000GB/s = ultra");

    /* VRAM flag boolean. */
    CHECK(wubu_gpumem_is_gb() == 0 || wubu_gpumem_is_gb() == 1, "vram flag is boolean");

    /* Tier strings. */
    CHECK(strcmp(wubu_gpumem_tier_str(0), "entry") == 0, "tier 0 = entry");
    CHECK(strcmp(wubu_gpumem_tier_str(3), "ultra") == 0, "tier 3 = ultra");

    /* Summary builds. */
    char out[160] = "";
    wubu_gpumem_summary(out, sizeof(out));
    CHECK(strstr(out, "gpumem[") != NULL, "summary has gpumem fragment");

    printf("\n=== GPUMEM TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
