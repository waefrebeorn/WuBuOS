/*
 * wubu_jackimpedance_selftest.c -- verifies jack impedance routing.
 */
#include "wubu_jackimpedance.h"
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
    printf("=== wubu_jackimpedance_selftest ===\n\n");
    wubu_hw_detect();
    wubu_jackimpedance_probe();
    printf("  ji=%d headphone=%d mic=%d line=%d threshold=%d\n",
           wubu_jackimpedance_present(), wubu_jackimpedance_headphone(),
           wubu_jackimpedance_mic(), wubu_jackimpedance_line(),
           wubu_jackimpedance_threshold());

    CHECK(strcmp(wubu_jackimpedance_type_for("16"), "16-ohm") == 0,
          "16 -> 16-ohm");
    CHECK(strcmp(wubu_jackimpedance_type_for("32"), "32-ohm") == 0,
          "32 -> 32-ohm");
    CHECK(strcmp(wubu_jackimpedance_type_for("150"), "150-ohm") == 0,
          "150 -> 150-ohm");
    CHECK(strcmp(wubu_jackimpedance_type_for("300"), "300-ohm") == 0,
          "300 -> 300-ohm");
    CHECK(strcmp(wubu_jackimpedance_type_for("600"), "600-ohm") == 0,
          "600 -> 600-ohm");
    CHECK(strcmp(wubu_jackimpedance_type_for("high"), "high-impedance") == 0,
          "high -> high-impedance");
    CHECK(strcmp(wubu_jackimpedance_type_for("low"), "low-impedance") == 0,
          "low -> low-impedance");
    CHECK(strcmp(wubu_jackimpedance_type_for("zzz"), "32-ohm") == 0,
          "zzz -> 32-ohm fallback");

    CHECK(strcmp(wubu_jackimpedance_device_for("headphone"), "headphone") == 0,
          "headphone -> headphone");
    CHECK(strcmp(wubu_jackimpedance_device_for("hp"), "headphone") == 0,
          "hp -> headphone");
    CHECK(strcmp(wubu_jackimpedance_device_for("headset"), "headset") == 0,
          "headset -> headset");
    CHECK(strcmp(wubu_jackimpedance_device_for("hs"), "headset") == 0,
          "hs -> headset");
    CHECK(strcmp(wubu_jackimpedance_device_for("mic"), "mic") == 0,
          "mic -> mic");
    CHECK(strcmp(wubu_jackimpedance_device_for("line"), "line") == 0,
          "line -> line");
    CHECK(strcmp(wubu_jackimpedance_device_for("zzz"), "headphone") == 0,
          "zzz -> headphone fallback");

    char s[256];
    wubu_jackimpedance_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "jackimpedance summary generated");

    printf("\n=== JACKIMPEDANCE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
