/*
 * wubu_decode_selftest.c -- verifies GPU video decode routing.
 */
#include "wubu_decode.h"
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
    printf("=== wubu_decode_selftest ===\n\n");
    wubu_hw_detect();
    wubu_decode_probe();
    printf("  dec=%d h264=%d h265=%d av1=%d vp9=%d\n",
           wubu_decode_present(), wubu_decode_h264(), wubu_decode_h265(),
           wubu_decode_av1(), wubu_decode_vp9());

    CHECK(strcmp(wubu_decode_codec_for("h264"), "H.264") == 0,
          "h264 -> H.264");
    CHECK(strcmp(wubu_decode_codec_for("avc"), "H.264") == 0,
          "avc -> H.264");
    CHECK(strcmp(wubu_decode_codec_for("h265"), "H.265") == 0,
          "h265 -> H.265");
    CHECK(strcmp(wubu_decode_codec_for("hevc"), "H.265") == 0,
          "hevc -> H.265");
    CHECK(strcmp(wubu_decode_codec_for("av1"), "AV1") == 0,
          "av1 -> AV1");
    CHECK(strcmp(wubu_decode_codec_for("vp9"), "VP9") == 0,
          "vp9 -> VP9");
    CHECK(strcmp(wubu_decode_codec_for("mpeg"), "MPEG") == 0,
          "mpeg -> MPEG");
    CHECK(strcmp(wubu_decode_codec_for("zzz"), "H.264") == 0,
          "zzz -> H.264 fallback");

    CHECK(strcmp(wubu_decode_api_for("uvd"), "VCN") == 0,
          "uvd -> VCN");
    CHECK(strcmp(wubu_decode_api_for("vcn"), "VCN") == 0,
          "vcn -> VCN");
    CHECK(strcmp(wubu_decode_api_for("amd"), "VCN") == 0,
          "amd -> VCN");
    CHECK(strcmp(wubu_decode_api_for("qsv"), "QuickSync") == 0,
          "qsv -> QuickSync");
    CHECK(strcmp(wubu_decode_api_for("intel"), "QuickSync") == 0,
          "intel -> QuickSync");
    CHECK(strcmp(wubu_decode_api_for("nvdec"), "NVDEC") == 0,
          "nvdec -> NVDEC");
    CHECK(strcmp(wubu_decode_api_for("nv"), "NVDEC") == 0,
          "nv -> NVDEC");
    CHECK(strcmp(wubu_decode_api_for("nvidia"), "NVDEC") == 0,
          "nvidia -> NVDEC");
    CHECK(strcmp(wubu_decode_api_for("v4l2"), "V4L2") == 0,
          "v4l2 -> V4L2");
    CHECK(strcmp(wubu_decode_api_for("zzz"), "VCN") == 0,
          "zzz -> VCN fallback");

    char s[256];
    wubu_decode_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "decode summary generated");

    printf("\n=== DECODE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
