/*
 * wubu_multigig_selftest.c -- verifies kernel-owned multi-gig routing.
 */
#include "wubu_multigig.h"
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
    printf("=== wubu_multigig_selftest ===\n\n");

    wubu_hw_detect();
    wubu_multigig_probe();

    printf("  present=%d 2g5=%d 5g=%d 10g=%d\n",
           wubu_multigig_present(), wubu_multigig_2g5(),
           wubu_multigig_5g(), wubu_multigig_10g());

    /* Multi-gig driver routing is always consistent. */
    CHECK(strcmp(wubu_multigig_driver_for("r8125"), "r8125") == 0,
          "r8125 -> r8125");
    CHECK(strcmp(wubu_multigig_driver_for("rtl8126"), "r8125") == 0,
          "rtl8126 -> r8125");
    CHECK(strcmp(wubu_multigig_driver_for("aqr107"), "aquantia") == 0,
          "aqr107 -> aquantia");
    CHECK(strcmp(wubu_multigig_driver_for("atlantic"), "atlantic") == 0,
          "atlantic -> atlantic");
    CHECK(strcmp(wubu_multigig_driver_for("marvell"), "m88x3310") == 0,
          "marvell -> m88x3310");
    CHECK(strcmp(wubu_multigig_driver_for("bcm848"), "bcm84881") == 0,
          "bcm848 -> bcm84881");
    CHECK(strcmp(wubu_multigig_driver_for("x550"), "ixgbe") == 0,
          "x550 -> ixgbe");
    CHECK(strcmp(wubu_multigig_driver_for("unknown"), "genphy") == 0,
          "unknown -> genphy fallback");

    char s[256];
    wubu_multigig_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "multigig summary generated");

    printf("\n=== MULTIGIG TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
