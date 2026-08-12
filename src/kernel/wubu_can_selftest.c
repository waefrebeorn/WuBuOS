/*
 * wubu_can_selftest.c -- verifies kernel-owned CAN bus routing.
 */
#include "wubu_can.h"
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
    printf("=== wubu_can_selftest ===\n\n");

    wubu_hw_detect();
    wubu_can_probe();

    printf("  present=%d usb=%d spi=%d drv=%s\n",
           wubu_can_present(), wubu_can_has_usb(), wubu_can_has_spi(),
           wubu_can_driver() ? wubu_can_driver() : "none");

    /* Controller routing is always consistent. */
    CHECK(strcmp(wubu_can_controller_driver("mcp2515"), "mcp251x") == 0,
          "mcp2515 -> mcp251x");
    CHECK(strcmp(wubu_can_controller_driver("mcp2518"), "mcp251xfd") == 0,
          "mcp2518 -> mcp251xfd");
    CHECK(strcmp(wubu_can_controller_driver("peak"), "peak_usb") == 0,
          "peak -> peak_usb");
    CHECK(strcmp(wubu_can_controller_driver("esd"), "esd_usb2") == 0,
          "esd -> esd_usb2");
    CHECK(strcmp(wubu_can_controller_driver("gs_usb"), "gs_usb") == 0,
          "gs_usb -> gs_usb");
    CHECK(strcmp(wubu_can_controller_driver("elm327"), "can327") == 0,
          "elm327 -> can327 (OBD-II)");
    CHECK(strcmp(wubu_can_controller_driver("sja1000"), "sja1000") == 0,
          "sja1000 -> sja1000");
    CHECK(wubu_can_controller_driver("unknown") != NULL,
          "unknown controller -> socketcan fallback");

    char s[256];
    wubu_can_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "CAN summary generated");

    printf("\n=== CAN TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
