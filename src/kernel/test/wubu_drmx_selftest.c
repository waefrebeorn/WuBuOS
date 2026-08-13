/*
 * wubu_drmx_selftest.c -- verifies kernel-owned DRM-advanced routing.
 */
#include "wubu_drmx.h"
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
    printf("=== wubu_drmx_selftest ===\n\n");

    wubu_hw_detect();
    wubu_drmx_probe();

    printf("  writeback=%d overlay=%d hdr=%d color=%d vkms=%d\n",
           wubu_drmx_writeback(), wubu_drmx_overlay(), wubu_drmx_hdr(),
           wubu_drmx_color_mgmt(), wubu_drmx_vkms());

    /* Writeback driver routing. */
    CHECK(strcmp(wubu_drmx_writeback_driver("vkms"), "vkms") == 0,
          "vkms -> vkms");
    CHECK(strcmp(wubu_drmx_writeback_driver("amdgpu"), "amdgpu-writeback") == 0,
          "amdgpu -> amdgpu-writeback");
    CHECK(strcmp(wubu_drmx_writeback_driver("i915"), "i915-writeback") == 0,
          "i915 -> i915-writeback");
    CHECK(strcmp(wubu_drmx_writeback_driver("unknown"), "drm-writeback") == 0,
          "unknown -> drm-writeback fallback");

    /* HDR mode routing. */
    CHECK(strcmp(wubu_drmx_hdr_mode("hdr10"), "HDR10") == 0,
          "hdr10 -> HDR10");
    CHECK(strcmp(wubu_drmx_hdr_mode("hlg"), "HLG") == 0,
          "hlg -> HLG");
    CHECK(strcmp(wubu_drmx_hdr_mode("pq"), "PQ") == 0,
          "pq -> PQ");
    CHECK(strcmp(wubu_drmx_hdr_mode("unknown"), "SDR") == 0,
          "unknown -> SDR fallback");

    char s[256];
    wubu_drmx_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "drmx summary generated");

    printf("\n=== DRMX TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
