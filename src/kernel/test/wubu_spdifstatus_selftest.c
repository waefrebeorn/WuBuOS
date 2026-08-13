/*
 * wubu_spdifstatus_selftest.c -- verifies SPDIF status routing.
 */
#include "wubu_spdifstatus.h"
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
    printf("=== wubu_spdifstatus_selftest ===\n\n");
    wubu_hw_detect();
    wubu_spdifstatus_probe();
    printf("  sp=%d lock=%d valid=%d aes=%d rate=%d\n",
           wubu_spdifstatus_present(), wubu_spdifstatus_lock(),
           wubu_spdifstatus_valid(), wubu_spdifstatus_aes(),
           wubu_spdifstatus_rate());

    CHECK(strcmp(wubu_spdifstatus_rate_for("44.1"), "44.1kHz") == 0,
          "44.1 -> 44.1kHz");
    CHECK(strcmp(wubu_spdifstatus_rate_for("44100"), "44.1kHz") == 0,
          "44100 -> 44.1kHz");
    CHECK(strcmp(wubu_spdifstatus_rate_for("48"), "48kHz") == 0,
          "48 -> 48kHz");
    CHECK(strcmp(wubu_spdifstatus_rate_for("96"), "96kHz") == 0,
          "96 -> 96kHz");
    CHECK(strcmp(wubu_spdifstatus_rate_for("192"), "192kHz") == 0,
          "192 -> 192kHz");
    CHECK(strcmp(wubu_spdifstatus_rate_for("32"), "32kHz") == 0,
          "32 -> 32kHz");
    CHECK(strcmp(wubu_spdifstatus_rate_for("88.2"), "88.2kHz") == 0,
          "88.2 -> 88.2kHz");
    CHECK(strcmp(wubu_spdifstatus_rate_for("zzz"), "48kHz") == 0,
          "zzz -> 48kHz fallback");

    CHECK(strcmp(wubu_spdifstatus_lock_for("lock"), "locked") == 0,
          "lock -> locked");
    CHECK(strcmp(wubu_spdifstatus_lock_for("detect"), "locked") == 0,
          "detect -> locked");
    CHECK(strcmp(wubu_spdifstatus_lock_for("unl"), "unlocked") == 0,
          "unl -> unlocked");
    CHECK(strcmp(wubu_spdifstatus_lock_for("nol"), "unlocked") == 0,
          "nol -> unlocked");
    CHECK(strcmp(wubu_spdifstatus_lock_for("valid"), "valid") == 0,
          "valid -> valid");
    CHECK(strcmp(wubu_spdifstatus_lock_for("zzz"), "unlocked") == 0,
          "zzz -> unlocked fallback");

    char s[256];
    wubu_spdifstatus_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "spdifstatus summary generated");

    printf("\n=== SPDIFSTATUS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
