/*
 * wubu_btaudio_selftest.c -- verifies Bluetooth audio profile routing.
 */
#include "wubu_btaudio.h"
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
    printf("=== wubu_btaudio_selftest ===\n");

    wubu_btaudio_probe();

    int p = wubu_btaudio_present();
    CHECK(p == 0 || p == 1, "btaudio present is boolean");

    /* Auto-select: <20ms = voice (1), >=20ms = media (0). */
    CHECK(wubu_btaudio_auto(10) == 1, "latency 10ms = voice");
    CHECK(wubu_btaudio_auto(100) == 0, "latency 100ms = media");
    CHECK(wubu_btaudio_auto(0) == 1, "latency 0 = voice");
    CHECK(wubu_btaudio_auto(-1) == 1, "latency -1 = voice (clamped)");

    /* Profile strings. */
    CHECK(strcmp(wubu_btaudio_profile_str(0), "none") == 0, "profile 0 = none");
    CHECK(strcmp(wubu_btaudio_profile_str(1), "a2dp") == 0, "profile 1 = a2dp");
    CHECK(strcmp(wubu_btaudio_profile_str(2), "hsp/hfp") == 0, "profile 2 = hsp/hfp");
    CHECK(strcmp(wubu_btaudio_profile_str(3), "sco") == 0, "profile 3 = sco");

    /* Summary builds. */
    char out[160] = "";
    wubu_btaudio_summary(out, sizeof(out));
    CHECK(strstr(out, "btaudio[") != NULL, "summary has btaudio fragment");

    printf("\n=== BTAUDIO TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
