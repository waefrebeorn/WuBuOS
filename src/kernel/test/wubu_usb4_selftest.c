/*
 * wubu_usb4_selftest.c -- verifies kernel-owned USB4/TB routing.
 */
#include "wubu_usb4.h"
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
    printf("=== wubu_usb4_selftest ===\n\n");

    wubu_hw_detect();
    wubu_usb4_probe();

    printf("  tb=%d usb4=%d bolt=%d secure=%d domains=%d\n",
           wubu_usb4_tb(), wubu_usb4_usb4(), wubu_usb4_bolt(),
           wubu_usb4_secure(), wubu_usb4_domains());

    /* Driver routing is always consistent. */
    CHECK(strcmp(wubu_usb4_driver_for("intel"), "thunderbolt") == 0,
          "intel -> thunderbolt");
    CHECK(strcmp(wubu_usb4_driver_for("amd"), "usb4") == 0,
          "amd -> usb4");
    CHECK(strcmp(wubu_usb4_driver_for("apple"), "thunderbolt") == 0,
          "apple -> thunderbolt");
    CHECK(strcmp(wubu_usb4_driver_for("titan"), "thunderbolt") == 0,
          "titan ridge -> thunderbolt");
    CHECK(strcmp(wubu_usb4_driver_for("unknown"), "thunderbolt") == 0,
          "unknown -> thunderbolt fallback");

    char s[256];
    wubu_usb4_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "usb4 summary generated");

    printf("\n=== USB4 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
