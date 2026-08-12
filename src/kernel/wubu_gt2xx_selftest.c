/*
 * wubu_gt2xx_selftest.c -- verifies NVIDIA GT2xx legacy routing.
 */
#include "wubu_gt2xx.h"
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
    printf("=== wubu_gt2xx_selftest ===\n");

    wubu_gt2xx_probe();

    int p = wubu_gt2xx_present();
    CHECK(p == 0 || p == 1, "gt2xx present is boolean");

    /* Legacy EOL fallback to nouveau. */
    CHECK(wubu_gt2xx_needs_nouveau(1) == 1, "legacy EOL = nouveau");
    CHECK(wubu_gt2xx_needs_nouveau(0) == 0, "not EOL = no fallback");

    /* Nouveau availability. */
    CHECK(wubu_gt2xx_nouveau_available(1) == 1, "nouveau available");
    CHECK(wubu_gt2xx_nouveau_available(0) == 0, "no nouveau");

    /* Summary builds. */
    char out[160] = "";
    wubu_gt2xx_summary(out, sizeof(out));
    CHECK(strstr(out, "gt2xx[") != NULL, "summary has gt2xx fragment");

    printf("\n=== GT2XX TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
