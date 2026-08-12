/*
 * wubu_pmicaudio_selftest.c -- verifies kernel-owned PMIC/audio-analog routing.
 */
#include "wubu_pmicaudio.h"
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
    printf("=== wubu_pmicaudio_selftest ===\n\n");

    wubu_hw_detect();
    wubu_pmicaudio_probe();

    printf("  pmic=%d reg=%d dac=%d amp=%d\n",
           wubu_pmicaudio_pmic(), wubu_pmicaudio_regulator(),
           wubu_pmicaudio_dac(), wubu_pmicaudio_amp());

    /* DAC routing. */
    CHECK(strcmp(wubu_pmicaudio_dac_route("es9038"), "es9038q2m") == 0,
          "es9038 -> es9038q2m");
    CHECK(strcmp(wubu_pmicaudio_dac_route("ak4490"), "ak4490") == 0,
          "ak4490 -> ak4490");
    CHECK(strcmp(wubu_pmicaudio_dac_route("pcm1792"), "pcm1792") == 0,
          "pcm1792 -> pcm1792");
    CHECK(strcmp(wubu_pmicaudio_dac_route("cs4398"), "cs4398") == 0,
          "cs4398 -> cs4398");
    CHECK(strcmp(wubu_pmicaudio_dac_route("unknown"), "snd_soc_dac") == 0,
          "unknown dac -> snd_soc_dac fallback");

    /* Amp routing. */
    CHECK(strcmp(wubu_pmicaudio_amp_route("tas5805"), "tas5805m") == 0,
          "tas5805 -> tas5805m");
    CHECK(strcmp(wubu_pmicaudio_amp_route("tpa3116"), "tpa3116") == 0,
          "tpa3116 -> tpa3116");
    CHECK(strcmp(wubu_pmicaudio_amp_route("max98357"), "max98357a") == 0,
          "max98357 -> max98357a");
    CHECK(strcmp(wubu_pmicaudio_amp_route("tda7498"), "tda7498") == 0,
          "tda7498 -> tda7498");
    CHECK(strcmp(wubu_pmicaudio_amp_route("unknown"), "snd_soc_amp") == 0,
          "unknown amp -> snd_soc_amp fallback");

    char s[256];
    wubu_pmicaudio_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "pmicaudio summary generated");

    printf("\n=== PMICAUDIO TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
