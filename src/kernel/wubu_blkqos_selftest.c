/*
 * wubu_blkqos_selftest.c -- verifies blk-QoS throttling routing.
 */
#include "wubu_blkqos.h"
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
    printf("=== wubu_blkqos_selftest ===\n\n");
    wubu_hw_detect();
    wubu_blkqos_probe();
    printf("  qos=%d throttle=%d weight=%d cg=%d limit=%d\n",
           wubu_blkqos_present(), wubu_blkqos_throttle(), wubu_blkqos_weight(),
           wubu_blkqos_cg(), wubu_blkqos_limit());

    CHECK(strcmp(wubu_blkqos_mode_for("latency"), "latency") == 0,
          "latency -> latency");
    CHECK(strcmp(wubu_blkqos_mode_for("cost_model"), "cost-model") == 0,
          "cost_model -> cost-model");
    CHECK(strcmp(wubu_blkqos_mode_for("throttle"), "throttle") == 0,
          "throttle -> throttle");
    CHECK(strcmp(wubu_blkqos_mode_for("weight"), "weight") == 0,
          "weight -> weight");
    CHECK(strcmp(wubu_blkqos_mode_for("zzz"), "throttle") == 0,
          "zzz -> throttle fallback");
    CHECK(strcmp(wubu_blkqos_mode_for("throt"), "throttle") == 0,
          "throt -> throttle (substring)");

    CHECK(strcmp(wubu_blkqos_unit_for("b/s"), "bytes") == 0,
          "b/s -> bytes");
    CHECK(strcmp(wubu_blkqos_unit_for("iops"), "iops") == 0,
          "iops -> iops");
    CHECK(strcmp(wubu_blkqos_unit_for("kbps"), "kbps") == 0,
          "kbps -> kbps");
    CHECK(strcmp(wubu_blkqos_unit_for("mbps"), "mbps") == 0,
          "mbps -> mbps");
    CHECK(strcmp(wubu_blkqos_unit_for("zzz"), "bytes") == 0,
          "zzz -> bytes fallback");

    char s[256];
    wubu_blkqos_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "blkqos summary generated");

    printf("\n=== BLKQOS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
