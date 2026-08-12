/*
 * wubu_ducking_selftest.c -- verifies kernel-owned ducking routing.
 */
#include "wubu_ducking.h"
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
    printf("=== wubu_ducking_selftest ===\n\n");
    wubu_hw_detect();
    wubu_ducking_probe();
    printf("  ducking=%d comp=%d limiter=%d gate=%d sidechain=%d\n",
           wubu_ducking_present(), wubu_ducking_comp(), wubu_ducking_limiter(),
           wubu_ducking_gate(), wubu_ducking_sidechain());

    CHECK(strcmp(wubu_ducking_type_for("ducking"), "ducking") == 0,
          "ducking -> ducking");
    CHECK(strcmp(wubu_ducking_type_for("duck"), "ducking") == 0,
          "duck -> ducking");
    CHECK(strcmp(wubu_ducking_type_for("compress"), "compressor") == 0,
          "compress -> compressor");
    CHECK(strcmp(wubu_ducking_type_for("comp"), "compressor") == 0,
          "comp -> compressor");
    CHECK(strcmp(wubu_ducking_type_for("limiter"), "limiter") == 0,
          "limiter -> limiter");
    CHECK(strcmp(wubu_ducking_type_for("gate"), "noise-gate") == 0,
          "gate -> noise-gate");
    CHECK(strcmp(wubu_ducking_type_for("expander"), "expander") == 0,
          "expander -> expander");
    CHECK(strcmp(wubu_ducking_type_for("zzz"), "compressor") == 0,
          "zzz -> compressor fallback");

    CHECK(strcmp(wubu_ducking_mode_for("up"), "upward") == 0,
          "up -> upward");
    CHECK(strcmp(wubu_ducking_mode_for("down"), "downward") == 0,
          "down -> downward");
    CHECK(strcmp(wubu_ducking_mode_for("side"), "sidechain") == 0,
          "side -> sidechain");
    CHECK(strcmp(wubu_ducking_mode_for("zzz"), "auto") == 0,
          "zzz -> auto fallback");

    char s[256];
    wubu_ducking_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "ducking summary generated");

    printf("\n=== DUCKING TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
