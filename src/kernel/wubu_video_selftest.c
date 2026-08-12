/*
 * wubu_video_selftest.c -- verifies kernel-owned video codec routing.
 */
#include "wubu_video.h"
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
    printf("=== wubu_video_selftest ===\n\n");

    wubu_hw_detect();
    wubu_video_probe();

    printf("  vaapi=%d vdpau=%d m2m=%d av1=%d hevc=%d vp9=%d\n",
           wubu_video_vaapi(), wubu_video_vdpau(), wubu_video_v4l2_m2m(),
           wubu_video_av1(), wubu_video_hevc(), wubu_video_vp9());

    /* Codec driver routing is always consistent. */
    CHECK(strcmp(wubu_video_driver_for("intel"), "iHD") == 0,
          "intel -> iHD");
    CHECK(strcmp(wubu_video_driver_for("amd"), "radeonsi") == 0,
          "amd -> radeonsi");
    CHECK(strcmp(wubu_video_driver_for("nvidia"), "vdpau") == 0,
          "nvidia -> vdpau");
    CHECK(strcmp(wubu_video_driver_for("qualcomm"), "venus") == 0,
          "qualcomm -> venus");
    CHECK(strcmp(wubu_video_driver_for("rockchip"), "rkvdec") == 0,
          "rockchip -> rkvdec");
    CHECK(strcmp(wubu_video_driver_for("unknown"), "v4l2-m2m") == 0,
          "unknown -> v4l2-m2m fallback");

    /* Present iff any engine. */
    CHECK(wubu_video_present() == (wubu_video_vaapi() || wubu_video_vdpau() || wubu_video_v4l2_m2m()),
          "present == (any engine)");

    char s[256];
    wubu_video_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "video summary generated");

    printf("\n=== VIDEO TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
