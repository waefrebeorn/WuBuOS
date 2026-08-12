/*
 * wubu_dspmode_selftest.c -- verifies kernel-owned DSP-mode routing.
 */
#include "wubu_dspmode.h"
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
    printf("=== wubu_dspmode_selftest ===\n\n");

    wubu_hw_detect();
    wubu_dspmode_probe();

    printf("  sof=%d pm=%d voice_wake=%d suspend=%d\n",
           wubu_dspmode_sof(), wubu_dspmode_pm(),
           wubu_dspmode_voice_wake(), wubu_dspmode_suspend_ok());

    /* DSP mode routing is always consistent. */
    CHECK(strcmp(wubu_dspmode_for("voice"), "voice-trigger") == 0,
          "voice -> voice-trigger");
    CHECK(strcmp(wubu_dspmode_for("wake"), "voice-trigger") == 0,
          "wake -> voice-trigger");
    CHECK(strcmp(wubu_dspmode_for("low-power"), "low-power") == 0,
          "low-power -> low-power");
    CHECK(strcmp(wubu_dspmode_for("suspend"), "suspend") == 0,
          "suspend -> suspend");
    CHECK(strcmp(wubu_dspmode_for("active"), "active") == 0,
          "active -> active");

    char s[256];
    wubu_dspmode_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "dspmode summary generated");

    printf("\n=== DSPMODE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
