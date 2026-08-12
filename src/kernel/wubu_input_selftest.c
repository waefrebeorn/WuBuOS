/*
 * wubu_input_selftest.c -- verifies kernel-owned input driver routing.
 *
 * Tests the gaps closed:
 * 1. Controller → driver routing (xpad vs xpadneo vs hid-playstation)
 * 2. BLE controller detection → xpadneo/hid-playstation routing
 * 3. Mouse polling rate tuning (cap 1000Hz)
 */
#include "wubu_input.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

static void test_xbox_usb(void)
{
    printf("  -- Xbox Series (USB) --\n");
    wubu_input_set_controller(0x045E, 0x0B12);
    const char *drv = wubu_input_controller_driver();
    printf("    driver=%s name=%s ble=%d\n",
           drv ? drv : "(null)", wubu_input_controller_name(),
           wubu_input_uses_ble());
    CHECK(strcmp(drv ? drv : "", "xpad") == 0,
          "Xbox Series USB routes to xpad");
    CHECK(!wubu_input_uses_ble(), "USB Xbox is not BLE");
    CHECK(wubu_input_routing_hint() == NULL,
          "USB Xbox needs no out-of-tree driver");
}

static void test_xbox_ble(void)
{
    printf("  -- Xbox wireless (BLE) --\n");
    wubu_input_set_controller(0x045E, 0x0B13);
    const char *drv = wubu_input_controller_driver();
    printf("    driver=%s name=%s ble=%d\n",
           drv ? drv : "(null)", wubu_input_controller_name(),
           wubu_input_uses_ble());
    CHECK(strcmp(drv ? drv : "", "xpadneo") == 0,
          "Xbox wireless BLE routes to xpadneo");
    CHECK(wubu_input_uses_ble(), "Xbox wireless is BLE");
    CHECK(wubu_input_routing_hint() != NULL,
          "BLE controller surfaces routing hint");
}

static void test_dualsense(void)
{
    printf("  -- DualSense --\n");
    wubu_input_set_controller(0x054C, 0x0CE6);
    const char *drv = wubu_input_controller_driver();
    printf("    driver=%s name=%s\n",
           drv ? drv : "(null)", wubu_input_controller_name());
    CHECK(strcmp(drv ? drv : "", "hid-playstation") == 0,
          "DualSense routes to hid-playstation");
}

static void test_poll_hz(void)
{
    printf("  -- mouse polling --\n");
    wubu_input_set_poll_hz(2000);
    printf("    detected=%dHz safe=%dHz\n",
           wubu_input_poll_hz(), wubu_input_safe_poll_hz());
    CHECK(wubu_input_safe_poll_hz() == 1000,
          "2000Hz capped to safe 1000Hz");
    wubu_input_set_poll_hz(500);
    CHECK(wubu_input_safe_poll_hz() == 500,
          "500Hz kept as-is (below cap)");
}

int main(void)
{
    printf("=== wubu_input_selftest ===\n\n");

    wubu_hw_detect();
    wubu_input_probe();

    test_xbox_usb();
    test_xbox_ble();
    test_dualsense();
    test_poll_hz();

    /* Summary. */
    wubu_input_set_controller(0x045E, 0x0B12);
    char sum[256] = "";
    wubu_input_summary(sum, sizeof(sum));
    printf("  summary: %s\n", sum);
    CHECK(sum[0] != '\0', "input summary generated");

    printf("\n=== INPUT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
