/*
 * wubu_bt_selftest.c -- verifies kernel-owned Bluetooth routing.
 */
#include "wubu_bt.h"
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
    printf("=== wubu_bt_selftest ===\n\n");

    wubu_hw_detect();
    wubu_bt_probe();

    printf("  present=%d usb=%d pci=%d uart=%d le_audio=%d drv=%s\n",
           wubu_bt_present(), wubu_bt_usb(), wubu_bt_pci(), wubu_bt_uart(),
           wubu_bt_le_audio(), wubu_bt_driver() ? wubu_bt_driver() : "none");

    /* Controller routing is always consistent. */
    CHECK(strcmp(wubu_bt_controller_driver("intel"), "btintel") == 0,
          "intel -> btintel");
    CHECK(strcmp(wubu_bt_controller_driver("broadcom"), "btbcm") == 0,
          "broadcom -> btbcm");
    CHECK(strcmp(wubu_bt_controller_driver("realtek"), "btrtl") == 0,
          "realtek -> btrtl");
    CHECK(strcmp(wubu_bt_controller_driver("mediatek"), "btmtk") == 0,
          "mediatek -> btmtk");
    CHECK(strcmp(wubu_bt_controller_driver("unknown"), "btusb") == 0,
          "unknown -> btusb fallback");

    char s[256];
    wubu_bt_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "bt summary generated");

    printf("\n=== BT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
