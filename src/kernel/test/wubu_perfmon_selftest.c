/*
 * wubu_perfmon_selftest.c -- verifies GPU perf-counter routing.
 */
#include "wubu_perfmon.h"
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
    printf("=== wubu_perfmon_selftest ===\n\n");
    wubu_hw_detect();
    wubu_perfmon_probe();
    printf("  pm=%d event=%d cycles=%d cache=%d occ=%d\n",
           wubu_perfmon_present(), wubu_perfmon_event(), wubu_perfmon_cycles(),
           wubu_perfmon_cache(), wubu_perfmon_occ());

    CHECK(strcmp(wubu_perfmon_metric_for("cycle"), "cycles") == 0,
          "cycle -> cycles");
    CHECK(strcmp(wubu_perfmon_metric_for("instr"), "instructions") == 0,
          "instr -> instructions");
    CHECK(strcmp(wubu_perfmon_metric_for("cache"), "cache-hits") == 0,
          "cache -> cache-hits");
    CHECK(strcmp(wubu_perfmon_metric_for("occup"), "occupancy") == 0,
          "occup -> occupancy");
    CHECK(strcmp(wubu_perfmon_metric_for("mem"), "mem-bandwidth") == 0,
          "mem -> mem-bandwidth");
    CHECK(strcmp(wubu_perfmon_metric_for("zzz"), "cycles") == 0,
          "zzz -> cycles fallback");

    CHECK(strcmp(wubu_perfmon_api_for("amd"), "gpuprofa") == 0,
          "amd -> gpuprofa");
    CHECK(strcmp(wubu_perfmon_api_for("intel"), "i915") == 0,
          "intel -> i915");
    CHECK(strcmp(wubu_perfmon_api_for("nv"), "nvml") == 0,
          "nv -> nvml");
    CHECK(strcmp(wubu_perfmon_api_for("zzz"), "gpuprof") == 0,
          "zzz -> gpuprof fallback");

    char s[256];
    wubu_perfmon_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "perfmon summary generated");

    printf("\n=== PERFMON TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
