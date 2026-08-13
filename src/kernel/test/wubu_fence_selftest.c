/*
 * wubu_fence_selftest.c -- verifies kernel-owned GPU fence routing.
 */
#include "wubu_fence.h"
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
    printf("=== wubu_fence_selftest ===\n\n");
    wubu_hw_detect();
    wubu_fence_probe();
    printf("  fence=%d timeout=%d signal=%d amd=%d i915=%d\n",
           wubu_fence_present(), wubu_fence_timeout(), wubu_fence_signal(),
           wubu_fence_amd(), wubu_fence_i915());

    CHECK(strcmp(wubu_fence_type_for("dma"), "dma-fence") == 0,
          "dma -> dma-fence");
    CHECK(strcmp(wubu_fence_type_for("gpu"), "gpu-fence") == 0,
          "gpu -> gpu-fence");
    CHECK(strcmp(wubu_fence_type_for("sched"), "sched-fence") == 0,
          "sched -> sched-fence");
    CHECK(strcmp(wubu_fence_type_for("zzz"), "dma-fence") == 0,
          "zzz -> dma-fence fallback");

    CHECK(strcmp(wubu_fence_action_for("wait"), "wait") == 0,
          "wait -> wait");
    CHECK(strcmp(wubu_fence_action_for("signal"), "signal") == 0,
          "signal -> signal");
    CHECK(strcmp(wubu_fence_action_for("timeout"), "timeout") == 0,
          "timeout -> timeout");
    CHECK(strcmp(wubu_fence_action_for("reset"), "reset") == 0,
          "reset -> reset");
    CHECK(strcmp(wubu_fence_action_for("zzz"), "wait") == 0,
          "zzz -> wait fallback");

    char s[256];
    wubu_fence_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "fence summary generated");

    printf("\n=== FENCE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
