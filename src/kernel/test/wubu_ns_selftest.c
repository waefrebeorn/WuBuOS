/*
 * wubu_ns_selftest.c -- verifies kernel-owned NVMe-namespace routing.
 */
#include "wubu_ns.h"
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
    printf("=== wubu_ns_selftest ===\n\n");

    wubu_hw_detect();
    wubu_ns_probe();

    printf("  nvme=%d ns=%d mpath=%d ana=%d cli=%d\n",
           wubu_ns_nvme(), wubu_ns_namespace(), wubu_ns_multipath(),
           wubu_ns_ana(), wubu_ns_cli());

    /* Namespace path routing. */
    CHECK(strcmp(wubu_ns_path_for("primary"), "primary") == 0,
          "primary -> primary");
    CHECK(strcmp(wubu_ns_path_for("secondary"), "secondary") == 0,
          "secondary -> secondary");
    CHECK(strcmp(wubu_ns_path_for("multipath"), "multipath") == 0,
          "multipath -> multipath");
    CHECK(strcmp(wubu_ns_path_for("unknown"), "ns") == 0,
          "unknown -> ns fallback");

    /* Namespace state routing. */
    CHECK(strcmp(wubu_ns_state_for("live"), "live") == 0,
          "live -> live");
    CHECK(strcmp(wubu_ns_state_for("offline"), "offline") == 0,
          "offline -> offline");
    CHECK(strcmp(wubu_ns_state_for("read-only"), "read-only") == 0,
          "read-only -> read-only");
    CHECK(strcmp(wubu_ns_state_for("unknown"), "live") == 0,
          "unknown -> live fallback");

    char s[256];
    wubu_ns_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "ns summary generated");

    printf("\n=== NS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
