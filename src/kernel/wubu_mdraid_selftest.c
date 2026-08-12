/*
 * wubu_mdraid_selftest.c -- verifies storage MD RAID routing.
 */
#include "wubu_mdraid.h"
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
    wubu_mdraid_probe();

    CHECK(wubu_mdraid_present() >= 0, "mdraid_present returns non-negative");

    CHECK(strcmp(wubu_mdraid_level_str(0), "raid0") == 0, "level raid0");
    CHECK(strcmp(wubu_mdraid_level_str(1), "raid1") == 0, "level raid1");
    CHECK(strcmp(wubu_mdraid_level_str(5), "raid5") == 0, "level raid5");
    CHECK(strcmp(wubu_mdraid_level_str(6), "raid6") == 0, "level raid6");
    CHECK(strcmp(wubu_mdraid_level_str(10), "raid10") == 0, "level raid10");
    CHECK(strcmp(wubu_mdraid_level_str(99), "unknown") == 0, "level unknown");

    CHECK(wubu_mdraid_degraded(3, 3) == 0, "degraded none");
    CHECK(wubu_mdraid_degraded(3, 2) == 1, "degraded 1");
    CHECK(wubu_mdraid_degraded(3, 0) == 3, "degraded all");
    CHECK(wubu_mdraid_degraded(0, 0) == 0, "degraded zero total");

    char buf[256];
    wubu_mdraid_summary(buf, sizeof(buf));
    CHECK(strstr(buf, "mdraid[") != NULL, "summary has header");
    CHECK(strstr(buf, "arrays=") != NULL, "summary has arrays");
    CHECK(strstr(buf, "healthy=") != NULL, "summary has healthy");

    printf("=== MDRAID TESTS: %d passed, %d failed ===\n", passes, fails);
    return fails > 0 ? 1 : 0;
}
