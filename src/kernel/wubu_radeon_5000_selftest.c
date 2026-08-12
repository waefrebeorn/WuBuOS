/*
 * wubu_radeon_5000_selftest.c -- verifies AMD Radeon HD 5000 routing.
 */
#include "wubu_radeon_5000.h"
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
    printf("=== wubu_radeon_5000_selftest ===\n");

    wubu_radeon_5000_probe();

    int p = wubu_radeon_5000_present();
    CHECK(p == 0 || p == 1, "radeon_5000 present is boolean");

    /* Legacy driver support. */
    CHECK(wubu_radeon_5000_supports_legacy(1) == 1, "legacy available = supported");
    CHECK(wubu_radeon_5000_supports_legacy(0) == 0, "no legacy = unsupported");

    /* Evergreen check. */
    CHECK(wubu_radeon_5000_is_evergreen(1) == 1, "family 1 = Evergreen");
    CHECK(wubu_radeon_5000_is_evergreen(2) == 0, "family 2 = NI not Evergreen");

    /* Summary builds. */
    char out[160] = "";
    wubu_radeon_5000_summary(out, sizeof(out));
    CHECK(strstr(out, "radeon_5000[") != NULL, "summary has radeon_5000 fragment");

    printf("\n=== RADEON_5000 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
