/*
 * wubu_samplerate_selftest.c -- verifies sample-rate routing.
 */
#include "wubu_samplerate.h"
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
    printf("=== wubu_samplerate_selftest ===\n\n");
    wubu_hw_detect();
    wubu_samplerate_probe();
    printf("  sr=%d pcm=%d float=%d 24bit=%d hi=%d\n",
           wubu_samplerate_present(), wubu_samplerate_pcm(),
           wubu_samplerate_float(), wubu_samplerate_24bit(),
           wubu_samplerate_hi());

    /* Format routing. */
    CHECK(strcmp(wubu_samplerate_fmt_for("float"), "float") == 0,
          "float -> float");
    CHECK(strcmp(wubu_samplerate_fmt_for("s24"), "s24") == 0,
          "s24 -> s24");
    CHECK(strcmp(wubu_samplerate_fmt_for("s32"), "s32") == 0,
          "s32 -> s32");
    CHECK(strcmp(wubu_samplerate_fmt_for("s16"), "s16") == 0,
          "s16 -> s16");
    CHECK(strcmp(wubu_samplerate_fmt_for("u8"), "u8") == 0,
          "u8 -> u8");
    CHECK(strcmp(wubu_samplerate_fmt_for("zzz"), "s16") == 0,
          "zzz -> s16 fallback");

    /* Rate routing. */
    CHECK(strcmp(wubu_samplerate_rate_for("192"), "high-res") == 0,
          "192 -> high-res");
    CHECK(strcmp(wubu_samplerate_rate_for("384"), "high-res") == 0,
          "384 -> high-res");
    CHECK(strcmp(wubu_samplerate_rate_for("96"), "high-rate") == 0,
          "96 -> high-rate");
    CHECK(strcmp(wubu_samplerate_rate_for("48"), "48k") == 0,
          "48 -> 48k");
    CHECK(strcmp(wubu_samplerate_rate_for("44"), "44.1k") == 0,
          "44 -> 44.1k");
    CHECK(strcmp(wubu_samplerate_rate_for("zzz"), "48k") == 0,
          "zzz -> 48k fallback");

    char s[256];
    wubu_samplerate_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "samplerate summary generated");

    printf("\n=== SAMPLERATE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
