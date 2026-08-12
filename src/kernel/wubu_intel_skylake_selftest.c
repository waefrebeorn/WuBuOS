/*
 * wubu_intel_skylake_selftest.c -- verifies Intel Gen9 Skylake routing.
 */
#include "wubu_intel_skylake.h"
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
    printf("=== wubu_intel_skylake_selftest ===\n");

    wubu_intel_skylake_probe();

    int p = wubu_intel_skylake_present();
    CHECK(p == 0 || p == 1, "skylake present is boolean");

    /* Iris Mesa. */
    CHECK(wubu_intel_skylake_uses_iris(1) == 1, "GL = uses Iris");
    CHECK(wubu_intel_skylake_uses_iris(0) == 0, "no GL = no Iris");

    /* ANV Vulkan. */
    CHECK(wubu_intel_skylake_has_anv(1) == 1, "ANV Vulkan supported");
    CHECK(wubu_intel_skylake_has_anv(0) == 0, "no ANV");

    /* Summary builds. */
    char out[160] = "";
    wubu_intel_skylake_summary(out, sizeof(out));
    CHECK(strstr(out, "skylake[") != NULL, "summary has skylake fragment");

    printf("\n=== SKYLAKE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
