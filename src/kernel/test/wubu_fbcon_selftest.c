/*
 * wubu_fbcon_selftest.c -- verifies framebuffer console routing.
 */
#include "wubu_fbcon.h"
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
    printf("=== wubu_fbcon_selftest ===\n\n");
    wubu_hw_detect();
    wubu_fbcon_probe();
    printf("  fb=%d drm=%d rotate=%d virtual=%d mode=%d\n",
           wubu_fbcon_present(), wubu_fbcon_drm(), wubu_fbcon_rotate(),
           wubu_fbcon_virtual(), wubu_fbcon_mode());

    CHECK(strcmp(wubu_fbcon_rotate_for("normal"), "normal") == 0,
          "normal -> normal");
    CHECK(strcmp(wubu_fbcon_rotate_for("left"), "left") == 0,
          "left -> left");
    CHECK(strcmp(wubu_fbcon_rotate_for("upside"), "upside-down") == 0,
          "upside -> upside-down");
    CHECK(strcmp(wubu_fbcon_rotate_for("right"), "right") == 0,
          "right -> right");
    CHECK(strcmp(wubu_fbcon_rotate_for("zzz"), "normal") == 0,
          "zzz -> normal fallback");

    CHECK(strcmp(wubu_fbcon_mode_for("800x600"), "800x600") == 0,
          "800x600 -> 800x600");
    CHECK(strcmp(wubu_fbcon_mode_for("1024x768"), "1024x768") == 0,
          "1024x768 -> 1024x768");
    CHECK(strcmp(wubu_fbcon_mode_for("1920x1080"), "1920x1080") == 0,
          "1920x1080 -> 1920x1080");
    CHECK(strcmp(wubu_fbcon_mode_for("zzz"), "1920x1080") == 0,
          "zzz -> 1920x1080 fallback");

    char s[256];
    wubu_fbcon_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "fbcon summary generated");

    printf("\n=== FBCON TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
