/*
 * wubu_vrr_selftest.c -- verifies G-Sync/FreeSync VRR routing.
 */
#include "wubu_vrr.h"
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
    printf("=== wubu_vrr_selftest ===\n");

    wubu_vrr_probe();

    int p = wubu_vrr_present();
    CHECK(p == 0 || p == 1, "vrr present is boolean");

    /* FreeSync. */
    CHECK(wubu_vrr_is_freesync(1) == 1, "FreeSync via AMDGPU");
    CHECK(wubu_vrr_is_freesync(0) == 0, "no FreeSync");

    /* G-Sync. */
    CHECK(wubu_vrr_is_gsync(1) == 1, "G-Sync via NVIDIA");
    CHECK(wubu_vrr_is_gsync(0) == 0, "no G-Sync");

    /* Summary builds. */
    char out[160] = "";
    wubu_vrr_summary(out, sizeof(out));
    CHECK(strstr(out, "vrr[") != NULL, "summary has vrr fragment");

    printf("\n=== VRR TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
