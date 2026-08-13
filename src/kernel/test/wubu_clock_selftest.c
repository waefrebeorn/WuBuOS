/*
 * wubu_clock_selftest.c -- verifies kernel-owned clock/thermal routing.
 */
#include "wubu_clock.h"
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
    printf("=== wubu_clock_selftest ===\n\n");

    wubu_hw_detect();
    wubu_clock_probe();

    printf("  rtc=%d thermal=%d zones=%d cooling=%d\n",
           wubu_clock_has_rtc(), wubu_clock_has_thermal(),
           wubu_clock_thermal_zones(), wubu_clock_has_cooling());

    /* RTC driver routing. */
    CHECK(strcmp(wubu_clock_rtc_for("ds1307"), "ds1307") == 0,
          "ds1307 -> ds1307");
    CHECK(strcmp(wubu_clock_rtc_for("ds3231"), "ds3231") == 0,
          "ds3231 -> ds3231");
    CHECK(strcmp(wubu_clock_rtc_for("pcf8523"), "pcf8523") == 0,
          "pcf8523 -> pcf8523");
    CHECK(strcmp(wubu_clock_rtc_for("pcf2127"), "pcf2127") == 0,
          "pcf2127 -> pcf2127");
    CHECK(strcmp(wubu_clock_rtc_for("m41t80"), "m41t80") == 0,
          "m41t80 -> m41t80");
    CHECK(strcmp(wubu_clock_rtc_for("cmos"), "rtc-cmos") == 0,
          "cmos -> rtc-cmos");
    CHECK(strcmp(wubu_clock_rtc_for("efi"), "rtc-efi") == 0,
          "efi -> rtc-efi");
    CHECK(strcmp(wubu_clock_rtc_for("unknown"), "rtc-core") == 0,
          "unknown rtc -> rtc-core");

    /* Thermal routing. */
    CHECK(strcmp(wubu_clock_thermal_for("int340"), "int340x") == 0,
          "int340 -> int340x");
    CHECK(strcmp(wubu_clock_thermal_for("coretemp"), "coretemp") == 0,
          "coretemp -> coretemp");
    CHECK(strcmp(wubu_clock_thermal_for("rockchip"), "rockchip_thermal") == 0,
          "rockchip -> rockchip_thermal");
    CHECK(strcmp(wubu_clock_thermal_for("exynos"), "exynos_tmu") == 0,
          "exynos -> exynos_tmu");
    CHECK(strcmp(wubu_clock_thermal_for("acpitz"), "acpitz") == 0,
          "acpitz -> acpitz");
    CHECK(strcmp(wubu_clock_thermal_for("unknown"), "thermal-core") == 0,
          "unknown thermal -> thermal-core");

    char s[256];
    wubu_clock_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "clock summary generated");

    printf("\n=== CLOCK TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
