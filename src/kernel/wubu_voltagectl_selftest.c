/*
 * wubu_voltagectl_selftest.c -- verifies GPU voltage control routing.
 */
#include "wubu_voltagectl.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } \
    else { passes++; } \
} while (0)

int main(void)
{
    int passes = 0, fails = 0;
    wubu_voltagectl_probe();

    CHECK(wubu_voltagectl_present() >= 0, "voltagectl_present returns non-negative");

    CHECK(strcmp(wubu_voltagectl_state_str(600), "low") == 0, "volt low");
    CHECK(strcmp(wubu_voltagectl_state_str(800), "nominal") == 0, "volt nominal");
    CHECK(strcmp(wubu_voltagectl_state_str(1000), "high") == 0, "volt high");
    CHECK(strcmp(wubu_voltagectl_state_str(1200), "critical") == 0, "volt critical");

    CHECK(wubu_voltagectl_mv_to_uv(800) == 800000, "mv to uv conversion");
    CHECK(wubu_voltagectl_mv_to_uv(0) == 0, "mv zero to uv");

    char buf[256];
    wubu_voltagectl_summary(buf, sizeof(buf));
    CHECK(strstr(buf, "voltagectl[") != NULL, "summary has header");
    CHECK(strstr(buf, "vddc=") != NULL, "summary has vddc");
    CHECK(strstr(buf, "vddgfx=") != NULL, "summary has vddgfx");

    printf("=== VOLTAGECTL TESTS: %d passed, %d failed ===\n", passes, fails);
    return fails > 0 ? 1 : 0;
}
