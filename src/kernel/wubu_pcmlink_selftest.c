/*
 * wubu_pcmlink_selftest.c -- verifies audio PCM link routing.
 */
#include "wubu_pcmlink.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } \
    else { passes++; } \
} while (0)

int main(void)
{
    int passes = 0, fails = 0;
    wubu_pcmlink_probe();

    CHECK(wubu_pcmlink_present() >= 0, "pcmlink_present returns non-negative");

    CHECK(strcmp(wubu_pcmlink_dir_str(0), "capture") == 0, "dir capture");
    CHECK(strcmp(wubu_pcmlink_dir_str(1), "playback") == 0, "dir playback");

    CHECK(strcmp(wubu_pcmlink_state_str(0), "idle") == 0, "state idle");
    CHECK(strcmp(wubu_pcmlink_state_str(1), "active") == 0, "state active");

    CHECK(wubu_pcmlink_channels("ch8") == 8, "channels 8ch");
    CHECK(wubu_pcmlink_channels("ch2") == 2, "channels 2ch");
    CHECK(wubu_pcmlink_channels("ch1") == 1, "channels 1ch");
    CHECK(wubu_pcmlink_channels(NULL) == 0, "channels null");
    CHECK(wubu_pcmlink_channels("unknown") == 2, "channels default");

    char buf[256];
    wubu_pcmlink_summary(buf, sizeof(buf));
    CHECK(strstr(buf, "pcmlink[") != NULL, "summary has header");
    CHECK(strstr(buf, "cap=") != NULL, "summary has capture");
    CHECK(strstr(buf, "play=") != NULL, "summary has playback");

    printf("=== PCMLINK TESTS: %d passed, %d failed ===\n", passes, fails);
    return fails > 0 ? 1 : 0;
}
