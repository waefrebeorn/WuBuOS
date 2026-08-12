/*
 * wubu_instinct_selftest.c -- verifies AMD Instinct MI routing.
 */
#include "wubu_instinct.h"
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
    printf("=== wubu_instinct_selftest ===\n");

    wubu_instinct_probe();

    int p = wubu_instinct_present();
    CHECK(p == 0 || p == 1, "instinct present is boolean");

    /* ROCm. */
    CHECK(wubu_instinct_uses_rocm(1) == 1, "ROCm available");
    CHECK(wubu_instinct_uses_rocm(0) == 0, "no ROCm");

    /* Data center. */
    CHECK(wubu_instinct_is_datacenter(1) == 1, "Instinct = datacenter");
    CHECK(wubu_instinct_is_datacenter(0) == 0, "consumer = not datacenter");

    /* Summary builds. */
    char out[160] = "";
    wubu_instinct_summary(out, sizeof(out));
    CHECK(strstr(out, "instinct[") != NULL, "summary has instinct fragment");

    printf("\n=== INSTINCT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
