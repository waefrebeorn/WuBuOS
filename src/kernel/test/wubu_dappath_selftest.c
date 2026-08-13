/*
 * wubu_dappath_selftest.c -- verifies DAPM path routing.
 */
#include "wubu_dappath.h"
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
    printf("=== wubu_dappath_selftest ===\n\n");
    wubu_hw_detect();
    wubu_dappath_probe();
    printf("  path=%d pb=%d cap=%d mux=%d mix=%d\n",
           wubu_dappath_present(), wubu_dappath_pb(), wubu_dappath_cap(),
           wubu_dappath_mux(), wubu_dappath_mix());

    CHECK(strcmp(wubu_dappath_type_for("pb"), "playback") == 0,
          "pb -> playback");
    CHECK(strcmp(wubu_dappath_type_for("play"), "playback") == 0,
          "play -> playback");
    CHECK(strcmp(wubu_dappath_type_for("cap"), "capture") == 0,
          "cap -> capture");
    CHECK(strcmp(wubu_dappath_type_for("capt"), "capture") == 0,
          "capt -> capture");
    CHECK(strcmp(wubu_dappath_type_for("record"), "capture") == 0,
          "record -> capture");
    CHECK(strcmp(wubu_dappath_type_for("mux"), "mux") == 0,
          "mux -> mux");
    CHECK(strcmp(wubu_dappath_type_for("mix"), "mix") == 0,
          "mix -> mix");
    CHECK(strcmp(wubu_dappath_type_for("adc"), "adc") == 0,
          "adc -> adc");
    CHECK(strcmp(wubu_dappath_type_for("dac"), "dac") == 0,
          "dac -> dac");
    CHECK(strcmp(wubu_dappath_type_for("zzz"), "playback") == 0,
          "zzz -> playback fallback");

    CHECK(strcmp(wubu_dappath_widget_for("adc"), "ADC") == 0,
          "adc -> ADC");
    CHECK(strcmp(wubu_dappath_widget_for("dac"), "DAC") == 0,
          "dac -> DAC");
    CHECK(strcmp(wubu_dappath_widget_for("pga"), "PGA") == 0,
          "pga -> PGA");
    CHECK(strcmp(wubu_dappath_widget_for("mix"), "Mixer") == 0,
          "mix -> Mixer");
    CHECK(strcmp(wubu_dappath_widget_for("mux"), "Mux") == 0,
          "mux -> Mux");
    CHECK(strcmp(wubu_dappath_widget_for("hp"), "Headphone") == 0,
          "hp -> Headphone");
    CHECK(strcmp(wubu_dappath_widget_for("mic"), "Mic") == 0,
          "mic -> Mic");
    CHECK(strcmp(wubu_dappath_widget_for("zzz"), "PGA") == 0,
          "zzz -> PGA fallback");

    char s[256];
    wubu_dappath_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "dappath summary generated");

    printf("\n=== DAPPATH TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
