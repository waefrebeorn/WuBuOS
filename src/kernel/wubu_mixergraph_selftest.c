/*
 * wubu_mixergraph_selftest.c -- verifies mixer graph routing.
 */
#include "wubu_mixergraph.h"
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
    printf("=== wubu_mixergraph_selftest ===\n\n");
    wubu_hw_detect();
    wubu_mixergraph_probe();
    printf("  mix=%d pb=%d cap=%d mon=%d groups=%d\n",
           wubu_mixergraph_present(), wubu_mixergraph_pb(),
           wubu_mixergraph_cap(), wubu_mixergraph_mon(),
           wubu_mixergraph_groups());

    CHECK(strcmp(wubu_mixergraph_path_for("pb"), "playback") == 0,
          "pb -> playback");
    CHECK(strcmp(wubu_mixergraph_path_for("play"), "playback") == 0,
          "play -> playback");
    CHECK(strcmp(wubu_mixergraph_path_for("cap"), "capture") == 0,
          "cap -> capture");
    CHECK(strcmp(wubu_mixergraph_path_for("capt"), "capture") == 0,
          "capt -> capture");
    CHECK(strcmp(wubu_mixergraph_path_for("record"), "capture") == 0,
          "record -> capture");
    CHECK(strcmp(wubu_mixergraph_path_for("mon"), "monitor") == 0,
          "mon -> monitor");
    CHECK(strcmp(wubu_mixergraph_path_for("monitor"), "monitor") == 0,
          "monitor -> monitor");
    CHECK(strcmp(wubu_mixergraph_path_for("be"), "loopback") == 0,
          "be -> loopback");
    CHECK(strcmp(wubu_mixergraph_path_for("loopback"), "loopback") == 0,
          "loopback -> loopback");
    CHECK(strcmp(wubu_mixergraph_path_for("zzz"), "playback") == 0,
          "zzz -> playback fallback");

    CHECK(strcmp(wubu_mixergraph_group_for("master"), "Master") == 0,
          "master -> Master");
    CHECK(strcmp(wubu_mixergraph_group_for("pcm"), "PCM") == 0,
          "pcm -> PCM");
    CHECK(strcmp(wubu_mixergraph_group_for("cd"), "CD") == 0,
          "cd -> CD");
    CHECK(strcmp(wubu_mixergraph_group_for("mic"), "Mic") == 0,
          "mic -> Mic");
    CHECK(strcmp(wubu_mixergraph_group_for("line"), "Line") == 0,
          "line -> Line");
    CHECK(strcmp(wubu_mixergraph_group_for("ig"), "Beep") == 0,
          "ig -> Beep");
    CHECK(strcmp(wubu_mixergraph_group_for("zzz"), "Master") == 0,
          "zzz -> Master fallback");

    char s[256];
    wubu_mixergraph_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "mixergraph summary generated");

    printf("\n=== MIXERGRAPH TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
