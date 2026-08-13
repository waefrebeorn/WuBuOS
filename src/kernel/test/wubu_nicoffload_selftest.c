/*
 * wubu_nicoffload_selftest.c -- verifies kernel-owned NIC offload routing.
 */
#include "wubu_nicoffload.h"
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
    printf("=== wubu_nicoffload_selftest ===\n\n");

    wubu_hw_detect();
    wubu_nicoffload_probe();

    printf("  nic=%d queues=%d tso=%d gro=%d rss=%d mq=%d\n",
           wubu_nicoffload_present(), wubu_nicoffload_queues(),
           wubu_nicoffload_tso(), wubu_nicoffload_gro(),
           wubu_nicoffload_rss(), wubu_nicoffload_multi_queue());

    /* Offload driver routing is always consistent. */
    CHECK(strcmp(wubu_nicoffload_driver_for("ixgbe"), "ixgbe") == 0,
          "ixgbe -> ixgbe");
    CHECK(strcmp(wubu_nicoffload_driver_for("i40e"), "i40e") == 0,
          "i40e -> i40e");
    CHECK(strcmp(wubu_nicoffload_driver_for("igc"), "igc") == 0,
          "igc -> igc");
    CHECK(strcmp(wubu_nicoffload_driver_for("ice"), "ice") == 0,
          "ice -> ice");
    CHECK(strcmp(wubu_nicoffload_driver_for("mlx5"), "mlx5") == 0,
          "mlx5 -> mlx5");
    CHECK(strcmp(wubu_nicoffload_driver_for("bnxt"), "bnxt") == 0,
          "bnxt -> bnxt");
    CHECK(strcmp(wubu_nicoffload_driver_for("e1000e"), "e1000e") == 0,
          "e1000e -> e1000e");
    CHECK(strcmp(wubu_nicoffload_driver_for("unknown"), "net-core") == 0,
          "unknown -> net-core fallback");

    /* Multi-queue implies RSS. */
    CHECK(!wubu_nicoffload_multi_queue() || wubu_nicoffload_rss(),
          "multi-queue -> RSS");

    char s[256];
    wubu_nicoffload_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "nicoffload summary generated");

    printf("\n=== NICOFFLOAD TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
