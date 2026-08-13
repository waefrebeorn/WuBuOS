/*
 * wubu_jackdetect_selftest.c -- verifies jack-detection routing.
 */
#include "wubu_jackdetect.h"
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
    printf("=== wubu_jackdetect_selftest ===\n\n");
    wubu_hw_detect();
    wubu_jackdetect_probe();
    printf("  jack=%d headset=%d mic=%d omtp=%d ctia=%d\n",
           wubu_jackdetect_present(), wubu_jackdetect_headset(), wubu_jackdetect_mic(),
           wubu_jackdetect_omtp(), wubu_jackdetect_ctia());

    CHECK(strcmp(wubu_jackdetect_pinout_for("omtp"), "omtp") == 0,
          "omtp -> omtp");
    CHECK(strcmp(wubu_jackdetect_pinout_for("ctia"), "ctia") == 0,
          "ctia -> ctia");
    CHECK(strcmp(wubu_jackdetect_pinout_for("ymck"), "ymck") == 0,
          "ymck -> ymck");
    CHECK(strcmp(wubu_jackdetect_pinout_for("zzz"), "ctia") == 0,
          "zzz -> ctia fallback");

    CHECK(strcmp(wubu_jackdetect_state_for("insert"), "inserted") == 0,
          "insert -> inserted");
    CHECK(strcmp(wubu_jackdetect_state_for("plug"), "inserted") == 0,
          "plug -> inserted");
    CHECK(strcmp(wubu_jackdetect_state_for("remov"), "removed") == 0,
          "remov -> removed");
    CHECK(strcmp(wubu_jackdetect_state_for("mic"), "mic-present") == 0,
          "mic -> mic-present");
    CHECK(strcmp(wubu_jackdetect_state_for("no-mic"), "no-mic") == 0,
          "no-mic -> no-mic");
    CHECK(strcmp(wubu_jackdetect_state_for("zzz"), "removed") == 0,
          "zzz -> removed fallback");

    char s[256];
    wubu_jackdetect_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "jackdetect summary generated");

    printf("\n=== JACKDETECT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
