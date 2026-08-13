/*
 * wubu_audiofw_selftest.c -- verifies audio firmware routing.
 */
#include "wubu_audiofw.h"
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
    printf("=== wubu_audiofw_selftest ===\n\n");
    wubu_hw_detect();
    wubu_audiofw_probe();
    printf("  afw=%d codec=%d dsp=%d loader=%d bios=%d\n",
           wubu_audiofw_present(), wubu_audiofw_codec(), wubu_audiofw_dsp(),
           wubu_audiofw_loader(), wubu_audiofw_bios());

    CHECK(strcmp(wubu_audiofw_codec_for("realtek"), "Realtek") == 0,
          "realtek -> Realtek");
    CHECK(strcmp(wubu_audiofw_codec_for("alc"), "Realtek") == 0,
          "alc -> Realtek");
    CHECK(strcmp(wubu_audiofw_codec_for("cirrus"), "Cirrus") == 0,
          "cirrus -> Cirrus");
    CHECK(strcmp(wubu_audiofw_codec_for("cs"), "Cirrus") == 0,
          "cs -> Cirrus");
    CHECK(strcmp(wubu_audiofw_codec_for("WM"), "WM") == 0,
          "WM -> WM");
    CHECK(strcmp(wubu_audiofw_codec_for("wmc"), "WM") == 0,
          "wmc -> WM");
    CHECK(strcmp(wubu_audiofw_codec_for("ti"), "TI") == 0,
          "ti -> TI");
    CHECK(strcmp(wubu_audiofw_codec_for("tlv320"), "TI") == 0,
          "tlv320 -> TI");
    CHECK(strcmp(wubu_audiofw_codec_for("conex"), "Conexant") == 0,
          "conex -> Conexant");
    CHECK(strcmp(wubu_audiofw_codec_for("zzz"), "Realtek") == 0,
          "zzz -> Realtek fallback");

    CHECK(strcmp(wubu_audiofw_loader_for("fw"), "firmware") == 0,
          "fw -> firmware");
    CHECK(strcmp(wubu_audiofw_loader_for("bios"), "bios") == 0,
          "bios -> bios");
    CHECK(strcmp(wubu_audiofw_loader_for("elf"), "elf") == 0,
          "elf -> elf");
    CHECK(strcmp(wubu_audiofw_loader_for("bezirk"), "bezirk") == 0,
          "bezirk -> bezirk");
    CHECK(strcmp(wubu_audiofw_loader_for("zzz"), "firmware") == 0,
          "zzz -> firmware fallback");

    char s[256];
    wubu_audiofw_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "audiofw summary generated");

    printf("\n=== AUDIOFW TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
