/*
 * wubu_thermalthrottle_selftest.c -- verifies thermal throttling routing.
 */
#include "wubu_thermalthrottle.h"
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
    printf("=== wubu_thermalthrottle_selftest ===\n\n");
    wubu_hw_detect();
    wubu_thermalthrottle_probe();
    printf("  throttle=%d zone=%d cooling=%d trip=%d governor=%d\n",
           wubu_thermalthrottle_present(), wubu_thermalthrottle_zone(),
           wubu_thermalthrottle_cooling(), wubu_thermalthrottle_trip(),
           wubu_thermalthrottle_governor());

    CHECK(strcmp(wubu_thermalthrottle_gov_for("step"), "step_wise") == 0,
          "step -> step_wise");
    CHECK(strcmp(wubu_thermalthrottle_gov_for("fair"), "fair_share") == 0,
          "fair -> fair_share");
    CHECK(strcmp(wubu_thermalthrottle_gov_for("user"), "user_space") == 0,
          "user -> user_space");
    CHECK(strcmp(wubu_thermalthrottle_gov_for("bang"), "bang_bang") == 0,
          "bang -> bang_bang");
    CHECK(strcmp(wubu_thermalthrottle_gov_for("zzz"), "step_wise") == 0,
          "zzz -> step_wise fallback");

    CHECK(strcmp(wubu_thermalthrottle_trip_for("crit"), "critical") == 0,
          "crit -> critical");
    CHECK(strcmp(wubu_thermalthrottle_trip_for("hot"), "hot") == 0,
          "hot -> hot");
    CHECK(strcmp(wubu_thermalthrottle_trip_for("pass"), "passive") == 0,
          "pass -> passive");
    CHECK(strcmp(wubu_thermalthrottle_trip_for("active"), "active") == 0,
          "active -> active");
    CHECK(strcmp(wubu_thermalthrottle_trip_for("zzz"), "passive") == 0,
          "zzz -> passive fallback");

    char s[256];
    wubu_thermalthrottle_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "thermalthrottle summary generated");

    printf("\n=== THERMALTHROTTLE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
