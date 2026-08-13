/*
 * wubu_jackstate_selftest.c -- verifies jack-state routing.
 */
#include "wubu_jackstate.h"
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
    printf("=== wubu_jackstate_selftest ===\n\n");
    wubu_hw_detect();
    wubu_jackstate_probe();
    printf("  js=%d plug=%d unplug=%d debounce=%d stable=%d\n",
           wubu_jackstate_present(), wubu_jackstate_plug(), wubu_jackstate_unplug(),
           wubu_jackstate_debounce(), wubu_jackstate_stable());

    CHECK(strcmp(wubu_jackstate_machine_for("unplug"), "unplugged") == 0,
          "unplug -> unplugged");
    CHECK(strcmp(wubu_jackstate_machine_for("removed"), "unplugged") == 0,
          "removed -> unplugged");
    CHECK(strcmp(wubu_jackstate_machine_for("plug"), "plugged") == 0,
          "plug -> plugged");
    CHECK(strcmp(wubu_jackstate_machine_for("insert"), "plugged") == 0,
          "insert -> plugged");
    CHECK(strcmp(wubu_jackstate_machine_for("pluggin"), "plugging") == 0,
          "pluggin -> plugging");
    CHECK(strcmp(wubu_jackstate_machine_for("unpluggin"), "unplugging") == 0,
          "unpluggin -> unplugging");
    CHECK(strcmp(wubu_jackstate_machine_for("bounc"), "bounce") == 0,
          "bounc -> bounce");
    CHECK(strcmp(wubu_jackstate_machine_for("zzz"), "unplugged") == 0,
          "zzz -> unplugged fallback");

    CHECK(strcmp(wubu_jackstate_event_for("in"), "plug_in") == 0,
          "in -> plug_in");
    CHECK(strcmp(wubu_jackstate_event_for("insert"), "plug_in") == 0,
          "insert -> plug_in");
    CHECK(strcmp(wubu_jackstate_event_for("plug"), "plug_in") == 0,
          "plug -> plug_in");
    CHECK(strcmp(wubu_jackstate_event_for("out"), "plug_out") == 0,
          "out -> plug_out");
    CHECK(strcmp(wubu_jackstate_event_for("remov"), "plug_out") == 0,
          "remov -> plug_out");
    CHECK(strcmp(wubu_jackstate_event_for("unplug"), "plug_out") == 0,
          "unplug -> plug_out");
    CHECK(strcmp(wubu_jackstate_event_for("zzz"), "plug_out") == 0,
          "zzz -> plug_out fallback");

    char s[256];
    wubu_jackstate_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "jackstate summary generated");

    printf("\n=== JACKSTATE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
