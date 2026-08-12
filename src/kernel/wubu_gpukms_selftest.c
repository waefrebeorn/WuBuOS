/*
 * wubu_gpukms_selftest.c -- verifies GPU KMS modeset routing.
 */
#include "wubu_gpukms.h"
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
    printf("=== wubu_gpukms_selftest ===\n");

    wubu_gpukms_probe();

    int p = wubu_gpukms_present();
    CHECK(p == 0 || p == 1, "gpukms present is boolean");

    /* Mode validation. */
    CHECK(wubu_gpukms_valid_mode(1920, 1080, 60) == 1, "1920x1080@60 valid");
    CHECK(wubu_gpukms_valid_mode(3840, 2160, 120) == 1, "4K@120 valid");
    CHECK(wubu_gpukms_valid_mode(640, 480, 30) == 1, "640x480@30 valid");
    CHECK(wubu_gpukms_valid_mode(320, 240, 60) == 0, "320x240 too small");
    CHECK(wubu_gpukms_valid_mode(1920, 1080, 10) == 0, "10Hz too low");
    CHECK(wubu_gpukms_valid_mode(1920, 1080, 300) == 0, "300Hz too high");

    /* Active check. */
    CHECK(wubu_gpukms_is_active(1, 1) == 1, "crtc+conn = active");
    CHECK(wubu_gpukms_is_active(1, 0) == 0, "crtc only = not active");
    CHECK(wubu_gpukms_is_active(0, 1) == 0, "conn only = not active");

    /* Summary builds. */
    char out[160] = "";
    wubu_gpukms_summary(out, sizeof(out));
    CHECK(strstr(out, "gpukms[") != NULL, "summary has gpukms fragment");

    printf("\n=== GPUKMS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
