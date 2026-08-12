/*
 * wubu_eq_selftest.c -- verifies kernel-owned audio-EQ routing.
 */
#include "wubu_eq.h"
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
    printf("=== wubu_eq_selftest ===\n\n");

    wubu_hw_detect();
    wubu_eq_probe();

    printf("  alsa=%d sw=%d dsp=%d biquad=%d loudness=%d\n",
           wubu_eq_alsa(), wubu_eq_software(), wubu_eq_dsp(),
           wubu_eq_biquad(), wubu_eq_loudness());

    /* EQ driver routing is always consistent. */
    CHECK(strcmp(wubu_eq_driver_for("sof"), "sof-dsp") == 0,
          "sof -> sof-dsp");
    CHECK(strcmp(wubu_eq_driver_for("pipewire"), "pw-eq") == 0,
          "pipewire -> pw-eq");
    CHECK(strcmp(wubu_eq_driver_for("pulse"), "pulse-eq") == 0,
          "pulse -> pulse-eq");
    CHECK(strcmp(wubu_eq_driver_for("alsa"), "alsa-eq") == 0,
          "alsa -> alsa-eq");
    CHECK(strcmp(wubu_eq_driver_for("loudness"), "loudness-drc") == 0,
          "loudness -> loudness-drc");
    CHECK(strcmp(wubu_eq_driver_for("unknown"), "eq-core") == 0,
          "unknown -> eq-core fallback");

    char s[256];
    wubu_eq_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "eq summary generated");

    printf("\n=== EQ TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
