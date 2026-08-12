/*
 * wubu_raidcache_selftest.c -- verifies kernel-owned RAID-cache routing.
 */
#include "wubu_raidcache.h"
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
    printf("=== wubu_raidcache_selftest ===\n\n");

    wubu_hw_detect();
    wubu_raidcache_probe();

    printf("  cache=%d dm-cache=%d bcache=%d zram=%d raid-jnl=%d\n",
           wubu_raidcache_present(), wubu_raidcache_dm_cache(),
           wubu_raidcache_bcache(), wubu_raidcache_zram(),
           wubu_raidcache_raid_journal());

    /* Cache driver routing is always consistent. */
    CHECK(strcmp(wubu_raidcache_driver_for("dm-cache"), "dm-cache") == 0,
          "dm-cache -> dm-cache");
    CHECK(strcmp(wubu_raidcache_driver_for("bcache"), "bcache") == 0,
          "bcache -> bcache");
    CHECK(strcmp(wubu_raidcache_driver_for("zram"), "zram") == 0,
          "zram -> zram");
    CHECK(strcmp(wubu_raidcache_driver_for("raid5"), "raid5-cache") == 0,
          "raid5 -> raid5-cache");
    CHECK(strcmp(wubu_raidcache_driver_for("lvm"), "lvm-cache") == 0,
          "lvm -> lvm-cache");
    CHECK(strcmp(wubu_raidcache_driver_for("unknown"), "cache-core") == 0,
          "unknown -> cache-core fallback");

    char s[256];
    wubu_raidcache_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "raidcache summary generated");

    printf("\n=== RAIDCACHE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
