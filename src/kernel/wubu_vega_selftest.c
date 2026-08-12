/*
 * wubu_vega_selftest.c -- verifies AMD GCN5 Vega routing.
 */
#include "wubu_vega.h"
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
    printf("=== wubu_vega_selftest ===\n");

    wubu_vega_probe();

    int p = wubu_vega_present();
    CHECK(p == 0 || p == 1, "vega present is boolean");

    /* RADV. */
    CHECK(wubu_vega_radv(1) == 1, "RADV Vulkan supported");
    CHECK(wubu_vega_radv(0) == 0, "no RADV");

    /* HBM memory. */
    CHECK(wubu_vega_hbm_memory(1) == 1, "HBM2 memory present");
    CHECK(wubu_vega_hbm_memory(0) == 0, "no HBM");

    /* Summary builds. */
    char out[160] = "";
    wubu_vega_summary(out, sizeof(out));
    CHECK(strstr(out, "vega[") != NULL, "summary has vega fragment");

    printf("\n=== VEGA TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
