/*
 * wubu_spdiftx_selftest.c -- verifies SPDIF TX control routing.
 */
#include "wubu_spdiftx.h"
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
    printf("=== wubu_spdiftx_selftest ===\n\n");
    wubu_hw_detect();
    wubu_spdiftx_probe();
    printf("  tx=%d iec=%d ac3=%d dts=%d optical=%d\n",
           wubu_spdiftx_present(), wubu_spdiftx_iec(), wubu_spdiftx_ac3(),
           wubu_spdiftx_dts(), wubu_spdiftx_optical());

    CHECK(strcmp(wubu_spdiftx_enc_for("ac3"), "ac3") == 0,
          "ac3 -> ac3");
    CHECK(strcmp(wubu_spdiftx_enc_for("eac3"), "ac3") == 0,
          "eac3 -> ac3");
    CHECK(strcmp(wubu_spdiftx_enc_for("dts"), "dts") == 0,
          "dts -> dts");
    CHECK(strcmp(wubu_spdiftx_enc_for("pcm"), "pcm") == 0,
          "pcm -> pcm");
    CHECK(strcmp(wubu_spdiftx_enc_for("lpcm"), "pcm") == 0,
          "lpcm -> pcm");
    CHECK(strcmp(wubu_spdiftx_enc_for("zzz"), "pcm") == 0,
          "zzz -> pcm fallback");

    CHECK(strcmp(wubu_spdiftx_media_for("opt"), "optical") == 0,
          "opt -> optical");
    CHECK(strcmp(wubu_spdiftx_media_for("coax"), "coax") == 0,
          "coax -> coax");
    CHECK(strcmp(wubu_spdiftx_media_for("arc"), "arc") == 0,
          "arc -> arc");
    CHECK(strcmp(wubu_spdiftx_media_for("hdmi"), "hdmi-arc") == 0,
          "hdmi -> hdmi-arc");
    CHECK(strcmp(wubu_spdiftx_media_for("zzz"), "optical") == 0,
          "zzz -> optical fallback");

    char s[256];
    wubu_spdiftx_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "spdiftx summary generated");

    printf("\n=== SPDIFTX TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
