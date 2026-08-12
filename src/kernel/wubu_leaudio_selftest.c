/*
 * wubu_leaudio_selftest.c -- verifies Bluetooth LE Audio routing.
 */
#include "wubu_leaudio.h"
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
    printf("=== wubu_leaudio_selftest ===\n");

    wubu_leaudio_probe();

    int p = wubu_leaudio_present();
    CHECK(p == 0 || p == 1, "leaudio present is boolean");

    /* LC3 latency. */
    CHECK(wubu_leaudio_latency(1) == 0, "frame 1 = 0ms latency");
    CHECK(wubu_leaudio_latency(10) == 7, "frame 10 = 7ms latency");
    CHECK(wubu_leaudio_latency(5) == 3, "frame 5 = 3ms latency");

    /* Codec strings. */
    CHECK(strcmp(wubu_leaudio_codec_str(0), "lc3") == 0, "codec 0 = lc3");
    CHECK(strcmp(wubu_leaudio_codec_str(1), "lcecc") == 0, "codec 1 = lcecc");

    /* Summary builds. */
    char out[160] = "";
    wubu_leaudio_summary(out, sizeof(out));
    CHECK(strstr(out, "leaudio[") != NULL, "summary has leaudio fragment");

    printf("\n=== LEAUDIO TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
