/*
 * wubu_radeon_legacy_selftest.c -- verifies AMD Radeon legacy routing.
 */
#include "wubu_radeon_legacy.h"
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
    printf("=== wubu_radeon_legacy_selftest ===\n");

    wubu_radeon_legacy_probe();

    int p = wubu_radeon_legacy_present();
    CHECK(p == 0 || p == 1, "radeon_legacy present is boolean");

    /* Generation routing. */
    CHECK(wubu_radeon_legacy_gen(1) == 1, "family 1 = Evergreen HD5000");
    CHECK(wubu_radeon_legacy_gen(2) == 2, "family 2 = NI HD6000");
    CHECK(wubu_radeon_legacy_gen(3) == 3, "family 3 = other pre-GCN");
    CHECK(wubu_radeon_legacy_gen(0) == 0, "family 0 = unknown");

    /* amdgpu 6.19 support. */
    CHECK(wubu_radeon_legacy_supported_by_amdgpu(1) == 1, "HD5000 folded into amdgpu");
    CHECK(wubu_radeon_legacy_supported_by_amdgpu(2) == 1, "HD6000 folded into amdgpu");
    CHECK(wubu_radeon_legacy_supported_by_amdgpu(3) == 0, "other pre-GCN not folded");

    /* Summary builds. */
    char out[160] = "";
    wubu_radeon_legacy_summary(out, sizeof(out));
    CHECK(strstr(out, "radeon_legacy[") != NULL, "summary has radeon_legacy fragment");

    printf("\n=== RADEON_LEGACY TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
