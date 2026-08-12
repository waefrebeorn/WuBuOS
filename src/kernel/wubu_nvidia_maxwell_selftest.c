/*
 * wubu_nvidia_maxwell_selftest.c -- verifies NVIDIA Maxwell routing.
 */
#include "wubu_nvidia_maxwell.h"
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
    printf("=== wubu_nvidia_maxwell_selftest ===\n");

    wubu_nvidia_maxwell_probe();

    int p = wubu_nvidia_maxwell_present();
    CHECK(p == 0 || p == 1, "nvidia_maxwell present is boolean");

    /* Proprietary driver routing. */
    CHECK(wubu_nvidia_maxwell_uses_proprietary(535) == 1, "535 = proprietary");
    CHECK(wubu_nvidia_maxwell_uses_proprietary(590) == 1, "590 = proprietary");
    CHECK(wubu_nvidia_maxwell_uses_proprietary(470) == 0, "470 = legacy fallback");

    /* NVK. */
    CHECK(wubu_nvidia_maxwell_has_nvk(1) == 1, "NVK enabled for Maxwell");
    CHECK(wubu_nvidia_maxwell_has_nvk(0) == 0, "no NVK");

    /* Summary builds. */
    char out[160] = "";
    wubu_nvidia_maxwell_summary(out, sizeof(out));
    CHECK(strstr(out, "nvidia_maxwell[") != NULL, "summary has nvidia_maxwell fragment");

    printf("\n=== NVIDIA_MAXWELL TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
