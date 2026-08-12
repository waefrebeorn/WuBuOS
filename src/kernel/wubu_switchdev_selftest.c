/*
 * wubu_switchdev_selftest.c -- verifies kernel-owned switch fabric routing.
 */
#include "wubu_switchdev.h"
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
    printf("=== wubu_switchdev_selftest ===\n\n");

    wubu_hw_detect();
    wubu_switchdev_probe();

    printf("  present=%d dsa=%d asic=%d drv=%s\n",
           wubu_switchdev_present(), wubu_switchdev_has_dsa(),
           wubu_switchdev_has_asic(),
           wubu_switchdev_driver() ? wubu_switchdev_driver() : "none");

    /* Switch driver routing is always consistent. */
    CHECK(strcmp(wubu_switchdev_driver_for("mv88e6"), "mv88e6xxx") == 0,
          "mv88e6 -> mv88e6xxx");
    CHECK(strcmp(wubu_switchdev_driver_for("ksz"), "ksz_common") == 0,
          "ksz -> ksz_common");
    CHECK(strcmp(wubu_switchdev_driver_for("rtl8366"), "rtl8366rb") == 0,
          "rtl8366 -> rtl8366rb");
    CHECK(strcmp(wubu_switchdev_driver_for("mt7530"), "mt7530") == 0,
          "mt7530 -> mt7530");
    CHECK(strcmp(wubu_switchdev_driver_for("qca8k"), "qca8k") == 0,
          "qca8k -> qca8k");
    CHECK(strcmp(wubu_switchdev_driver_for("spectrum"), "mlxsw_spectrum") == 0,
          "spectrum -> mlxsw_spectrum");
    CHECK(strcmp(wubu_switchdev_driver_for("b53"), "b53") == 0,
          "b53 -> b53");
    CHECK(strcmp(wubu_switchdev_driver_for("ocelot"), "ocelot_switch") == 0,
          "ocelot -> ocelot_switch");
    CHECK(strcmp(wubu_switchdev_driver_for("unknown"), "dsa") == 0,
          "unknown -> dsa fallback");

    char s[256];
    wubu_switchdev_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "switchdev summary generated");

    printf("\n=== SWITCHDEV TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
