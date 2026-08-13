/*
 * wubu_raid5_selftest.c -- verifies RAID5 routing.
 */
#include "wubu_raid5.h"
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
    printf("=== wubu_raid5_selftest ===\n\n");
    wubu_hw_detect();
    wubu_raid5_probe();
    printf("  raid5=%d stripe=%d layout=%d parity=%d disks=%d\n",
           wubu_raid5_present(), wubu_raid5_stripe(), wubu_raid5_layout(),
           wubu_raid5_parity(), wubu_raid5_disks());

    CHECK(strcmp(wubu_raid5_layout_for("left-asym"), "left-asymmetric") == 0,
          "left-asym -> left-asymmetric");
    CHECK(strcmp(wubu_raid5_layout_for("left-sym"), "left-symmetric") == 0,
          "left-sym -> left-symmetric");
    CHECK(strcmp(wubu_raid5_layout_for("right-asym"), "right-asymmetric") == 0,
          "right-asym -> right-asymmetric");
    CHECK(strcmp(wubu_raid5_layout_for("right-sym"), "right-symmetric") == 0,
          "right-sym -> right-symmetric");
    CHECK(strcmp(wubu_raid5_layout_for("la"), "left-asymmetric") == 0,
          "la -> left-asymmetric");
    CHECK(strcmp(wubu_raid5_layout_for("ls"), "left-symmetric") == 0,
          "ls -> left-symmetric");
    CHECK(strcmp(wubu_raid5_layout_for("ra"), "right-asymmetric") == 0,
          "ra -> right-asymmetric");
    CHECK(strcmp(wubu_raid5_layout_for("rs"), "right-symmetric") == 0,
          "rs -> right-symmetric");
    CHECK(strcmp(wubu_raid5_layout_for("zzz"), "left-symmetric") == 0,
          "zzz -> left-symmetric fallback");

    CHECK(strcmp(wubu_raid5_parity_for("P"), "P") == 0,
          "P -> P");
    CHECK(strcmp(wubu_raid5_parity_for("Q"), "Q") == 0,
          "Q -> Q");
    CHECK(strcmp(wubu_raid5_parity_for("P+Q"), "P+Q") == 0,
          "P+Q -> P+Q");
    CHECK(strcmp(wubu_raid5_parity_for("single"), "P") == 0,
          "single -> P");
    CHECK(strcmp(wubu_raid5_parity_for("double"), "P+Q") == 0,
          "double -> P+Q");
    CHECK(strcmp(wubu_raid5_parity_for("raid6"), "P+Q") == 0,
          "raid6 -> P+Q");
    CHECK(strcmp(wubu_raid5_parity_for("zzz"), "P") == 0,
          "zzz -> P fallback");

    char s[256];
    wubu_raid5_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "raid5 summary generated");

    printf("\n=== RAID5 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
