/*
 * wubu_net_selftest.c -- verifies kernel-owned network driver routing.
 *
 * Tests the gaps closed:
 * 1. Wi-Fi chip detection (Intel/Realtek/MediaTek)
 * 2. Ethernet detection + 2.5GbE r8168 routing
 * 3. Power-save-disabling config (the latency fix)
 */
#include "wubu_net.h"
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
    printf("=== wubu_net_selftest ===\n\n");

    wubu_hw_detect();
    wubu_net_probe();

    printf("  wifi      = %d\n", wubu_net_has_wifi());
    printf("  wifi_drv  = %s\n", wubu_net_wifi_driver() ? wubu_net_wifi_driver() : "(none)");
    printf("  wifi_vendor = %04x\n", wubu_net_wifi_vendor());
    printf("  eth       = %d\n", wubu_net_has_eth());
    printf("  eth_drv   = %s\n", wubu_net_eth_driver() ? wubu_net_eth_driver() : "(none)");
    printf("  eth_2g5   = %d\n", wubu_net_has_2g5());
    const char *ps = wubu_net_power_save_disable();
    printf("  power_save_disable = %s\n", ps ? ps : "(none)");

    /* 1. Topology valid (WSL2 host owns network). */
    int wsl2 = wubu_hw_gpu_path() && strstr(wubu_hw_gpu_path(), "dxg");
    CHECK(wubu_net_has_wifi() || wubu_net_has_eth() || wsl2,
          "network topology valid (Wi-Fi/eth probed or WSL2-host-owned)");

    /* 2. Driver routing: if a Wi-Fi chip is detected, it has a driver. */
    if (wubu_net_has_wifi()) {
        CHECK(wubu_net_wifi_driver() != NULL,
              "Wi-Fi chip has a routed driver");
        CHECK(strstr(ps ? ps : "", "power_save") != NULL ||
              strstr(ps ? ps : "", "aspm") != NULL,
              "Wi-Fi power-save disabling config emitted");
    }

    /* 3. 2.5GbE uses r8168, not the broken r8169. */
    if (wubu_net_has_2g5()) {
        CHECK(strstr(wubu_net_eth_driver() ? wubu_net_eth_driver() : "", "r8168") != NULL,
              "2.5GbE NIC routed to r8168-dkms (not broken r8169)");
    }

    /* 4. Summary. */
    char sum[256] = "";
    wubu_net_summary(sum, sizeof(sum));
    printf("  summary: %s\n", sum);
    CHECK(sum[0] != '\0', "network summary generated");

    printf("\n=== NET TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
