/*
 * wubu_jack_selftest.c -- verifies audio jack detection routing.
 */
#include "wubu_jack.h"
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
    printf("=== wubu_jack_selftest ===\n\n");
    wubu_hw_detect();
    wubu_jack_probe();
    printf("  jack=%d hp=%d mic=%d spdif=%d imp=%d\n",
           wubu_jack_present(), wubu_jack_headphone(),
           wubu_jack_mic(), wubu_jack_spdif(),
           wubu_jack_impedance());

    CHECK(strcmp(wubu_jack_type_for("headphone"), "headphone") == 0,
          "headphone -> headphone");
    CHECK(strcmp(wubu_jack_type_for("headset"), "headset") == 0,
          "headset -> headset");
    CHECK(strcmp(wubu_jack_type_for("mic"), "mic") == 0,
          "mic -> mic");
    CHECK(strcmp(wubu_jack_type_for("line"), "line") == 0,
          "line -> line");
    CHECK(strcmp(wubu_jack_type_for("spdif"), "spdif") == 0,
          "spdif -> spdif");
    CHECK(strcmp(wubu_jack_type_for("cd"), "cd") == 0,
          "cd -> cd");
    CHECK(strcmp(wubu_jack_type_for("zzz"), "headphone") == 0,
          "zzz -> headphone fallback");

    CHECK(strcmp(wubu_jack_state_for("plug"), "plugged") == 0,
          "plug -> plugged");
    CHECK(strcmp(wubu_jack_state_for("unplug"), "unplugged") == 0,
          "unplug -> unplugged");
    CHECK(strcmp(wubu_jack_state_for("present"), "present") == 0,
          "present -> present");
    CHECK(strcmp(wubu_jack_state_for("absent"), "absent") == 0,
          "absent -> absent");
    CHECK(strcmp(wubu_jack_state_for("zzz"), "absent") == 0,
          "zzz -> absent fallback");

    char s[256];
    wubu_jack_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "jack summary generated");

    printf("\n=== JACK TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
