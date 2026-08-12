/*
 * wubu_nvidia_turing_selftest.c -- verifies NVIDIA Turing routing.
 */
#include "wubu_nvidia_turing.h"
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
    printf("=== wubu_nvidia_turing_selftest ===\n");

    wubu_nvidia_turing_probe();

    int p = wubu_nvidia_turing_present();
    CHECK(p == 0 || p == 1, "nvidia_turing present is boolean");

    /* RT core. */
    CHECK(wubu_nvidia_turing_has_rt_core(1) == 1, "RT core supported");
    CHECK(wubu_nvidia_turing_has_rt_core(0) == 0, "no RT core");

    /* Vulkan. */
    CHECK(wubu_nvidia_turing_vulkan(13) == 1, "Vulkan 1.3 ok");
    CHECK(wubu_nvidia_turing_vulkan(14) == 1, "Vulkan 1.4 ok");
    CHECK(wubu_nvidia_turing_vulkan(12) == 0, "Vulkan 1.2 = no");

    /* Summary builds. */
    char out[160] = "";
    wubu_nvidia_turing_summary(out, sizeof(out));
    CHECK(strstr(out, "nvidia_turing[") != NULL, "summary has nvidia_turing fragment");

    printf("\n=== NVIDIA_TURING TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
