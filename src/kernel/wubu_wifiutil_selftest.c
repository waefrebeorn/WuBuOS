/*
 * wubu_wifiutil_selftest.c -- verifies kernel-owned WiFi-util routing.
 */
#include "wubu_wifiutil.h"
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
    printf("=== wubu_wifiutil_selftest ===\n\n");
    wubu_hw_detect();
    wubu_wifiutil_probe();
    printf("  util=%d cca=%d airtime=%d survey=%d chan=%d\n",
           wubu_wifiutil_present(), wubu_wifiutil_cca(), wubu_wifiutil_airtime(),
           wubu_wifiutil_survey(), wubu_wifiutil_chan());

    /* Band routing. */
    CHECK(strcmp(wubu_wifiutil_band_for("2.4"), "2.4ghz") == 0,
          "2.4 -> 2.4ghz");
    CHECK(strcmp(wubu_wifiutil_band_for("2g"), "2.4ghz") == 0,
          "2g -> 2.4ghz");
    CHECK(strcmp(wubu_wifiutil_band_for("5"), "5ghz") == 0,
          "5 -> 5ghz");
    CHECK(strcmp(wubu_wifiutil_band_for("6"), "6ghz") == 0,
          "6 -> 6ghz");
    CHECK(strcmp(wubu_wifiutil_band_for("zzz"), "2.4ghz") == 0,
          "zzz -> 2.4ghz fallback");

    /* State routing. */
    CHECK(strcmp(wubu_wifiutil_state_for("busy"), "busy") == 0,
          "busy -> busy");
    CHECK(strcmp(wubu_wifiutil_state_for("rx"), "rx") == 0,
          "rx -> rx");
    CHECK(strcmp(wubu_wifiutil_state_for("tx"), "tx") == 0,
          "tx -> tx");
    CHECK(strcmp(wubu_wifiutil_state_for("idle"), "idle") == 0,
          "idle -> idle");
    CHECK(strcmp(wubu_wifiutil_state_for("zzz"), "unknown") == 0,
          "zzz -> unknown fallback");

    char s[256];
    wubu_wifiutil_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "wifiutil summary generated");

    printf("\n=== WIFIUTIL TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
