/*
 * wubu_vram_selftest.c -- verifies kernel-owned VRAM routing.
 */
#include "wubu_vram.h"
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
    printf("=== wubu_vram_selftest ===\n\n");

    wubu_hw_detect();
    wubu_vram_probe();

    printf("  vram=%d fb=%d stolen=%d ttm=%d drm_mm=%d\n",
           wubu_vram_present(), wubu_vram_fb(), wubu_vram_stolen(),
           wubu_vram_ttm(), wubu_vram_drm_mm());

    /* Pool routing. */
    CHECK(strcmp(wubu_vram_pool_for("stolen"), "stolen") == 0,
          "stolen -> stolen");
    CHECK(strcmp(wubu_vram_pool_for("ttm"), "ttm") == 0,
          "ttm -> ttm");
    CHECK(strcmp(wubu_vram_pool_for("vram"), "vram") == 0,
          "vram -> vram");
    CHECK(strcmp(wubu_vram_pool_for("fb"), "framebuffer") == 0,
          "fb -> framebuffer");
    CHECK(strcmp(wubu_vram_pool_for("unknown"), "vram") == 0,
          "unknown -> vram fallback");

    /* Alloc routing. */
    CHECK(strcmp(wubu_vram_alloc_for("gpu"), "gpu-domain") == 0,
          "gpu -> gpu-domain");
    CHECK(strcmp(wubu_vram_alloc_for("cpu"), "cpu-domain") == 0,
          "cpu -> cpu-domain");
    CHECK(strcmp(wubu_vram_alloc_for("wc"), "write-combining") == 0,
          "wc -> write-combining");
    CHECK(strcmp(wubu_vram_alloc_for("uc"), "uncached") == 0,
          "uc -> uncached");
    CHECK(strcmp(wubu_vram_alloc_for("wb"), "write-back") == 0,
          "wb -> write-back");
    CHECK(strcmp(wubu_vram_alloc_for("unknown"), "vram") == 0,
          "unknown -> vram fallback");

    char s[256];
    wubu_vram_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "vram summary generated");

    printf("\n=== VRAM TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
