/*
 * wubu_camera_selftest.c -- verifies kernel-owned V4L2 camera routing.
 */
#include "wubu_camera.h"
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
    printf("=== wubu_camera_selftest ===\n\n");

    wubu_hw_detect();
    wubu_camera_probe();

    printf("  v4l2=%d uvc=%d isp=%d mipi=%d video=%d media=%d\n",
           wubu_camera_present(), wubu_camera_has_uvc(), wubu_camera_has_isp(),
           wubu_camera_has_mipi(), wubu_camera_video_nodes(),
           wubu_camera_media_devices());

    /* Pipeline routing is always consistent. */
    CHECK(strcmp(wubu_camera_pipeline_driver("uvc"), "uvcvideo") == 0,
          "uvc -> uvcvideo");
    CHECK(strcmp(wubu_camera_pipeline_driver("rkisp"), "rkisp1") == 0,
          "rkisp -> rkisp1");
    CHECK(strcmp(wubu_camera_pipeline_driver("imx219"), "imx219") == 0,
          "imx219 -> imx219");
    CHECK(strcmp(wubu_camera_pipeline_driver("ov5640"), "ov5640") == 0,
          "ov5640 -> ov5640");
    CHECK(strcmp(wubu_camera_pipeline_driver("vimc"), "vimc") == 0,
          "vimc -> vimc (virtual)");
    CHECK(strcmp(wubu_camera_pipeline_driver("unknown"), "v4l2") == 0,
          "unknown -> v4l2 fallback");

    char s[256];
    wubu_camera_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "camera summary generated");

    printf("\n=== CAMERA TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
