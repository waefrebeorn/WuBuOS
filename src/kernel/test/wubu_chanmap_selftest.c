/*
 * wubu_chanmap_selftest.c -- verifies kernel-owned channel-map routing.
 */
#include "wubu_chanmap.h"
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
    printf("=== wubu_chanmap_selftest ===\n\n");
    wubu_hw_detect();
    wubu_chanmap_probe();
    printf("  map=%d stereo=%d 51=%d 71=%d chmap=%d\n",
           wubu_chanmap_present(), wubu_chanmap_stereo(), wubu_chanmap_51(),
           wubu_chanmap_71(), wubu_chanmap_chmap());

    CHECK(strcmp(wubu_chanmap_pos_for("fl"), "front-left") == 0,
          "fl -> front-left");
    CHECK(strcmp(wubu_chanmap_pos_for("fr"), "front-right") == 0,
          "fr -> front-right");
    CHECK(strcmp(wubu_chanmap_pos_for("fc"), "front-center") == 0,
          "fc -> front-center");
    CHECK(strcmp(wubu_chanmap_pos_for("lfe"), "lfe") == 0,
          "lfe -> lfe");
    CHECK(strcmp(wubu_chanmap_pos_for("sl"), "surround-left") == 0,
          "sl -> surround-left");
    CHECK(strcmp(wubu_chanmap_pos_for("sr"), "surround-right") == 0,
          "sr -> surround-right");
    CHECK(strcmp(wubu_chanmap_pos_for("zzz"), "front-left") == 0,
          "zzz -> front-left fallback");

    CHECK(strcmp(wubu_chanmap_layout_for("mono"), "mono") == 0,
          "mono -> mono");
    CHECK(strcmp(wubu_chanmap_layout_for("stereo"), "stereo") == 0,
          "stereo -> stereo");
    CHECK(strcmp(wubu_chanmap_layout_for("5.1"), "5.1") == 0,
          "5.1 -> 5.1");
    CHECK(strcmp(wubu_chanmap_layout_for("7.1"), "7.1") == 0,
          "7.1 -> 7.1");
    CHECK(strcmp(wubu_chanmap_layout_for("zzz"), "stereo") == 0,
          "zzz -> stereo fallback");

    char s[256];
    wubu_chanmap_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "chanmap summary generated");

    printf("\n=== CHANMAP TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
