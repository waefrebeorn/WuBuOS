/*
 * wubu_powervr_selftest.c -- verifies Imagination PowerVR routing.
 */
#include "wubu_powervr.h"
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
    printf("=== wubu_powervr_selftest ===\n");

    wubu_powervr_probe();

    int p = wubu_powervr_present();
    CHECK(p == 0 || p == 1, "powervr present is boolean");

    /* pvrsrvkm routing. */
    CHECK(wubu_powervr_uses_pvrsrvkm(1) == 1, "kernel 6.16+ = uses pvrsrvkm");
    CHECK(wubu_powervr_uses_pvrsrvkm(0) == 0, "no 6.16 = no pvrsrvkm");

    /* Vulkan. */
    CHECK(wubu_powervr_has_vulkan(1) == 1, "mesa 25.3+ = has vulkan");
    CHECK(wubu_powervr_has_vulkan(0) == 0, "no 25.3 = no vulkan");

    /* Summary builds. */
    char out[160] = "";
    wubu_powervr_summary(out, sizeof(out));
    CHECK(strstr(out, "powervr[") != NULL, "summary has powervr fragment");

    printf("\n=== POWERVr TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
