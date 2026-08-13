/*
 * wubu_ptp_selftest.c -- verifies kernel-owned PTP/TSN + haptics routing.
 */
#include "wubu_ptp.h"
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
    printf("=== wubu_ptp_selftest ===\n\n");

    wubu_hw_detect();
    wubu_ptp_probe();

    printf("  ptp=%d phc=%d tsn=%d haptic=%d\n",
           wubu_ptp_present(), wubu_ptp_phc_clocks(),
           wubu_ptp_has_tsn(), wubu_ptp_has_haptic());

    /* PTP driver routing. */
    CHECK(strcmp(wubu_ptp_driver_for("igb"), "igb-ptp") == 0,
          "igb -> igb-ptp");
    CHECK(strcmp(wubu_ptp_driver_for("i210"), "igb-ptp") == 0,
          "i210 -> igb-ptp");
    CHECK(strcmp(wubu_ptp_driver_for("ixgbe"), "ixgbe-ptp") == 0,
          "ixgbe -> ixgbe-ptp");
    CHECK(strcmp(wubu_ptp_driver_for("ice"), "ice-ptp") == 0,
          "ice -> ice-ptp");
    CHECK(strcmp(wubu_ptp_driver_for("mlx5"), "mlx5-ptp") == 0,
          "mlx5 -> mlx5-ptp");
    CHECK(strcmp(wubu_ptp_driver_for("tsn"), "tsn") == 0,
          "tsn -> tsn");
    CHECK(strcmp(wubu_ptp_driver_for("unknown"), "ptp") == 0,
          "unknown -> ptp fallback");

    /* Haptics routing. */
    CHECK(strcmp(wubu_ptp_haptic_for("xbox"), "xpad") == 0,
          "xbox -> xpad");
    CHECK(strcmp(wubu_ptp_haptic_for("sony"), "sony") == 0,
          "sony -> sony");
    CHECK(strcmp(wubu_ptp_haptic_for("playstation"), "sony") == 0,
          "playstation -> sony");
    CHECK(strcmp(wubu_ptp_haptic_for("unknown"), "ff-memless") == 0,
          "unknown -> ff-memless fallback");

    char s[256];
    wubu_ptp_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "ptp summary generated");

    printf("\n=== PTP TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
