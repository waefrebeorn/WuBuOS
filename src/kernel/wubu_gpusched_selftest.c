/*
 * wubu_gpusched_selftest.c -- verifies kernel-owned GPU-scheduler routing.
 */
#include "wubu_gpusched.h"
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
    printf("=== wubu_gpusched_selftest ===\n\n");

    wubu_hw_detect();
    wubu_gpusched_probe();

    printf("  sched=%d guc=%d prio=%d preempt=%d fair=%d\n",
           wubu_gpusched_present(), wubu_gpusched_guc(),
           wubu_gpusched_prio(), wubu_gpusched_preempt(),
           wubu_gpusched_fair());

    /* Priority routing. */
    CHECK(strcmp(wubu_gpusched_prio_for("high"), "high") == 0,
          "high -> high");
    CHECK(strcmp(wubu_gpusched_prio_for("normal"), "normal") == 0,
          "normal -> normal");
    CHECK(strcmp(wubu_gpusched_prio_for("low"), "low") == 0,
          "low -> low");
    CHECK(strcmp(wubu_gpusched_prio_for("unknown"), "normal") == 0,
          "unknown -> normal fallback");

    /* Class routing. */
    CHECK(strcmp(wubu_gpusched_class_for("3d"), "3d") == 0,
          "3d -> 3d");
    CHECK(strcmp(wubu_gpusched_class_for("compute"), "compute") == 0,
          "compute -> compute");
    CHECK(strcmp(wubu_gpusched_class_for("video"), "video") == 0,
          "video -> video");
    CHECK(strcmp(wubu_gpusched_class_for("copy"), "copy") == 0,
          "copy -> copy");
    CHECK(strcmp(wubu_gpusched_class_for("unknown"), "3d") == 0,
          "unknown -> 3d fallback");

    char s[256];
    wubu_gpusched_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "gpusched summary generated");

    printf("\n=== GPUSCHED TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
