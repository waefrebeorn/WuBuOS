/*
 * wubu_codec_selftest.c -- verifies kernel-owned audio codec routing.
 */
#include "wubu_codec.h"
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
    printf("=== wubu_codec_selftest ===\n\n");

    wubu_hw_detect();
    wubu_codec_probe();

    printf("  present=%d hda=%d asoc=%d sof=%d drv=%s\n",
           wubu_codec_present(), wubu_codec_has_hda(), wubu_codec_has_asoc(),
           wubu_codec_has_sof_dsp(),
           wubu_codec_driver() ? wubu_codec_driver() : "none");

    /* HDA codec routing. */
    CHECK(strcmp(wubu_codec_hda_driver("realtek"), "snd_hda_codec_realtek") == 0,
          "realtek -> snd_hda_codec_realtek");
    CHECK(strcmp(wubu_codec_hda_driver("idt"), "snd_hda_codec_idt") == 0,
          "idt -> snd_hda_codec_idt");
    CHECK(strcmp(wubu_codec_hda_driver("cirrus"), "snd_hda_codec_cirrus") == 0,
          "cirrus -> snd_hda_codec_cirrus");
    CHECK(strcmp(wubu_codec_hda_driver("hdmi"), "snd_hda_codec_hdmi") == 0,
          "hdmi -> snd_hda_codec_hdmi");
    CHECK(strcmp(wubu_codec_hda_driver("unknown"), "snd_hda_codec_generic") == 0,
          "unknown hda -> generic");

    /* ASoC codec routing. */
    CHECK(strcmp(wubu_codec_asoc_driver("wm8960"), "snd_soc_wm8960") == 0,
          "wm8960 -> snd_soc_wm8960");
    CHECK(strcmp(wubu_codec_asoc_driver("cs42l42"), "snd_soc_cs42l42") == 0,
          "cs42l42 -> snd_soc_cs42l42");
    CHECK(strcmp(wubu_codec_asoc_driver("rt5682"), "snd_soc_rt5682") == 0,
          "rt5682 -> snd_soc_rt5682");
    CHECK(strcmp(wubu_codec_asoc_driver("max98357"), "snd_soc_max98357a") == 0,
          "max98357 -> snd_soc_max98357a");
    CHECK(strcmp(wubu_codec_asoc_driver("unknown"), "snd_soc_generic") == 0,
          "unknown asoc -> generic");

    char s[256];
    wubu_codec_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "codec summary generated");

    printf("\n=== CODEC TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
