/*
 * wubu_nvidia_kepler_selftest.c -- verifies NVIDIA Kepler legacy routing.
 */
#include "wubu_nvidia_kepler.h"
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
    printf("=== wubu_nvidia_kepler_selftest ===\n");

    wubu_nvidia_kepler_probe();

    int p = wubu_nvidia_kepler_present();
    CHECK(p == 0 || p == 1, "nvidia_kepler present is boolean");

    /* Legacy need. */
    CHECK(wubu_nvidia_kepler_needs_legacy(1) == 1, "Kepler = needs legacy");
    CHECK(wubu_nvidia_kepler_needs_legacy(0) == 0, "non-Kepler = no legacy");

    /* NVK support. */
    CHECK(wubu_nvidia_kepler_nvk_support(1) == 0, "Kepler = no NVK");
    CHECK(wubu_nvidia_kepler_nvk_support(2) == 1, "Maxwell+ = NVK");

    /* Summary builds. */
    char out[160] = "";
    wubu_nvidia_kepler_summary(out, sizeof(out));
    CHECK(strstr(out, "nvidia_kepler[") != NULL, "summary has nvidia_kepler fragment");

    printf("\n=== NVIDIA_KEPLER TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
