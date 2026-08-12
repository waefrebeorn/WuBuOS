/*
 * wubu_computectx_selftest.c -- verifies compute-context routing.
 */
#include "wubu_computectx.h"
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
    printf("=== wubu_computectx_selftest ===\n\n");
    wubu_hw_detect();
    wubu_computectx_probe();
    printf("  ctx=%d kfd=%d queue=%d opencl=%d cuda=%d\n",
           wubu_computectx_present(), wubu_computectx_kfd(), wubu_computectx_queue(),
           wubu_computectx_opencl(), wubu_computectx_cuda());

    CHECK(strcmp(wubu_computectx_queue_for("compute"), "compute") == 0,
          "compute -> compute");
    CHECK(strcmp(wubu_computectx_queue_for("gfx"), "compute") == 0,
          "gfx -> compute");
    CHECK(strcmp(wubu_computectx_queue_for("dma"), "dma") == 0,
          "dma -> dma");
    CHECK(strcmp(wubu_computectx_queue_for("copy"), "copy") == 0,
          "copy -> copy");
    CHECK(strcmp(wubu_computectx_queue_for("sdma"), "sdma") == 0,
          "sdma -> sdma");
    CHECK(strcmp(wubu_computectx_queue_for("zzz"), "compute") == 0,
          "zzz -> compute fallback");

    CHECK(strcmp(wubu_computectx_prio_for("high"), "high") == 0,
          "high -> high");
    CHECK(strcmp(wubu_computectx_prio_for("rt"), "high") == 0,
          "rt -> high");
    CHECK(strcmp(wubu_computectx_prio_for("low"), "low") == 0,
          "low -> low");
    CHECK(strcmp(wubu_computectx_prio_for("normal"), "normal") == 0,
          "normal -> normal");
    CHECK(strcmp(wubu_computectx_prio_for("zzz"), "normal") == 0,
          "zzz -> normal fallback");

    char s[256];
    wubu_computectx_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "computectx summary generated");

    printf("\n=== COMPUTECTX TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
