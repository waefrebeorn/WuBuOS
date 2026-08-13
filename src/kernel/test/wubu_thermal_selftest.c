/*
 * wubu_thermal_selftest.c -- verifies kernel-owned thermal routing.
 */
#include "wubu_thermal.h"
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
    printf("=== wubu_thermal_selftest ===\n\n");

    wubu_hw_detect();
    wubu_thermal_probe();

    printf("  hwmon=%d fan=%d zone=%d trip=%d fanctl=%d\n",
           wubu_thermal_hwmon(), wubu_thermal_fan(), wubu_thermal_zone(),
           wubu_thermal_trip(), wubu_thermal_fancontrol());

    /* Thermal mode routing. */
    CHECK(strcmp(wubu_thermal_mode_for("auto"), "auto") == 0,
          "auto -> auto");
    CHECK(strcmp(wubu_thermal_mode_for("manual"), "manual") == 0,
          "manual -> manual");
    CHECK(strcmp(wubu_thermal_mode_for("disabled"), "disabled") == 0,
          "disabled -> disabled");
    CHECK(strcmp(wubu_thermal_mode_for("unknown"), "auto") == 0,
          "unknown -> auto fallback");

    /* Fan curve routing. */
    CHECK(strcmp(wubu_thermal_curve_for("aggressive"), "aggressive") == 0,
          "aggressive -> aggressive");
    CHECK(strcmp(wubu_thermal_curve_for("quiet"), "quiet") == 0,
          "quiet -> quiet");
    CHECK(strcmp(wubu_thermal_curve_for("balanced"), "balanced") == 0,
          "balanced -> balanced");
    CHECK(strcmp(wubu_thermal_curve_for("unknown"), "balanced") == 0,
          "unknown -> balanced fallback");

    char s[256];
    wubu_thermal_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "thermal summary generated");

    printf("\n=== THERMAL TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
