/*
 * wubu_spdif_selftest.c -- verifies kernel-owned SPDIF routing.
 */
#include "wubu_spdif.h"
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
    printf("=== wubu_spdif_selftest ===\n\n");

    wubu_hw_detect();
    wubu_spdif_probe();

    printf("  spdif=%d hdmi=%d iec61937=%d passthru=%d i2s=%d\n",
           wubu_spdif_present(), wubu_spdif_hdmi(),
           wubu_spdif_iec61937(), wubu_spdif_passthru(),
           wubu_spdif_i2s());

    /* Codec routing. */
    CHECK(strcmp(wubu_spdif_codec_for("ac3"), "ac3") == 0,
          "ac3 -> ac3");
    CHECK(strcmp(wubu_spdif_codec_for("eac3"), "ac3") == 0,
          "eac3 -> ac3");
    CHECK(strcmp(wubu_spdif_codec_for("dts"), "dts") == 0,
          "dts -> dts");
    CHECK(strcmp(wubu_spdif_codec_for("pcm"), "pcm") == 0,
          "pcm -> pcm");
    CHECK(strcmp(wubu_spdif_codec_for("aac"), "aac") == 0,
          "aac -> aac");
    CHECK(strcmp(wubu_spdif_codec_for("unknown"), "pcm") == 0,
          "unknown -> pcm fallback");

    /* Format routing. */
    CHECK(strcmp(wubu_spdif_fmt_for("raw"), "raw") == 0,
          "raw -> raw");
    CHECK(strcmp(wubu_spdif_fmt_for("burst"), "burst") == 0,
          "burst -> burst");
    CHECK(strcmp(wubu_spdif_fmt_for("hbr"), "hbr") == 0,
          "hbr -> hbr");
    CHECK(strcmp(wubu_spdif_fmt_for("fbr"), "fbr") == 0,
          "fbr -> fbr");
    CHECK(strcmp(wubu_spdif_fmt_for("unknown"), "raw") == 0,
          "unknown -> raw fallback");

    char s[256];
    wubu_spdif_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "spdif summary generated");

    printf("\n=== SPDIF TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
