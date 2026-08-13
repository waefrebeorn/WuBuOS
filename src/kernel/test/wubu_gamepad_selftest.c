/*
 * wubu_gamepad_selftest.c -- verifies kernel-owned gamepad/DSC routing.
 */
#include "wubu_gamepad.h"
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
    printf("=== wubu_gamepad_selftest ===\n\n");

    wubu_hw_detect();
    wubu_gamepad_probe();

    printf("  pad=%d wheel=%d arcade=%d dsc=%d\n",
           wubu_gamepad_present(), wubu_gamepad_wheel(),
           wubu_gamepad_arcade(), wubu_gamepad_dsc());

    /* Controller routing is always consistent. */
    CHECK(strcmp(wubu_gamepad_controller_for("xbox"), "xpad") == 0,
          "xbox -> xpad");
    CHECK(strcmp(wubu_gamepad_controller_for("playstation"), "hid-playstation") == 0,
          "playstation -> hid-playstation");
    CHECK(strcmp(wubu_gamepad_controller_for("nintendo"), "hid-nintendo") == 0,
          "nintendo -> hid-nintendo");
    CHECK(strcmp(wubu_gamepad_controller_for("steam"), "hid-steam") == 0,
          "steam -> hid-steam");
    CHECK(strcmp(wubu_gamepad_controller_for("g29"), "g29_ff") == 0,
          "g29 -> g29_ff");
    CHECK(strcmp(wubu_gamepad_controller_for("thrustmaster"), "hid-tmff") == 0,
          "thrustmaster -> hid-tmff");
    CHECK(strcmp(wubu_gamepad_controller_for("unknown"), "uinput") == 0,
          "unknown -> uinput fallback");

    /* DSC routing. */
    CHECK(strcmp(wubu_gamepad_dsc_for("i915"), "i915-dsc") == 0,
          "i915 -> i915-dsc");
    CHECK(strcmp(wubu_gamepad_dsc_for("amdgpu"), "amdgpu-dsc") == 0,
          "amdgpu -> amdgpu-dsc");
    CHECK(strcmp(wubu_gamepad_dsc_for("nouveau"), "nouveau-dsc") == 0,
          "nouveau -> nouveau-dsc");
    CHECK(strcmp(wubu_gamepad_dsc_for("unknown"), "drm-dsc") == 0,
          "unknown -> drm-dsc fallback");

    char s[256];
    wubu_gamepad_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "gamepad summary generated");

    printf("\n=== GAMEPAD TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
