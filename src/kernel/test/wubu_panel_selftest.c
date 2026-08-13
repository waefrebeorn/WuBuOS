/*
 * wubu_panel_selftest.c -- verifies GPU panel routing.
 */
#include "wubu_panel.h"
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
    printf("=== wubu_panel_selftest ===\n\n");
    wubu_hw_detect();
    wubu_panel_probe();
    printf("  pan=%d lvds=%d edp=%d hdmi=%d dp=%d vga=%d\n",
           wubu_panel_present(), wubu_panel_lvds(),
           wubu_panel_edp(), wubu_panel_hdmi(),
           wubu_panel_dp(), wubu_panel_vga());

    CHECK(strcmp(wubu_panel_type_for("lvds"), "LVDS") == 0,
          "lvds -> LVDS");
    CHECK(strcmp(wubu_panel_type_for("edp"), "eDP") == 0,
          "edp -> eDP");
    CHECK(strcmp(wubu_panel_type_for("hdmi"), "HDMI") == 0,
          "hdmi -> HDMI");
    CHECK(strcmp(wubu_panel_type_for("dp"), "DisplayPort") == 0,
          "dp -> DisplayPort");
    CHECK(strcmp(wubu_panel_type_for("vga"), "VGA") == 0,
          "vga -> VGA");
    CHECK(strcmp(wubu_panel_type_for("dvi"), "DVI") == 0,
          "dvi -> DVI");
    CHECK(strcmp(wubu_panel_type_for("composite"), "Composite") == 0,
          "composite -> Composite");
    CHECK(strcmp(wubu_panel_type_for("svideo"), "S-Video") == 0,
          "svideo -> S-Video");
    CHECK(strcmp(wubu_panel_type_for("zzz"), "HDMI") == 0,
          "zzz -> HDMI fallback");

    CHECK(strcmp(wubu_panel_status_for("connected"), "connected") == 0,
          "connected -> connected");
    CHECK(strcmp(wubu_panel_status_for("disconnected"), "disconnected") == 0,
          "disconnected -> disconnected");
    CHECK(strcmp(wubu_panel_status_for("unknown"), "unknown") == 0,
          "unknown -> unknown");
    CHECK(strcmp(wubu_panel_status_for("zzz"), "unknown") == 0,
          "zzz -> unknown fallback");

    char s[256];
    wubu_panel_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "panel summary generated");

    printf("\n=== PANEL TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
