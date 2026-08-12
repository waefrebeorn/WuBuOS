/*
 * wubu_peripheral_selftest.c -- verifies kernel-owned peripheral routing.
 */
#include "wubu_peripheral.h"
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
    printf("=== wubu_peripheral_selftest ===\n\n");

    wubu_hw_detect();
    wubu_peripheral_probe();

    printf("  serial=%d par=%d gpio=%d hwmon=%d smbus=%d\n",
           wubu_peripheral_has_serial(), wubu_peripheral_has_parallel(),
           wubu_peripheral_has_gpio(), wubu_peripheral_has_hwmon(),
           wubu_peripheral_has_smbus());

    /* Driver routing is always consistent. */
    CHECK(wubu_peripheral_serial_driver() != NULL, "serial driver resolved");
    CHECK(strcmp(wubu_peripheral_parallel_driver(), "parport_pc") == 0,
          "parallel -> parport_pc");
    CHECK(strcmp(wubu_peripheral_gpio_driver(), "gpiolib") == 0,
          "GPIO -> gpiolib");

    char s[256];
    wubu_peripheral_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "peripheral summary generated");

    printf("\n=== PERIPHERAL TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
