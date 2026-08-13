/*
 * wubu_compressor_selftest.c -- verifies compressor routing.
 */
#include "wubu_compressor.h"
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
    printf("=== wubu_compressor_selftest ===\n\n");
    wubu_hw_detect();
    wubu_compressor_probe();
    printf("  comp=%d thresh=%d ratio=%d attack=%d release=%d\n",
           wubu_compressor_present(), wubu_compressor_thresh(),
           wubu_compressor_ratio(), wubu_compressor_attack(),
           wubu_compressor_release());

    CHECK(strcmp(wubu_compressor_ratio_for("2:1"), "2:1") == 0,
          "2:1 -> 2:1");
    CHECK(strcmp(wubu_compressor_ratio_for("4:1"), "4:1") == 0,
          "4:1 -> 4:1");
    CHECK(strcmp(wubu_compressor_ratio_for("8:1"), "8:1") == 0,
          "8:1 -> 8:1");
    CHECK(strcmp(wubu_compressor_ratio_for("inf"), "limiter") == 0,
          "inf -> limiter");
    CHECK(strcmp(wubu_compressor_ratio_for("limit"), "limiter") == 0,
          "limit -> limiter");
    CHECK(strcmp(wubu_compressor_ratio_for("3:1"), "3:1") == 0,
          "3:1 -> 3:1");
    CHECK(strcmp(wubu_compressor_ratio_for("1:1"), "1:1") == 0,
          "1:1 -> 1:1");
    CHECK(strcmp(wubu_compressor_ratio_for("zzz"), "4:1") == 0,
          "zzz -> 4:1 fallback");

    CHECK(strcmp(wubu_compressor_knee_for("hard"), "hard") == 0,
          "hard -> hard");
    CHECK(strcmp(wubu_compressor_knee_for("soft"), "soft") == 0,
          "soft -> soft");
    CHECK(strcmp(wubu_compressor_knee_for("medium"), "medium") == 0,
          "medium -> medium");
    CHECK(strcmp(wubu_compressor_knee_for("zzz"), "soft") == 0,
          "zzz -> soft fallback");

    char s[256];
    wubu_compressor_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "compressor summary generated");

    printf("\n=== COMPRESSOR TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
