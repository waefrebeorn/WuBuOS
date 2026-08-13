/*
 * wubu_renoir_selftest.c -- verifies AMD Raven/Renoir APU routing.
 */
#include "wubu_renoir.h"
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
    printf("=== wubu_renoir_selftest ===\n");

    wubu_renoir_probe();

    int p = wubu_renoir_present();
    CHECK(p == 0 || p == 1, "renoir present is boolean");

    /* RADV Vulkan. */
    CHECK(wubu_renoir_uses_radv(1) == 1, "RADV Vulkan supported");
    CHECK(wubu_renoir_uses_radv(0) == 0, "no RADV");

    /* APU check. */
    CHECK(wubu_renoir_is_apu(1) == 1, "Renoir = APU");
    CHECK(wubu_renoir_is_apu(0) == 0, "non-Renoir = not APU");

    /* Summary builds. */
    char out[160] = "";
    wubu_renoir_summary(out, sizeof(out));
    CHECK(strstr(out, "renoir[") != NULL, "summary has renoir fragment");

    printf("\n=== RENoir TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
