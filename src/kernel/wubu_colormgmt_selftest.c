/*
 * wubu_colormgmt_selftest.c -- verifies kernel-owned color-mgmt routing.
 */
#include "wubu_colormgmt.h"
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
    printf("=== wubu_colormgmt_selftest ===\n\n");

    wubu_hw_detect();
    wubu_colormgmt_probe();

    printf("  ctm=%d gamma=%d degamma=%d csc=%d 3dlut=%d\n",
           wubu_colormgmt_ctm(), wubu_colormgmt_gamma(),
           wubu_colormgmt_degamma(), wubu_colormgmt_csc(),
           wubu_colormgmt_3dlut());

    /* LUT routing. */
    CHECK(strcmp(wubu_colormgmt_lut_for("gamma"), "gamma-lut") == 0,
          "gamma -> gamma-lut");
    CHECK(strcmp(wubu_colormgmt_lut_for("degamma"), "degamma-lut") == 0,
          "degamma -> degamma-lut");
    CHECK(strcmp(wubu_colormgmt_lut_for("3d"), "3d-lut") == 0,
          "3d -> 3d-lut");
    CHECK(strcmp(wubu_colormgmt_lut_for("ctm"), "ctm") == 0,
          "ctm -> ctm");
    CHECK(strcmp(wubu_colormgmt_lut_for("unknown"), "lut") == 0,
          "unknown -> lut fallback");

    /* CSC routing. */
    CHECK(strcmp(wubu_colormgmt_csc_for("bt709"), "bt709") == 0,
          "bt709 -> bt709");
    CHECK(strcmp(wubu_colormgmt_csc_for("bt2020"), "bt2020") == 0,
          "bt2020 -> bt2020");
    CHECK(strcmp(wubu_colormgmt_csc_for("rgb"), "rgb") == 0,
          "rgb -> rgb");
    CHECK(strcmp(wubu_colormgmt_csc_for("ycbcr"), "ycbcr") == 0,
          "ycbcr -> ycbcr");
    CHECK(strcmp(wubu_colormgmt_csc_for("unknown"), "csc") == 0,
          "unknown -> csc fallback");

    char s[256];
    wubu_colormgmt_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "colormgmt summary generated");

    printf("\n=== COLORMGMT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
