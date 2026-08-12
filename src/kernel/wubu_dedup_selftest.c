/*
 * wubu_dedup_selftest.c -- verifies kernel-owned dedup routing.
 */
#include "wubu_dedup.h"
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
    printf("=== wubu_dedup_selftest ===\n\n");

    wubu_hw_detect();
    wubu_dedup_probe();

    printf("  dedup=%d dm=%d btrfs=%d xfs=%d zfs=%d\n",
           wubu_dedup_present(), wubu_dedup_dm(),
           wubu_dedup_btrfs(), wubu_dedup_xfs(), wubu_dedup_zfs());

    /* Mode routing. */
    CHECK(strcmp(wubu_dedup_mode_for("inode"), "inode-dedup") == 0,
          "inode -> inode-dedup");
    CHECK(strcmp(wubu_dedup_mode_for("block"), "block-dedup") == 0,
          "block -> block-dedup");
    CHECK(strcmp(wubu_dedup_mode_for("file"), "file-dedup") == 0,
          "file -> file-dedup");
    CHECK(strcmp(wubu_dedup_mode_for("offline"), "offline") == 0,
          "offline -> offline");
    CHECK(strcmp(wubu_dedup_mode_for("online"), "online") == 0,
          "online -> online");
    CHECK(strcmp(wubu_dedup_mode_for("unknown"), "dedup") == 0,
          "unknown -> dedup fallback");

    /* Level routing. */
    CHECK(strcmp(wubu_dedup_level_for("aggressive"), "aggressive") == 0,
          "aggressive -> aggressive");
    CHECK(strcmp(wubu_dedup_level_for("conservative"), "conservative") == 0,
          "conservative -> conservative");
    CHECK(strcmp(wubu_dedup_level_for("none"), "none") == 0,
          "none -> none");
    CHECK(strcmp(wubu_dedup_level_for("unknown"), "conservative") == 0,
          "unknown -> conservative fallback");

    char s[256];
    wubu_dedup_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "dedup summary generated");

    printf("\n=== DEDUP TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
