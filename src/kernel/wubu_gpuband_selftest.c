/*
 * wubu_gpuband_selftest.c -- verifies kernel-owned GPU-band routing.
 */
#include "wubu_gpuband.h"
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
    printf("=== wubu_gpuband_selftest ===\n\n");

    wubu_hw_detect();
    wubu_gpuband_probe();

    printf("  band=%d fair=%d prio=%d entity=%d stats=%d\n",
           wubu_gpuband_present(), wubu_gpuband_fair(),
           wubu_gpuband_prio(), wubu_gpuband_entity(),
           wubu_gpuband_stats());

    /* Priority routing. */
    CHECK(strcmp(wubu_gpuband_prio_for("high"), "high") == 0, "high -> high");
    CHECK(strcmp(wubu_gpuband_prio_for("critical"), "high") == 0,
          "critical -> high");
    CHECK(strcmp(wubu_gpuband_prio_for("low"), "low") == 0, "low -> low");
    CHECK(strcmp(wubu_gpuband_prio_for("kernel"), "kernel") == 0,
          "kernel -> kernel");
    CHECK(strcmp(wubu_gpuband_prio_for("normal"), "normal") == 0,
          "normal -> normal");
    CHECK(strcmp(wubu_gpuband_prio_for("zzz"), "normal") == 0,
          "zzz -> normal fallback");

    /* Class routing. */
    CHECK(strcmp(wubu_gpuband_class_for("3d"), "3d") == 0, "3d -> 3d");
    CHECK(strcmp(wubu_gpuband_class_for("compute"), "compute") == 0,
          "compute -> compute");
    CHECK(strcmp(wubu_gpuband_class_for("video"), "video") == 0,
          "video -> video");
    CHECK(strcmp(wubu_gpuband_class_for("copy"), "copy") == 0,
          "copy -> copy");
    CHECK(strcmp(wubu_gpuband_class_for("zzz"), "default") == 0,
          "zzz -> default fallback");

    char s[256];
    wubu_gpuband_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "gpuband summary generated");

    printf("\n=== GPUBAND TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
