/*
 * wubu_vpuencode_selftest.c -- verifies GPU video encode routing.
 */
#include "wubu_vpuencode.h"
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
    printf("=== wubu_vpuencode_selftest ===\n");

    wubu_vpuencode_probe();

    int p = wubu_vpuencode_present();
    CHECK(p == 0 || p == 1, "vpuencode present is boolean");

    /* Bitrate estimation. */
    CHECK(wubu_vpuencode_rate(1920, 1080, 30) == 62208, "1080p30 = 62208kbps");
    CHECK(wubu_vpuencode_rate(3840, 2160, 60) == 497664, "4K60 = 497664kbps");

    /* Codec strings. */
    CHECK(strcmp(wubu_vpuencode_codec_str(0), "h264") == 0, "codec 0 = h264");
    CHECK(strcmp(wubu_vpuencode_codec_str(1), "h265") == 0, "codec 1 = h265");
    CHECK(strcmp(wubu_vpuencode_codec_str(2), "av1") == 0, "codec 2 = av1");

    /* Summary builds. */
    char out[160] = "";
    wubu_vpuencode_summary(out, sizeof(out));
    CHECK(strstr(out, "vpuencode[") != NULL, "summary has vpuencode fragment");

    printf("\n=== VPUENCODE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
