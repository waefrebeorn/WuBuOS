/*
 * wubu_ptp_sync_selftest.c -- verifies kernel-owned PTP routing.
 */
#include "wubu_ptp_sync.h"
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
    printf("=== wubu_ptp_sync_selftest ===\n\n");

    wubu_hw_detect();
    wubu_ptp_sync_probe();

    printf("  phc=%d ptp4l=%d phc2sys=%d hwts=%d synced=%d\n",
           wubu_ptp_sync_phc(), wubu_ptp_sync_ptp4l(),
           wubu_ptp_sync_phc2sys(), wubu_ptp_sync_hwts(),
           wubu_ptp_sync_synced());

    /* PTP role routing. */
    CHECK(strcmp(wubu_ptp_sync_role_for("master"), "master") == 0,
          "master -> master");
    CHECK(strcmp(wubu_ptp_sync_role_for("slave"), "slave") == 0,
          "slave -> slave");
    CHECK(strcmp(wubu_ptp_sync_role_for("transparent"), "transparent") == 0,
          "transparent -> transparent");
    CHECK(strcmp(wubu_ptp_sync_role_for("boundary"), "boundary") == 0,
          "boundary -> boundary");
    CHECK(strcmp(wubu_ptp_sync_role_for("unknown"), "ptp") == 0,
          "unknown -> ptp fallback");

    /* NIC PTP routing. */
    CHECK(strcmp(wubu_ptp_sync_nic_for("igc"), "igc-phc") == 0,
          "igc -> igc-phc");
    CHECK(strcmp(wubu_ptp_sync_nic_for("ixgbe"), "ixgbe-phc") == 0,
          "ixgbe -> ixgbe-phc");
    CHECK(strcmp(wubu_ptp_sync_nic_for("mlx5"), "mlx5-phc") == 0,
          "mlx5 -> mlx5-phc");
    CHECK(strcmp(wubu_ptp_sync_nic_for("unknown"), "phc") == 0,
          "unknown -> phc fallback");

    char s[256];
    wubu_ptp_sync_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "ptpsync summary generated");

    printf("\n=== PTPSYNC TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
