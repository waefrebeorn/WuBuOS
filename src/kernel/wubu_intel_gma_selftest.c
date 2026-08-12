/*
 * wubu_intel_gma_selftest.c -- verifies Intel GMA legacy GPU routing.
 */
#include "wubu_intel_gma.h"
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
    printf("=== wubu_intel_gma_selftest ===\n");

    wubu_intel_gma_probe();

    int p = wubu_intel_gma_present();
    CHECK(p == 0 || p == 1, "intel_gma present is boolean");

    /* i915 routing. */
    CHECK(wubu_intel_gma_uses_i915(3) == 1, "G31/G45 (gen3) = i915");
    CHECK(wubu_intel_gma_uses_i915(5) == 1, "GMA 950 (gen5) = i915");
    CHECK(wubu_intel_gma_uses_i915(8) == 0, "gen8 = not GMA legacy");

    /* llvmpipe fallback. */
    CHECK(wubu_intel_gma_needs_llvmpipe(0) == 1, "no accel = llvmpipe");
    CHECK(wubu_intel_gma_needs_llvmpipe(1) == 0, "accel = no llvmpipe");

    /* Summary builds. */
    char out[160] = "";
    wubu_intel_gma_summary(out, sizeof(out));
    CHECK(strstr(out, "intel_gma[") != NULL, "summary has intel_gma fragment");

    printf("\n=== INTEL_GMA TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
