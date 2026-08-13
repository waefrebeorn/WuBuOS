/*
 * wubu_fantml_selftest.c -- verifies GPU fan + thermal routing.
 */
#include "wubu_fantml.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } \
    else { passes++; } \
} while (0)

int main(void)
{
    int passes = 0, fails = 0;
    wubu_fantml_probe();

    CHECK(wubu_fantml_present() >= 0, "fantml_present returns non-negative");

    CHECK(strcmp(wubu_fantml_status_str(30), "cool") == 0, "temp cool");
    CHECK(strcmp(wubu_fantml_status_str(60), "normal") == 0, "temp normal");
    CHECK(strcmp(wubu_fantml_status_str(80), "warm") == 0, "temp warm");
    CHECK(strcmp(wubu_fantml_status_str(90), "hot") == 0, "temp hot");
    CHECK(strcmp(wubu_fantml_status_str(105), "critical") == 0, "temp critical");

    CHECK(wubu_fantml_fan_pct(0, 1000) == 0, "fan pct 0 rpm");
    CHECK(wubu_fantml_fan_pct(500, 1000) == 50, "fan pct 50%");
    CHECK(wubu_fantml_fan_pct(1000, 1000) == 100, "fan pct 100%");
    CHECK(wubu_fantml_fan_pct(500, 0) == 0, "fan pct zero max_rpm");

    char buf[256];
    wubu_fantml_summary(buf, sizeof(buf));
    CHECK(strstr(buf, "fantml[") != NULL, "summary has header");
    CHECK(strstr(buf, "temp=") != NULL, "summary has temp");
    CHECK(strstr(buf, "rpm=") != NULL, "summary has rpm");

    printf("=== FANTML TESTS: %d passed, %d failed ===\n", passes, fails);
    return fails > 0 ? 1 : 0;
}
