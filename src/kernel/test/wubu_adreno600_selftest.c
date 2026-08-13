/*
 * wubu_adreno600_selftest.c -- verifies Qualcomm Adreno 600 routing.
 */
#include "wubu_adreno600.h"
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
    printf("=== wubu_adreno600_selftest ===\n");

    wubu_adreno600_probe();

    int p = wubu_adreno600_present();
    CHECK(p == 0 || p == 1, "adreno600 present is boolean");

    /* Turnip Vulkan routing. */
    CHECK(wubu_adreno600_has_vulkan(1) == 1, "vulkan available = has vulkan");
    CHECK(wubu_adreno600_has_vulkan(0) == 0, "no vulkan = no vulkan");

    /* a6xx check. */
    CHECK(wubu_adreno600_is_a6xx(1) == 1, "a6xx flag set");
    CHECK(wubu_adreno600_is_a6xx(0) == 0, "not a6xx");

    /* Summary builds. */
    char out[160] = "";
    wubu_adreno600_summary(out, sizeof(out));
    CHECK(strstr(out, "adreno600[") != NULL, "summary has adreno600 fragment");

    printf("\n=== ADRENO600 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
