/*
 * wubu_vblank_selftest.c -- verifies GPU VBLANK routing.
 */
#include "wubu_vblank.h"
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
    printf("=== wubu_vblank_selftest ===\n\n");
    wubu_hw_detect();
    wubu_vblank_probe();
    printf("  vbl=%d counter=%d event=%d time=%d flip=%d\n",
           wubu_vblank_present(), wubu_vblank_counter(),
           wubu_vblank_event(), wubu_vblank_time(), wubu_vblank_flip());

    CHECK(strcmp(wubu_vblank_src_for("crtc"), "CRTC") == 0,
          "crtc -> CRTC");
    CHECK(strcmp(wubu_vblank_src_for("connector"), "Connector") == 0,
          "connector -> Connector");
    CHECK(strcmp(wubu_vblank_src_for("encoder"), "Encoder") == 0,
          "encoder -> Encoder");
    CHECK(strcmp(wubu_vblank_src_for("plane"), "Plane") == 0,
          "plane -> Plane");
    CHECK(strcmp(wubu_vblank_src_for("primary"), "Primary") == 0,
          "primary -> Primary");
    CHECK(strcmp(wubu_vblank_src_for("cursor"), "Cursor") == 0,
          "cursor -> Cursor");
    CHECK(strcmp(wubu_vblank_src_for("zzz"), "CRTC") == 0,
          "zzz -> CRTC fallback");

    CHECK(strcmp(wubu_vblank_mode_for("event"), "event") == 0,
          "event -> event");
    CHECK(strcmp(wubu_vblank_mode_for("flip"), "page-flip") == 0,
          "flip -> page-flip");
    CHECK(strcmp(wubu_vblank_mode_for("counter"), "counter") == 0,
          "counter -> counter");
    CHECK(strcmp(wubu_vblank_mode_for("time"), "time") == 0,
          "time -> time");
    CHECK(strcmp(wubu_vblank_mode_for("disable"), "disabled") == 0,
          "disable -> disabled");
    CHECK(strcmp(wubu_vblank_mode_for("enable"), "enabled") == 0,
          "enable -> enabled");
    CHECK(strcmp(wubu_vblank_mode_for("zzz"), "event") == 0,
          "zzz -> event fallback");

    char s[256];
    wubu_vblank_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "vblank summary generated");

    printf("\n=== VBLANK TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
