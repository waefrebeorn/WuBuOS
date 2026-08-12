/*
 * wubu_touch_selftest.c -- verifies kernel-owned touch routing.
 */
#include "wubu_touch.h"
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
    printf("=== wubu_touch_selftest ===\n\n");

    wubu_hw_detect();
    wubu_touch_probe();

    printf("  touch=%d elan=%d synaptics=%d mt=%d wacom=%d\n",
           wubu_touch_present(), wubu_touch_elan(), wubu_touch_synaptics(),
           wubu_touch_multitouch(), wubu_touch_wacom());

    /* Touch driver routing is always consistent. */
    CHECK(strcmp(wubu_touch_driver_for("elan"), "elan_i2c") == 0,
          "elan -> elan_i2c");
    CHECK(strcmp(wubu_touch_driver_for("synaptics"), "rmi4") == 0,
          "synaptics -> rmi4");
    CHECK(strcmp(wubu_touch_driver_for("alps"), "alps") == 0,
          "alps -> alps");
    CHECK(strcmp(wubu_touch_driver_for("wacom"), "wacom") == 0,
          "wacom -> wacom");
    CHECK(strcmp(wubu_touch_driver_for("goodix"), "goodix_ts") == 0,
          "goodix -> goodix_ts");
    CHECK(strcmp(wubu_touch_driver_for("cypress"), "cypress-sf") == 0,
          "cypress -> cypress-sf");
    CHECK(strcmp(wubu_touch_driver_for("silead"), "silead") == 0,
          "silead -> silead");
    CHECK(strcmp(wubu_touch_driver_for("unknown"), "hid-multitouch") == 0,
          "unknown -> hid-multitouch fallback");

    char s[256];
    wubu_touch_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "touch summary generated");

    printf("\n=== TOUCH TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
