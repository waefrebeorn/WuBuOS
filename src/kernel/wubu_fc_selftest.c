/*
 * wubu_fc_selftest.c -- verifies kernel-owned ethernet-FC routing.
 */
#include "wubu_fc.h"
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
    printf("=== wubu_fc_selftest ===\n\n");

    wubu_hw_detect();
    wubu_fc_probe();

    printf("  fc=%d pause=%d pfc=%d autoneg=%d ethtool=%d\n",
           wubu_fc_supported(), wubu_fc_pause(), wubu_fc_pfc(),
           wubu_fc_autoneg(), wubu_fc_ethtool());

    /* FC mode routing. */
    CHECK(strcmp(wubu_fc_mode_for("rx"), "rx-pause") == 0,
          "rx -> rx-pause");
    CHECK(strcmp(wubu_fc_mode_for("tx"), "tx-pause") == 0,
          "tx -> tx-pause");
    CHECK(strcmp(wubu_fc_mode_for("both"), "both-pause") == 0,
          "both -> both-pause");
    CHECK(strcmp(wubu_fc_mode_for("pfc"), "pfc") == 0,
          "pfc -> pfc");
    CHECK(strcmp(wubu_fc_mode_for("unknown"), "fc") == 0,
          "unknown -> fc fallback");

    /* Autoneg routing. */
    CHECK(strcmp(wubu_fc_autoneg_for("on"), "on") == 0,
          "on -> on");
    CHECK(strcmp(wubu_fc_autoneg_for("off"), "off") == 0,
          "off -> off");
    CHECK(strcmp(wubu_fc_autoneg_for("unknown"), "auto") == 0,
          "unknown -> auto fallback");

    char s[256];
    wubu_fc_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "fc summary generated");

    printf("\n=== FC TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
