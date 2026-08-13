/*
 * wubu_hidadv_selftest.c -- verifies kernel-owned HID routing.
 */
#include "wubu_hidadv.h"
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
    printf("=== wubu_hidadv_selftest ===\n\n");

    wubu_hw_detect();
    wubu_hidadv_probe();

    printf("  hid=%d generic=%d mt=%d ff=%d vendor=%d\n",
           wubu_hidadv_present(), wubu_hidadv_generic(), wubu_hidadv_multitouch(),
           wubu_hidadv_ff(), wubu_hidadv_vendor());

    /* HID driver routing is always consistent. */
    CHECK(strcmp(wubu_hidadv_driver_for("logitech"), "hid-logitech-dj") == 0,
          "logitech -> hid-logitech-dj");
    CHECK(strcmp(wubu_hidadv_driver_for("apple"), "hid-apple") == 0,
          "apple -> hid-apple");
    CHECK(strcmp(wubu_hidadv_driver_for("sony"), "hid-sony") == 0,
          "sony -> hid-sony");
    CHECK(strcmp(wubu_hidadv_driver_for("xbox"), "hid-xboxone") == 0,
          "xbox -> hid-xboxone");
    CHECK(strcmp(wubu_hidadv_driver_for("steam"), "hid-steam") == 0,
          "steam -> hid-steam");
    CHECK(strcmp(wubu_hidadv_driver_for("multitouch"), "hid-multitouch") == 0,
          "multitouch -> hid-multitouch");
    CHECK(strcmp(wubu_hidadv_driver_for("unknown"), "hid-generic") == 0,
          "unknown -> hid-generic fallback");

    char s[256];
    wubu_hidadv_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "hidadv summary generated");

    printf("\n=== HIDADV TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
