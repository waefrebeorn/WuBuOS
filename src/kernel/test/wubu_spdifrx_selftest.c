/*
 * wubu_spdifrx_selftest.c -- verifies SPDIF receiver routing.
 */
#include "wubu_spdifrx.h"
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
    printf("=== wubu_spdifrx_selftest ===\n\n");
    wubu_hw_detect();
    wubu_spdifrx_probe();
    printf("  rx=%d rate=%d lock=%d format=%d pcm=%d\n",
           wubu_spdifrx_present(), wubu_spdifrx_rate(),
           wubu_spdifrx_lock(), wubu_spdifrx_format(),
           wubu_spdifrx_pcm());

    CHECK(strcmp(wubu_spdifrx_format_for("pcm"), "PCM") == 0,
          "pcm -> PCM");
    CHECK(strcmp(wubu_spdifrx_format_for("ac3"), "AC3") == 0,
          "ac3 -> AC3");
    CHECK(strcmp(wubu_spdifrx_format_for("dts"), "DTS") == 0,
          "dts -> DTS");
    CHECK(strcmp(wubu_spdifrx_format_for("dts-hd"), "DTS-HD") == 0,
          "dts-hd -> DTS-HD");
    CHECK(strcmp(wubu_spdifrx_format_for("truehd"), "TrueHD") == 0,
          "truehd -> TrueHD");
    CHECK(strcmp(wubu_spdifrx_format_for("eac3"), "E-AC-3") == 0,
          "eac3 -> E-AC-3");
    CHECK(strcmp(wubu_spdifrx_format_for("zzz"), "PCM") == 0,
          "zzz -> PCM fallback");

    CHECK(strcmp(wubu_spdifrx_lock_for("lock"), "locked") == 0,
          "lock -> locked");
    CHECK(strcmp(wubu_spdifrx_lock_for("plck"), "locked") == 0,
          "plck -> locked");
    CHECK(strcmp(wubu_spdifrx_lock_for("unl"), "unlocked") == 0,
          "unl -> unlocked");
    CHECK(strcmp(wubu_spdifrx_lock_for("nol"), "unlocked") == 0,
          "nol -> unlocked");
    CHECK(strcmp(wubu_spdifrx_lock_for("invalid"), "invalid") == 0,
          "invalid -> invalid");
    CHECK(strcmp(wubu_spdifrx_lock_for("zzz"), "unlocked") == 0,
          "zzz -> unlocked fallback");

    char s[256];
    wubu_spdifrx_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "spdifrx summary generated");

    printf("\n=== SPDIFRX TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
