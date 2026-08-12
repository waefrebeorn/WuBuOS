/*
 * wubu_pm_selftest.c -- verifies kernel-owned power-mode routing.
 */
#include "wubu_pm.h"
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
    printf("=== wubu_pm_selftest ===\n\n");

    wubu_hw_detect();
    wubu_pm_probe();

    printf("  s0ix=%d s3=%d s4=%d runtime=%d cpuidle=%d\n",
           wubu_pm_s0ix(), wubu_pm_s3(), wubu_pm_s4(),
           wubu_pm_runtime(), wubu_pm_cpuidle());

    /* Sleep-state routing. */
    CHECK(strcmp(wubu_pm_sleep_state(1<<0), "s2idle") == 0,
          "s0ix -> s2idle");
    CHECK(strcmp(wubu_pm_sleep_state(1<<1), "s3") == 0,
          "s3 -> s3");
    CHECK(strcmp(wubu_pm_sleep_state(1<<2), "s4") == 0,
          "s4 -> s4");
    CHECK(strcmp(wubu_pm_sleep_state(0), "none") == 0,
          "none -> none");

    /* CPU idle routing. */
    CHECK(strcmp(wubu_pm_idle_for("intel"), "intel_idle") == 0,
          "intel -> intel_idle");
    CHECK(strcmp(wubu_pm_idle_for("amd"), "acpi_idle") == 0,
          "amd -> acpi_idle");
    CHECK(strcmp(wubu_pm_idle_for("arm"), "cpuidle-arm") == 0,
          "arm -> cpuidle-arm");
    CHECK(strcmp(wubu_pm_idle_for("unknown"), "cpuidle") == 0,
          "unknown -> cpuidle fallback");

    char s[256];
    wubu_pm_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "pm summary generated");

    printf("\n=== PM TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
