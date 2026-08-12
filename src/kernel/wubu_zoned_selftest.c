/*
 * wubu_zoned_selftest.c -- verifies kernel-owned zoned storage routing.
 */
#include "wubu_zoned.h"
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
    printf("=== wubu_zoned_selftest ===\n\n");

    wubu_hw_detect();
    wubu_zoned_probe();

    printf("  zoned=%d smr=%d zns=%d zonefs=%d zones=%d\n",
           wubu_zoned_present(), wubu_zoned_smr(), wubu_zoned_zns(),
           wubu_zoned_zonefs(), wubu_zoned_zones());

    /* Zoned driver routing is always consistent. */
    CHECK(strcmp(wubu_zoned_driver_for("nvme"), "nvme-zns") == 0,
          "nvme -> nvme-zns");
    CHECK(strcmp(wubu_zoned_driver_for("sd"), "zbc") == 0,
          "sd -> zbc");
    CHECK(strcmp(wubu_zoned_driver_for("sata"), "zbc") == 0,
          "sata -> zbc");
    CHECK(strcmp(wubu_zoned_driver_for("zonefs"), "zonefs") == 0,
          "zonefs -> zonefs");
    CHECK(strcmp(wubu_zoned_driver_for("unknown"), "blk-zoned") == 0,
          "unknown -> blk-zoned fallback");

    char s[256];
    wubu_zoned_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "zoned summary generated");

    printf("\n=== ZONED TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
