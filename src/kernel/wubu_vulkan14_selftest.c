/*
 * wubu_vulkan14_selftest.c -- verifies Vulkan 1.4 routing.
 */
#include "wubu_vulkan14.h"
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
    printf("=== wubu_vulkan14_selftest ===\n");

    wubu_vulkan14_probe();

    int p = wubu_vulkan14_present();
    CHECK(p == 0 || p == 1, "vulkan14 present is boolean");

    /* Full profile. */
    CHECK(wubu_vulkan14_full_profile(1) == 1, "full profile available");
    CHECK(wubu_vulkan14_full_profile(0) == 0, "no full profile");

    /* Conformant. */
    CHECK(wubu_vulkan14_is_conformant(1) == 1, "RADV VK 1.4 conformant");
    CHECK(wubu_vulkan14_is_conformant(0) == 0, "not conformant");

    /* Summary builds. */
    char out[160] = "";
    wubu_vulkan14_summary(out, sizeof(out));
    CHECK(strstr(out, "vulkan14[") != NULL, "summary has vulkan14 fragment");

    printf("\n=== VULKAN14 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
