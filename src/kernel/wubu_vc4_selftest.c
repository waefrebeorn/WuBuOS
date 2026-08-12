/*
 * wubu_vc4_selftest.c -- verifies Broadcom VideoCore vc4 routing.
 */
#include "wubu_vc4.h"
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
    printf("=== wubu_vc4_selftest ===\n");

    wubu_vc4_probe();

    int p = wubu_vc4_present();
    CHECK(p == 0 || p == 1, "vc4 present is boolean");

    /* Dual driver routing. */
    CHECK(wubu_vc4_uses_vc4_v3d(1) == 1, "dual driver = vc4+v3d");
    CHECK(wubu_vc4_uses_vc4_v3d(0) == 0, "not dual driver");

    /* 3D acceleration. */
    CHECK(wubu_vc4_has_3d(1) == 1, "v3d available = has 3D");
    CHECK(wubu_vc4_has_3d(0) == 0, "no v3d = no 3D");

    /* Summary builds. */
    char out[160] = "";
    wubu_vc4_summary(out, sizeof(out));
    CHECK(strstr(out, "vc4[") != NULL, "summary has vc4 fragment");

    printf("\n=== VC4 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
