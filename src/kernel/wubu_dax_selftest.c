/*
 * wubu_dax_selftest.c -- verifies kernel-owned DAX routing.
 */
#include "wubu_dax.h"
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
    printf("=== wubu_dax_selftest ===\n\n");
    wubu_hw_detect();
    wubu_dax_probe();
    printf("  dax=%d pmem=%d fs=%d inode=%d dev=%d\n",
           wubu_dax_present(), wubu_dax_pmem(), wubu_dax_fs(),
           wubu_dax_inode(), wubu_dax_dev());

    CHECK(strcmp(wubu_dax_type_for("fs"), "fs-dax") == 0,
          "fs -> fs-dax");
    CHECK(strcmp(wubu_dax_type_for("filesystem"), "fs-dax") == 0,
          "filesystem -> fs-dax");
    CHECK(strcmp(wubu_dax_type_for("dev"), "dev-dax") == 0,
          "dev -> dev-dax");
    CHECK(strcmp(wubu_dax_type_for("device"), "dev-dax") == 0,
          "device -> dev-dax");
    CHECK(strcmp(wubu_dax_type_for("pmd"), "pmd-dax") == 0,
          "pmd -> pmd-dax");
    CHECK(strcmp(wubu_dax_type_for("zzz"), "fs-dax") == 0,
          "zzz -> fs-dax fallback");

    CHECK(strcmp(wubu_dax_fs_for("ext4"), "ext4") == 0,
          "ext4 -> ext4");
    CHECK(strcmp(wubu_dax_fs_for("xfs"), "xfs") == 0,
          "xfs -> xfs");
    CHECK(strcmp(wubu_dax_fs_for("btrfs"), "btrfs") == 0,
          "btrfs -> btrfs");
    CHECK(strcmp(wubu_dax_fs_for("pmem"), "pmemfs") == 0,
          "pmem -> pmemfs");
    CHECK(strcmp(wubu_dax_fs_for("zzz"), "ext4") == 0,
          "zzz -> ext4 fallback");

    char s[256];
    wubu_dax_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "dax summary generated");

    printf("\n=== DAX TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
