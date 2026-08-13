/*
 * wubu_qos_selftest.c -- verifies kernel-owned QoS/ACL routing.
 */
#include "wubu_qos.h"
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
    printf("=== wubu_qos_selftest ===\n\n");

    wubu_hw_detect();
    wubu_qos_probe();

    printf("  tc=%d offload=%d flower=%d shaping=%d ecn=%d\n",
           wubu_qos_tc(), wubu_qos_offload(), wubu_qos_flower(),
           wubu_qos_shaping(), wubu_qos_ecn());

    /* QoS driver routing is always consistent. */
    CHECK(strcmp(wubu_qos_driver_for("mlxsw"), "mlxsw") == 0,
          "mlxsw -> mlxsw");
    CHECK(strcmp(wubu_qos_driver_for("spectrum"), "mlxsw") == 0,
          "spectrum -> mlxsw");
    CHECK(strcmp(wubu_qos_driver_for("felix"), "felix") == 0,
          "felix -> felix");
    CHECK(strcmp(wubu_qos_driver_for("ocelot"), "felix") == 0,
          "ocelot -> felix");
    CHECK(strcmp(wubu_qos_driver_for("mv88e"), "mv88e6xxx") == 0,
          "mv88e -> mv88e6xxx");
    CHECK(strcmp(wubu_qos_driver_for("ksz"), "ksz") == 0,
          "ksz -> ksz");
    CHECK(strcmp(wubu_qos_driver_for("unknown"), "net-core") == 0,
          "unknown -> net-core fallback");

    char s[256];
    wubu_qos_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "qos summary generated");

    printf("\n=== QOS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
