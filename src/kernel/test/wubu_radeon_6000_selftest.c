/*
 * wubu_radeon_6000_selftest.c -- verifies AMD Radeon HD 6000 routing.
 */
#include "wubu_radeon_6000.h"
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
    printf("=== wubu_radeon_6000_selftest ===\n");

    wubu_radeon_6000_probe();

    int p = wubu_radeon_6000_present();
    CHECK(p == 0 || p == 1, "radeon_6000 present is boolean");

    /* Legacy driver need. */
    CHECK(wubu_radeon_6000_needs_legacy(0) == 1, "no amdgpu = needs legacy");
    CHECK(wubu_radeon_6000_needs_legacy(1) == 0, "amdgpu present = no legacy");

    /* Pre-GCN check. */
    CHECK(wubu_radeon_6000_is_pre_gcn(2) == 1, "NI is pre-GCN");
    CHECK(wubu_radeon_6000_is_pre_gcn(1) == 0, "Evergreen family 1 not NI");

    /* Summary builds. */
    char out[160] = "";
    wubu_radeon_6000_summary(out, sizeof(out));
    CHECK(strstr(out, "radeon_6000[") != NULL, "summary has radeon_6000 fragment");

    printf("\n=== RADEON_6000 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
