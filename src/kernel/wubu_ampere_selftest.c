/*
 * wubu_ampere_selftest.c -- verifies NVIDIA Ampere routing.
 */
#include "wubu_ampere.h"
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
    printf("=== wubu_ampere_selftest ===\n");

    wubu_ampere_probe();

    int p = wubu_ampere_present();
    CHECK(p == 0 || p == 1, "ampere present is boolean");

    /* Ray tracing. */
    CHECK(wubu_ampere_has_raytracing(1) == 1, "RT cores present");
    CHECK(wubu_ampere_has_raytracing(0) == 0, "no RT cores");

    /* Vulkan. */
    CHECK(wubu_ampere_vulkan(14) == 1, "Vulkan 1.4 ok");
    CHECK(wubu_ampere_vulkan(13) == 0, "Vulkan 1.3 = no");

    /* Summary builds. */
    char out[160] = "";
    wubu_ampere_summary(out, sizeof(out));
    CHECK(strstr(out, "ampere[") != NULL, "summary has ampere fragment");

    printf("\n=== AMPERE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
