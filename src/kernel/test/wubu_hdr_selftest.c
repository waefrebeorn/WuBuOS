/*
 * wubu_hdr_selftest.c -- verifies kernel-owned HDR/jack routing.
 */
#include "wubu_hdr.h"
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
    printf("=== wubu_hdr_selftest ===\n\n");

    wubu_hw_detect();
    wubu_hdr_probe();

    printf("  hdr10=%d hdr10p=%d dv=%d sink=%d jack=%d\n",
           wubu_hdr_hdr10(), wubu_hdr_hdr10p(), wubu_hdr_dv(),
           wubu_hdr_sink(), wubu_hdr_jack());

    /* HDR metadata routing. */
    CHECK(strcmp(wubu_hdr_meta_for("hdr10"), "hdr10") == 0,
          "hdr10 -> hdr10");
    CHECK(strcmp(wubu_hdr_meta_for("hdr10+"), "hdr10plus") == 0,
          "hdr10+ -> hdr10plus");
    CHECK(strcmp(wubu_hdr_meta_for("dv"), "dolby-vision") == 0,
          "dv -> dolby-vision");
    CHECK(strcmp(wubu_hdr_meta_for("hlg"), "hlg") == 0,
          "hlg -> hlg");
    CHECK(strcmp(wubu_hdr_meta_for("unknown"), "sdr") == 0,
          "unknown -> sdr fallback");

    /* Jack detection routing. */
    CHECK(strcmp(wubu_hdr_jack_for("headphone"), "hda-headphone") == 0,
          "headphone -> hda-headphone");
    CHECK(strcmp(wubu_hdr_jack_for("mic"), "hda-mic") == 0,
          "mic -> hda-mic");
    CHECK(strcmp(wubu_hdr_jack_for("hdmi"), "hda-hdmi") == 0,
          "hdmi -> hda-hdmi");
    CHECK(strcmp(wubu_hdr_jack_for("asoc"), "asoc-jack") == 0,
          "asoc -> asoc-jack");
    CHECK(strcmp(wubu_hdr_jack_for("unknown"), "jack") == 0,
          "unknown -> jack fallback");

    char s[256];
    wubu_hdr_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "hdr summary generated");

    printf("\n=== HDR TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
