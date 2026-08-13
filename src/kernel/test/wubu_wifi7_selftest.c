/*
 * wubu_wifi7_selftest.c -- verifies kernel-owned Wi-Fi 7 routing.
 */
#include "wubu_wifi7.h"
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
    printf("=== wubu_wifi7_selftest ===\n\n");

    wubu_hw_detect();
    wubu_wifi7_probe();

    printf("  present=%d mlo=%d 6ghz=%d 320mhz=%d drv=%s\n",
           wubu_wifi7_present(), wubu_wifi7_mlo(), wubu_wifi7_6ghz(),
           wubu_wifi7_320mhz(), wubu_wifi7_driver() ? wubu_wifi7_driver() : "none");

    /* Driver routing by vendor:device. */
    CHECK(strcmp(wubu_wifi7_driver_for(0x8086, 0x2725), "iwlwifi") == 0,
          "Intel BE200 -> iwlwifi");
    CHECK(strcmp(wubu_wifi7_driver_for(0x8086, 0x2726), "iwlwifi") == 0,
          "Intel BE201 -> iwlwifi");
    CHECK(strcmp(wubu_wifi7_driver_for(0x17CB, 0x1107), "ath12k_pci") == 0,
          "Qualcomm WCN7850 -> ath12k_pci");
    CHECK(strcmp(wubu_wifi7_driver_for(0x14C3, 0x0712), "mt7925e") == 0,
          "MediaTek MT7925 -> mt7925e");
    CHECK(strcmp(wubu_wifi7_driver_for(0x10EC, 0x8922), "rtw89") == 0,
          "Realtek RTL8922 -> rtw89");
    CHECK(wubu_wifi7_driver_for(0xFFFF, 0xFFFF) == NULL,
          "unknown -> NULL (no wifi7)");

    /* Present implies all Wi-Fi 7 features. */
    CHECK(!wubu_wifi7_present() || wubu_wifi7_mlo(),
          "wifi7 present -> MLO supported");
    CHECK(!wubu_wifi7_present() || wubu_wifi7_6ghz(),
          "wifi7 present -> 6GHz supported");

    char s[256];
    wubu_wifi7_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "wifi7 summary generated");

    printf("\n=== WIFI7 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
