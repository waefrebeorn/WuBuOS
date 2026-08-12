/*
 * wubu_raid_selftest.c -- verifies kernel-owned RAID/SAS routing.
 */
#include "wubu_raid.h"
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
    printf("=== wubu_raid_selftest ===\n\n");

    wubu_hw_detect();
    wubu_raid_probe();

    printf("  present=%d sas=%d md=%d drv=%s\n",
           wubu_raid_present(), wubu_raid_has_sas(), wubu_raid_has_md(),
           wubu_raid_driver() ? wubu_raid_driver() : "none");

    /* Controller routing is always consistent. */
    CHECK(strcmp(wubu_raid_controller_driver("megaraid"), "megaraid_sas") == 0,
          "megaraid -> megaraid_sas");
    CHECK(strcmp(wubu_raid_controller_driver("lsi"), "mpt3sas") == 0,
          "lsi -> mpt3sas");
    CHECK(strcmp(wubu_raid_controller_driver("smartpqi"), "smartpqi") == 0,
          "smartpqi -> smartpqi");
    CHECK(strcmp(wubu_raid_controller_driver("adaptec"), "aacraid") == 0,
          "adaptec -> aacraid");
    CHECK(strcmp(wubu_raid_controller_driver("marvell"), "mv_sas") == 0,
          "marvell -> mv_sas");
    CHECK(strcmp(wubu_raid_controller_driver("areca"), "arcmsr") == 0,
          "areca -> arcmsr");
    CHECK(strcmp(wubu_raid_controller_driver("3ware"), "3w-sas") == 0,
          "3ware -> 3w-sas");
    CHECK(strcmp(wubu_raid_controller_driver("unknown"), "scsi_mod") == 0,
          "unknown -> scsi_mod fallback");

    char s[256];
    wubu_raid_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "raid summary generated");

    printf("\n=== RAID TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
