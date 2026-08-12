/*
 * wubu_uac_selftest.c -- verifies kernel-owned UAC routing.
 */
#include "wubu_uac.h"
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
    printf("=== wubu_uac_selftest ===\n\n");
    wubu_hw_detect();
    wubu_uac_probe();
    printf("  uac=%d uac1=%d uac2=%d iso=%d alt=%d\n",
           wubu_uac_present(), wubu_uac_uac1(), wubu_uac_uac2(),
           wubu_uac_iso(), wubu_uac_alt());

    CHECK(strcmp(wubu_uac_version_for("01"), "uac1") == 0,
          "01 -> uac1");
    CHECK(strcmp(wubu_uac_version_for("02"), "uac2") == 0,
          "02 -> uac2");
    CHECK(strcmp(wubu_uac_version_for("03"), "uac3") == 0,
          "03 -> uac3");
    CHECK(strcmp(wubu_uac_version_for("zzz"), "uac2") == 0,
          "zzz -> uac2 fallback");

    CHECK(strcmp(wubu_uac_ep_for("bulk"), "bulk") == 0,
          "bulk -> bulk");
    CHECK(strcmp(wubu_uac_ep_for("interrupt"), "interrupt") == 0,
          "interrupt -> interrupt");
    CHECK(strcmp(wubu_uac_ep_for("iso-in"), "iso-in") == 0,
          "iso-in -> iso-in");
    CHECK(strcmp(wubu_uac_ep_for("iso-out"), "iso-out") == 0,
          "iso-out -> iso-out");
    CHECK(strcmp(wubu_uac_ep_for("zzz"), "bulk") == 0,
          "zzz -> bulk fallback");

    char s[256];
    wubu_uac_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "uac summary generated");

    printf("\n=== UAC TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
