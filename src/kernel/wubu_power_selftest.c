/*
 * wubu_power_selftest.c -- verifies kernel-owned CPU/power routing.
 */
#include "wubu_power.h"
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
    printf("=== wubu_power_selftest ===\n\n");

    wubu_hw_detect();
    wubu_power_probe();

    printf("  cpu=%d cores=%d cpufreq=%s gov=%s bat=%d therm=%d fan=%d\n",
           wubu_power_cpu_vendor(), wubu_power_ncores(),
           wubu_power_cpufreq_driver() ? wubu_power_cpufreq_driver() : "(none)",
           wubu_power_governor() ? wubu_power_governor() : "(none)",
           wubu_power_has_battery(), wubu_power_has_thermal(),
           wubu_power_has_fan());

    /* CPU vendor + cores always resolvable on real hardware. */
    CHECK(wubu_power_cpu_vendor() >= 0, "CPU vendor resolved");
    CHECK(wubu_power_ncores() >= 1, "core count >= 1");
    CHECK(wubu_power_cpufreq_driver() != NULL, "cpufreq driver resolved");
    CHECK(wubu_power_governor() != NULL, "governor resolved");

    /* cpufreq driver is sane for the detected vendor. */
    const char *drv = wubu_power_cpufreq_driver();
    CHECK(strcmp(drv, "intel_pstate") == 0 || strcmp(drv, "amd_pstate") == 0
          || strcmp(drv, "cpufreq-dt") == 0 || strcmp(drv, "acpi-cpufreq") == 0,
          "cpufreq driver is a known module");

    /* C-state cap either present (x86) or NULL (arm). */
    const char *cs = wubu_power_cstate_cap();
    CHECK(cs == NULL || strstr(cs, "max_cstate") != NULL,
          "C-state cap param references max_cstate");

    char s[256];
    wubu_power_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "power summary generated");

    printf("\n=== POWER TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
