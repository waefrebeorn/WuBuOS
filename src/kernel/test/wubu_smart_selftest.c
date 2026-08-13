/*
 * wubu_smart_selftest.c -- verifies kernel-owned SMART routing.
 */
#include "wubu_smart.h"
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
    printf("=== wubu_smart_selftest ===\n\n");
    wubu_hw_detect();
    wubu_smart_probe();
    printf("  smart=%d ata=%d nvme=%d health=%d temp=%d\n",
           wubu_smart_present(), wubu_smart_ata(), wubu_smart_nvme(),
           wubu_smart_health(), wubu_smart_temp());

    /* Attr routing. */
    CHECK(strcmp(wubu_smart_attr_for("realloc"), "reallocated-sectors") == 0,
          "realloc -> reallocated-sectors");
    CHECK(strcmp(wubu_smart_attr_for("wear"), "wear-leveling") == 0,
          "wear -> wear-leveling");
    CHECK(strcmp(wubu_smart_attr_for("temp"), "temperature") == 0,
          "temp -> temperature");
    CHECK(strcmp(wubu_smart_attr_for("pending"), "pending-sector") == 0,
          "pending -> pending-sector");
    CHECK(strcmp(wubu_smart_attr_for("uncorrect"), "uncorrectable") == 0,
          "uncorrect -> uncorrectable");
    CHECK(strcmp(wubu_smart_attr_for("zzz"), "unknown") == 0,
          "zzz -> unknown fallback");

    /* Status routing. */
    CHECK(strcmp(wubu_smart_status_for("pass"), "ok") == 0,
          "pass -> ok");
    CHECK(strcmp(wubu_smart_status_for("ok"), "ok") == 0,
          "ok -> ok");
    CHECK(strcmp(wubu_smart_status_for("warn"), "warning") == 0,
          "warn -> warning");
    CHECK(strcmp(wubu_smart_status_for("fail"), "critical") == 0,
          "fail -> critical");
    CHECK(strcmp(wubu_smart_status_for("zzz"), "unknown") == 0,
          "zzz -> unknown fallback");

    char s[256];
    wubu_smart_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "smart summary generated");

    printf("\n=== SMART TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
