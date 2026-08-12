/*
 * wubu_phy_selftest.c -- verifies kernel-owned Ethernet PHY routing.
 */
#include "wubu_phy.h"
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
    printf("=== wubu_phy_selftest ===\n\n");

    wubu_hw_detect();
    wubu_phy_probe();

    printf("  present=%d mdio=%d link=%d drv=%s\n",
           wubu_phy_present(), wubu_phy_has_mdio(), wubu_phy_link_up(),
           wubu_phy_driver() ? wubu_phy_driver() : "none");

    /* PHY driver routing is always consistent. */
    CHECK(strcmp(wubu_phy_driver_for("marvell"), "marvell-phy") == 0,
          "marvell -> marvell-phy");
    CHECK(strcmp(wubu_phy_driver_for("broadcom"), "broadcom-phy") == 0,
          "broadcom -> broadcom-phy");
    CHECK(strcmp(wubu_phy_driver_for("micrel"), "micrel-phy") == 0,
          "micrel -> micrel-phy");
    CHECK(strcmp(wubu_phy_driver_for("realtek"), "realtek-phy") == 0,
          "realtek -> realtek-phy");
    CHECK(strcmp(wubu_phy_driver_for("dp83867"), "ti-phy") == 0,
          "dp83867 -> ti-phy");
    CHECK(strcmp(wubu_phy_driver_for("yt85"), "motorcomm") == 0,
          "yt85 -> motorcomm");
    CHECK(strcmp(wubu_phy_driver_for("at803"), "at803x") == 0,
          "at803 -> at803x");
    CHECK(strcmp(wubu_phy_driver_for("unknown"), "genphy") == 0,
          "unknown -> genphy fallback");

    char s[256];
    wubu_phy_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "phy summary generated");

    printf("\n=== PHY TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
