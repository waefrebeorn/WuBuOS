/*
 * wubu_smr_selftest.c -- verifies kernel-owned SMR routing.
 */
#include "wubu_smr.h"
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
    printf("=== wubu_smr_selftest ===\n\n");
    wubu_hw_detect();
    wubu_smr_probe();
    printf("  smr=%d zone=%d zns=%d wp=%d zonefs=%d\n",
           wubu_smr_present(), wubu_smr_zone(), wubu_smr_zns(),
           wubu_smr_wp(), wubu_smr_zonefs());

    CHECK(strcmp(wubu_smr_zone_for("swr"), "sequential-write-required") == 0,
          "swr -> sequential-write-required");
    CHECK(strcmp(wubu_smr_zone_for("soc"), "sequential-write-preferred") == 0,
          "soc -> sequential-write-preferred");
    CHECK(strcmp(wubu_smr_zone_for("conv"), "conventional") == 0,
          "conv -> conventional");
    CHECK(strcmp(wubu_smr_zone_for("off"), "offline") == 0,
          "off -> offline");
    CHECK(strcmp(wubu_smr_zone_for("ro"), "read-only") == 0,
          "ro -> read-only");
    CHECK(strcmp(wubu_smr_zone_for("zzz"), "sequential-write-required") == 0,
          "zzz -> swr fallback");

    CHECK(strcmp(wubu_smr_op_for("append"), "append") == 0, "append -> append");
    CHECK(strcmp(wubu_smr_op_for("reset"), "reset") == 0, "reset -> reset");
    CHECK(strcmp(wubu_smr_op_for("report"), "report") == 0, "report -> report");
    CHECK(strcmp(wubu_smr_op_for("open"), "open") == 0, "open -> open");
    CHECK(strcmp(wubu_smr_op_for("close"), "close") == 0, "close -> close");
    CHECK(strcmp(wubu_smr_op_for("zzz"), "report") == 0, "zzz -> report fallback");

    char s[256];
    wubu_smr_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "smr summary generated");

    printf("\n=== SMR TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
