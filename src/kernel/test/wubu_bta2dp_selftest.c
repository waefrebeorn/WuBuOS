/*
 * wubu_bta2dp_selftest.c -- verifies Bluetooth A2DP routing.
 */
#include "wubu_bta2dp.h"
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
    printf("=== wubu_bta2dp_selftest ===\n");

    wubu_bta2dp_probe();

    int p = wubu_bta2dp_present();
    CHECK(p == 0 || p == 1, "bta2dp present is boolean");

    /* Codec bitrates. */
    CHECK(wubu_bta2dp_bitrate(0) == 320, "SBC = 320kbps");
    CHECK(wubu_bta2dp_bitrate(1) == 256, "MP3 = 256kbps");
    CHECK(wubu_bta2dp_bitrate(2) == 320, "AAC = 320kbps");
    CHECK(wubu_bta2dp_bitrate(3) == 576, "aptX = 576kbps");
    CHECK(wubu_bta2dp_bitrate(4) == 990, "aptX HD = 990kbps");
    CHECK(wubu_bta2dp_bitrate(99) == 320, "unknown codec = fallback 320");

    /* Codec strings. */
    CHECK(strcmp(wubu_bta2dp_codec_str(0), "sbc") == 0, "codec 0 = sbc");
    CHECK(strcmp(wubu_bta2dp_codec_str(1), "mp3") == 0, "codec 1 = mp3");
    CHECK(strcmp(wubu_bta2dp_codec_str(2), "aac") == 0, "codec 2 = aac");
    CHECK(strcmp(wubu_bta2dp_codec_str(3), "aptx") == 0, "codec 3 = aptx");
    CHECK(strcmp(wubu_bta2dp_codec_str(4), "aptx_hd") == 0, "codec 4 = aptx_hd");

    /* Summary builds. */
    char out[160] = "";
    wubu_bta2dp_summary(out, sizeof(out));
    CHECK(strstr(out, "bta2dp[") != NULL, "summary has bta2dp fragment");

    printf("\n=== BTA2DP TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
