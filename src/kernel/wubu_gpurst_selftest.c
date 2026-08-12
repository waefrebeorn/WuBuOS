/*
 * wubu_gpurst_selftest.c -- verifies kernel-owned GPU reset routing.
 */
#include "wubu_gpurst.h"
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
    printf("=== wubu_gpurst_selftest ===\n\n");
    wubu_hw_detect();
    wubu_gpurst_probe();
    printf("  reset=%d ring=%d hb=%d timeout=%d recover=%d\n",
           wubu_gpurst_present(), wubu_gpurst_ring(), wubu_gpurst_hb(),
           wubu_gpurst_timeout(), wubu_gpurst_recover());

    /* Stage routing. */
    CHECK(strcmp(wubu_gpurst_stage_for("pre"), "pre-reset") == 0,
          "pre -> pre-reset");
    CHECK(strcmp(wubu_gpurst_stage_for("stop"), "pre-reset") == 0,
          "stop -> pre-reset");
    CHECK(strcmp(wubu_gpurst_stage_for("reset"), "reset") == 0,
          "reset -> reset");
    CHECK(strcmp(wubu_gpurst_stage_for("post"), "post-reset") == 0,
          "post -> post-reset");
    CHECK(strcmp(wubu_gpurst_stage_for("fault"), "fault") == 0,
          "fault -> fault");
    CHECK(strcmp(wubu_gpurst_stage_for("zzz"), "idle") == 0,
          "zzz -> idle fallback");

    /* Ring routing. */
    CHECK(strcmp(wubu_gpurst_ring_for("gfx"), "gfx") == 0,
          "gfx -> gfx");
    CHECK(strcmp(wubu_gpurst_ring_for("3d"), "gfx") == 0,
          "3d -> gfx");
    CHECK(strcmp(wubu_gpurst_ring_for("compute"), "compute") == 0,
          "compute -> compute");
    CHECK(strcmp(wubu_gpurst_ring_for("dma"), "dma") == 0,
          "dma -> dma");
    CHECK(strcmp(wubu_gpurst_ring_for("video"), "video") == 0,
          "video -> video");
    CHECK(strcmp(wubu_gpurst_ring_for("zzz"), "gfx") == 0,
          "zzz -> gfx fallback");

    char s[256];
    wubu_gpurst_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "gpurst summary generated");

    printf("\n=== GPURST TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
