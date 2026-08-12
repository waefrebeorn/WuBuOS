/*
 * wubu_devmapper_selftest.c -- verifies DM routing.
 */
#include "wubu_devmapper.h"
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
    printf("=== wubu_devmapper_selftest ===\n\n");
    wubu_hw_detect();
    wubu_devmapper_probe();
    printf("  dm=%d linear=%d stripe=%d mirror=%d snapshot=%d\n",
           wubu_devmapper_present(), wubu_devmapper_linear(),
           wubu_devmapper_stripe(), wubu_devmapper_mirror(),
           wubu_devmapper_snapshot());

    CHECK(strcmp(wubu_devmapper_target_for("linear"), "linear") == 0,
          "linear -> linear");
    CHECK(strcmp(wubu_devmapper_target_for("stripe"), "stripe") == 0,
          "stripe -> stripe");
    CHECK(strcmp(wubu_devmapper_target_for("mirror"), "mirror") == 0,
          "mirror -> mirror");
    CHECK(strcmp(wubu_devmapper_target_for("snapshot"), "snapshot") == 0,
          "snapshot -> snapshot");
    CHECK(strcmp(wubu_devmapper_target_for("thin"), "thin") == 0,
          "thin -> thin");
    CHECK(strcmp(wubu_devmapper_target_for("crypt"), "crypt") == 0,
          "crypt -> crypt");
    CHECK(strcmp(wubu_devmapper_target_for("multipath"), "multipath") == 0,
          "multipath -> multipath");
    CHECK(strcmp(wubu_devmapper_target_for("zzz"), "linear") == 0,
          "zzz -> linear fallback");

    CHECK(strcmp(wubu_devmapper_mode_for("read"), "read") == 0,
          "read -> read");
    CHECK(strcmp(wubu_devmapper_mode_for("write"), "write") == 0,
          "write -> write");
    CHECK(strcmp(wubu_devmapper_mode_for("read-write"), "read-write") == 0,
          "read-write -> read-write");
    CHECK(strcmp(wubu_devmapper_mode_for("rw"), "read-write") == 0,
          "rw -> read-write");
    CHECK(strcmp(wubu_devmapper_mode_for("ro"), "read-only") == 0,
          "ro -> read-only");
    CHECK(strcmp(wubu_devmapper_mode_for("read-only"), "read-only") == 0,
          "read-only -> read-only");
    CHECK(strcmp(wubu_devmapper_mode_for("zzz"), "read-write") == 0,
          "zzz -> read-write fallback");

    char s[256];
    wubu_devmapper_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "devmapper summary generated");

    printf("\n=== DEVMAPPER TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
