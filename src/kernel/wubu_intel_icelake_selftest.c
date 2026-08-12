/*
 * wubu_intel_icelake_selftest.c -- verifies Intel Gen11 Ice Lake routing.
 */
#include "wubu_intel_icelake.h"
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
    printf("=== wubu_intel_icelake_selftest ===\n");

    wubu_intel_icelake_probe();

    int p = wubu_intel_icelake_present();
    CHECK(p == 0 || p == 1, "icelake present is boolean");

    /* Iris Mesa. */
    CHECK(wubu_intel_icelake_uses_iris(1) == 1, "GL = uses Iris");
    CHECK(wubu_intel_icelake_uses_iris(0) == 0, "no GL = no Iris");

    /* ANV Vulkan. */
    CHECK(wubu_intel_icelake_has_anv(1) == 1, "ANV Vulkan supported");
    CHECK(wubu_intel_icelake_has_anv(0) == 0, "no ANV");

    /* Summary builds. */
    char out[160] = "";
    wubu_intel_icelake_summary(out, sizeof(out));
    CHECK(strstr(out, "icelake[") != NULL, "summary has icelake fragment");

    printf("\n=== ICELAKE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
