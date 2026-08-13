/*
 * wubu_ddcci_selftest.c -- verifies kernel-owned DDC/CI routing.
 */
#include "wubu_ddcci.h"
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
    printf("=== wubu_ddcci_selftest ===\n\n");
    wubu_hw_detect();
    wubu_ddcci_probe();
    printf("  ddc=%d i2c=%d cec=%d edid=%d ctrl=%d\n",
           wubu_ddcci_present(), wubu_ddcci_i2c(), wubu_ddcci_cec(),
           wubu_ddcci_edid(), wubu_ddcci_ctrl());

    /* Command routing. */
    CHECK(strcmp(wubu_ddcci_cmd_for("brightness"), "0x10") == 0,
          "brightness -> 0x10");
    CHECK(strcmp(wubu_ddcci_cmd_for("vcb"), "0x10") == 0,
          "vcb -> 0x10");
    CHECK(strcmp(wubu_ddcci_cmd_for("contrast"), "0x12") == 0,
          "contrast -> 0x12");
    CHECK(strcmp(wubu_ddcci_cmd_for("osd"), "0x60") == 0,
          "osd -> 0x60");
    CHECK(strcmp(wubu_ddcci_cmd_for("power"), "0x6D") == 0,
          "power -> 0x6D");
    CHECK(strcmp(wubu_ddcci_cmd_for("input"), "0x60") == 0,
          "input -> 0x60");
    CHECK(strcmp(wubu_ddcci_cmd_for("zzz"), "0x10") == 0,
          "zzz -> 0x10 fallback");

    /* Bus routing. */
    CHECK(strcmp(wubu_ddcci_bus_for("ddc"), "ddc-bus") == 0,
          "ddc -> ddc-bus");
    CHECK(strcmp(wubu_ddcci_bus_for("i2c"), "i2c-bus") == 0,
          "i2c -> i2c-bus");
    CHECK(strcmp(wubu_ddcci_bus_for("cec"), "cec-bus") == 0,
          "cec -> cec-bus");
    CHECK(strcmp(wubu_ddcci_bus_for("zzz"), "ddc-bus") == 0,
          "zzz -> ddc-bus fallback");

    char s[256];
    wubu_ddcci_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "ddcci summary generated");

    printf("\n=== DDCCI TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
