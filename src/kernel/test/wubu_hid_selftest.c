/*
 * wubu_hid_selftest.c -- verifies kernel-owned HID routing.
 */
#include "wubu_hid.h"
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
    printf("=== wubu_hid_selftest ===\n\n");

    wubu_hw_detect();
    wubu_hid_probe();

    printf("  hid=%d generic=%d mt=%d ff=%d vendor=%d\n",
           wubu_hid_present(), wubu_hid_generic(), wubu_hid_multitouch(),
           wubu_hid_ff(), wubu_hid_vendor());

    /* HID driver routing is always consistent. */
    CHECK(strcmp(wubu_hid_driver_for("logitech"), "hid-logitech-dj") == 0,
          "logitech -> hid-logitech-dj");
    CHECK(strcmp(wubu_hid_driver_for("apple"), "hid-apple") == 0,
          "apple -> hid-apple");
    CHECK(strcmp(wubu_hid_driver_for("sony"), "hid-sony") == 0,
          "sony -> hid-sony");
    CHECK(strcmp(wubu_hid_driver_for("xbox"), "hid-xboxone") == 0,
          "xbox -> hid-xboxone");
    CHECK(strcmp(wubu_hid_driver_for("steam"), "hid-steam") == 0,
          "steam -> hid-steam");
    CHECK(strcmp(wubu_hid_driver_for("multitouch"), "hid-multitouch") == 0,
          "multitouch -> hid-multitouch");
    CHECK(strcmp(wubu_hid_driver_for("unknown"), "hid-generic") == 0,
          "unknown -> hid-generic fallback");

    char s[256];
    wubu_hid_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "hid summary generated");

    printf("\n=== HID TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
