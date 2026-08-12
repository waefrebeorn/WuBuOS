/*
 * wubu_drm_selftest.c -- verifies GPU DRM routing.
 */
#include "wubu_drm.h"
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
    printf("=== wubu_drm_selftest ===\n\n");
    wubu_hw_detect();
    wubu_drm_probe();
    printf("  drm=%d kms=%d gem=%d prime=%d msi=%d\n",
           wubu_drm_present(), wubu_drm_kms(), wubu_drm_gem(),
           wubu_drm_prime(), wubu_drm_msi());

    CHECK(strcmp(wubu_drm_subsys_for("amdgpu"), "amdgpu") == 0,
          "amdgpu -> amdgpu");
    CHECK(strcmp(wubu_drm_subsys_for("amd"), "amdgpu") == 0,
          "amd -> amdgpu");
    CHECK(strcmp(wubu_drm_subsys_for("i915"), "i915") == 0,
          "i915 -> i915");
    CHECK(strcmp(wubu_drm_subsys_for("intel"), "i915") == 0,
          "intel -> i915");
    CHECK(strcmp(wubu_drm_subsys_for("nouveau"), "nouveau") == 0,
          "nouveau -> nouveau");
    CHECK(strcmp(wubu_drm_subsys_for("mgag200"), "mgag200") == 0,
          "mgag200 -> mgag200");
    CHECK(strcmp(wubu_drm_subsys_for("mga"), "mgag200") == 0,
          "mga -> mgag200");
    CHECK(strcmp(wubu_drm_subsys_for("ast"), "ast") == 0,
          "ast -> ast");
    CHECK(strcmp(wubu_drm_subsys_for("virgl"), "virtio-gpu") == 0,
          "virgl -> virtio-gpu");
    CHECK(strcmp(wubu_drm_subsys_for("bochs"), "bochs") == 0,
          "bochs -> bochs");
    CHECK(strcmp(wubu_drm_subsys_for("zzz"), "amdgpu") == 0,
          "zzz -> amdgpu fallback");

    CHECK(strcmp(wubu_drm_obj_for("crtc"), "CRTC") == 0,
          "crtc -> CRTC");
    CHECK(strcmp(wubu_drm_obj_for("connector"), "Connector") == 0,
          "connector -> Connector");
    CHECK(strcmp(wubu_drm_obj_for("encoder"), "Encoder") == 0,
          "encoder -> Encoder");
    CHECK(strcmp(wubu_drm_obj_for("plane"), "Plane") == 0,
          "plane -> Plane");
    CHECK(strcmp(wubu_drm_obj_for("framebuffer"), "Framebuffer") == 0,
          "framebuffer -> Framebuffer");
    CHECK(strcmp(wubu_drm_obj_for("gem"), "GEM") == 0,
          "gem -> GEM");
    CHECK(strcmp(wubu_drm_obj_for("zzz"), "Framebuffer") == 0,
          "zzz -> Framebuffer fallback");

    char s[256];
    wubu_drm_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "drm summary generated");

    printf("\n=== DRM TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
