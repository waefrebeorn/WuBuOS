/*
 * wubu_overclock_selftest.c -- verifies kernel-owned overclock routing.
 */
#include "wubu_overclock.h"
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
    printf("=== wubu_overclock_selftest ===\n\n");
    wubu_hw_detect();
    wubu_overclock_probe();
    printf("  oc=%d od=%d sysfs=%d core=%d mem=%d\n",
           wubu_overclock_present(), wubu_overclock_od(), wubu_overclock_sysfs(),
           wubu_overclock_core(), wubu_overclock_mem());

    /* Clock routing. */
    CHECK(strcmp(wubu_overclock_clk_for("core"), "core") == 0,
          "core -> core");
    CHECK(strcmp(wubu_overclock_clk_for("sclk"), "core") == 0,
          "sclk -> core");
    CHECK(strcmp(wubu_overclock_clk_for("mem"), "memory") == 0,
          "mem -> memory");
    CHECK(strcmp(wubu_overclock_clk_for("mclk"), "memory") == 0,
          "mclk -> memory");
    CHECK(strcmp(wubu_overclock_clk_for("vddc"), "vddc") == 0,
          "vddc -> vddc");
    CHECK(strcmp(wubu_overclock_clk_for("soc"), "soc") == 0,
          "soc -> soc");
    CHECK(strcmp(wubu_overclock_clk_for("zzz"), "core") == 0,
          "zzz -> core fallback");

    /* State routing. */
    CHECK(strcmp(wubu_overclock_state_for("boot"), "boot") == 0,
          "boot -> boot");
    CHECK(strcmp(wubu_overclock_state_for("stable"), "stable") == 0,
          "stable -> stable");
    CHECK(strcmp(wubu_overclock_state_for("oc"), "overclock") == 0,
          "oc -> overclock");
    CHECK(strcmp(wubu_overclock_state_for("stock"), "stock") == 0,
          "stock -> stock");
    CHECK(strcmp(wubu_overclock_state_for("zzz"), "stock") == 0,
          "zzz -> stock fallback");

    char s[256];
    wubu_overclock_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "overclock summary generated");

    printf("\n=== OVERCLOCK TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
