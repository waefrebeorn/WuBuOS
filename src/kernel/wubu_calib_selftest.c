/*
 * wubu_calib_selftest.c -- verifies kernel-owned calibration routing.
 */
#include "wubu_calib.h"
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
    printf("=== wubu_calib_selftest ===\n\n");

    wubu_hw_detect();
    wubu_calib_probe();

    printf("  drm=%d ddc=%d gamma=%d colord=%d icc=%d\n",
           wubu_calib_drm_color(), wubu_calib_ddc(), wubu_calib_gamma(),
           wubu_calib_colord(), wubu_calib_icc());

    /* Calibration driver routing is always consistent. */
    CHECK(strcmp(wubu_calib_driver_for("drm"), "drm-color") == 0,
          "drm -> drm-color");
    CHECK(strcmp(wubu_calib_driver_for("ddc"), "ddc-ci") == 0,
          "ddc -> ddc-ci");
    CHECK(strcmp(wubu_calib_driver_for("gamma"), "gamma-lut") == 0,
          "gamma -> gamma-lut");
    CHECK(strcmp(wubu_calib_driver_for("icc"), "icc-profile") == 0,
          "icc -> icc-profile");
    CHECK(strcmp(wubu_calib_driver_for("colord"), "colord") == 0,
          "colord -> colord");
    CHECK(strcmp(wubu_calib_driver_for("unknown"), "calib-core") == 0,
          "unknown -> calib-core fallback");

    char s[256];
    wubu_calib_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "calib summary generated");

    printf("\n=== CALIB TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
