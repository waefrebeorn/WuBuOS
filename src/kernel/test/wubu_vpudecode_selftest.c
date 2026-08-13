/*
 * wubu_vpudecode_selftest.c -- verifies GPU video decode routing.
 */
#include "wubu_vpudecode.h"
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
    printf("=== wubu_vpudecode_selftest ===\n");

    wubu_vpudecode_probe();

    int p = wubu_vpudecode_present();
    CHECK(p == 0 || p == 1, "vpudecode present is boolean");

    /* Codec support matrix. */
    CHECK(wubu_vpudecode_supported(0) == 1, "H.264 supported");
    CHECK(wubu_vpudecode_supported(1) == 1, "H.265 supported");
    CHECK(wubu_vpudecode_supported(2) == 1, "AV1 supported");
    CHECK(wubu_vpudecode_supported(3) == 1, "VP9 supported");
    CHECK(wubu_vpudecode_supported(4) == 1, "VP8 supported");
    CHECK(wubu_vpudecode_supported(5) == 0, "unknown codec not supported");
    CHECK(wubu_vpudecode_supported(-1) == 0, "invalid codec not supported");

    /* Codec strings. */
    CHECK(strcmp(wubu_vpudecode_codec_str(0), "h264") == 0, "codec 0 = h264");
    CHECK(strcmp(wubu_vpudecode_codec_str(2), "av1") == 0, "codec 2 = av1");

    /* Summary builds. */
    char out[160] = "";
    wubu_vpudecode_summary(out, sizeof(out));
    CHECK(strstr(out, "vpudecode[") != NULL, "summary has vpudecode fragment");

    printf("\n=== VPUDECODE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
