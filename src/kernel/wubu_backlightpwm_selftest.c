/*
 * wubu_backlightpwm_selftest.c -- verifies kernel-owned backlight routing.
 */
#include "wubu_backlightpwm.h"
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
    printf("=== wubu_backlightpwm_selftest ===\n\n");

    wubu_hw_detect();
    wubu_backlightpwm_probe();

    printf("  bl=%d pwm=%d sysfs=%d acpi=%d intel=%d\n",
           wubu_backlightpwm_present(), wubu_backlightpwm_pwm(),
           wubu_backlightpwm_sysfs(), wubu_backlightpwm_acpi(),
           wubu_backlightpwm_intel());

    /* Type routing. */
    CHECK(strcmp(wubu_backlightpwm_type_for("sysfs"), "sysfs") == 0,
          "sysfs -> sysfs");
    CHECK(strcmp(wubu_backlightpwm_type_for("acpi"), "acpi-video") == 0,
          "acpi -> acpi-video");
    CHECK(strcmp(wubu_backlightpwm_type_for("intel"), "intel-backlight") == 0,
          "intel -> intel-backlight");
    CHECK(strcmp(wubu_backlightpwm_type_for("amd"), "amdgpu-bl") == 0,
          "amd -> amdgpu-bl");
    CHECK(strcmp(wubu_backlightpwm_type_for("pwm"), "pwm-raw") == 0,
          "pwm -> pwm-raw");
    CHECK(strcmp(wubu_backlightpwm_type_for("unknown"), "backlight") == 0,
          "unknown -> backlight fallback");

    /* Brightness routing. */
    CHECK(strcmp(wubu_backlightpwm_brightness_for("max"), "max") == 0,
          "max -> max");
    CHECK(strcmp(wubu_backlightpwm_brightness_for("min"), "min") == 0,
          "min -> min");
    CHECK(strcmp(wubu_backlightpwm_brightness_for("off"), "min") == 0,
          "off -> min");
    CHECK(strcmp(wubu_backlightpwm_brightness_for("50"), "50") == 0,
          "50 -> 50");
    CHECK(strcmp(wubu_backlightpwm_brightness_for("half"), "50") == 0,
          "half -> 50");
    CHECK(strcmp(wubu_backlightpwm_brightness_for("unknown"), "auto") == 0,
          "unknown -> auto fallback");

    char s[256];
    wubu_backlightpwm_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "backlight summary generated");

    printf("\n=== BACKLIGHTPWM TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
