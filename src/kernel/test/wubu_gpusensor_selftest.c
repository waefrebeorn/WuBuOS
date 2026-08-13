/*
 * wubu_gpusensor_selftest.c -- verifies kernel-owned GPU-sensor routing.
 */
#include "wubu_gpusensor.h"
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
    printf("=== wubu_gpusensor_selftest ===\n\n");

    wubu_hw_detect();
    wubu_gpusensor_probe();

    printf("  hwmon=%d temp=%d fan=%d power=%d curve=%d\n",
           wubu_gpusensor_hwmon(), wubu_gpusensor_temp(),
           wubu_gpusensor_fan(), wubu_gpusensor_power(),
           wubu_gpusensor_curve());

    /* Fan curve routing. */
    CHECK(strcmp(wubu_gpusensor_curve_for("aggressive"), "aggressive") == 0,
          "aggressive -> aggressive");
    CHECK(strcmp(wubu_gpusensor_curve_for("quiet"), "quiet") == 0,
          "quiet -> quiet");
    CHECK(strcmp(wubu_gpusensor_curve_for("balanced"), "balanced") == 0,
          "balanced -> balanced");
    CHECK(strcmp(wubu_gpusensor_curve_for("zero"), "zero-rpm") == 0,
          "zero -> zero-rpm");
    CHECK(strcmp(wubu_gpusensor_curve_for("unknown"), "balanced") == 0,
          "unknown -> balanced fallback");

    /* Metric routing. */
    CHECK(strcmp(wubu_gpusensor_metric_for("temp"), "temperature") == 0,
          "temp -> temperature");
    CHECK(strcmp(wubu_gpusensor_metric_for("fan"), "fan-speed") == 0,
          "fan -> fan-speed");
    CHECK(strcmp(wubu_gpusensor_metric_for("power"), "power-watts") == 0,
          "power -> power-watts");
    CHECK(strcmp(wubu_gpusensor_metric_for("unknown"), "gpu") == 0,
          "unknown -> gpu fallback");

    char s[256];
    wubu_gpusensor_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "gpusensor summary generated");

    printf("\n=== GPUSENSOR TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
