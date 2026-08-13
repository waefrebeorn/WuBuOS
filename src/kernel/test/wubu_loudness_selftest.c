/*
 * wubu_loudness_selftest.c -- verifies kernel-owned loudness routing.
 */
#include "wubu_loudness.h"
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
    printf("=== wubu_loudness_selftest ===\n\n");

    wubu_hw_detect();
    wubu_loudness_probe();

    printf("  loud=%d gain=%d r128=%d lufs=%d pw=%d\n",
           wubu_loudness_present(), wubu_loudness_replaygain(),
           wubu_loudness_r128(), wubu_loudness_lufs(),
           wubu_loudness_pw());

    /* Mode routing. */
    CHECK(strcmp(wubu_loudness_mode_for("track"), "track-gain") == 0,
          "track -> track-gain");
    CHECK(strcmp(wubu_loudness_mode_for("album"), "album-gain") == 0,
          "album -> album-gain");
    CHECK(strcmp(wubu_loudness_mode_for("lufs"), "lufs") == 0,
          "lufs -> lufs");
    CHECK(strcmp(wubu_loudness_mode_for("off"), "off") == 0,
          "off -> off");
    CHECK(strcmp(wubu_loudness_mode_for("unknown"), "track-gain") == 0,
          "unknown -> track-gain fallback");

    /* Target routing. */
    CHECK(strcmp(wubu_loudness_target_for("89"), "-18lufs") == 0,
          "89 -> -18lufs");
    CHECK(strcmp(wubu_loudness_target_for("83"), "-16luft") == 0,
          "83 -> -16luft");
    CHECK(strcmp(wubu_loudness_target_for("93"), "-23lufs") == 0,
          "93 -> -23lufs");
    CHECK(strcmp(wubu_loudness_target_for("unknown"), "-18lufs") == 0,
          "unknown -> -18lufs fallback");

    char s[256];
    wubu_loudness_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "loudness summary generated");

    printf("\n=== LOUDNESS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
