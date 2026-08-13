/*
 * wubu_backlight_selftest.c -- verifies kernel-owned backlight/WoL routing.
 */
#include "wubu_backlight.h"
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
    printf("=== wubu_backlight_selftest ===\n\n");

    wubu_hw_detect();
    wubu_backlight_probe();

    printf("  bl=%d acpi=%d native=%d wol=%d magic=%d\n",
           wubu_backlight_present(), wubu_backlight_acpi(),
           wubu_backlight_native(), wubu_backlight_wol(),
           wubu_backlight_wol_magic());

    /* Backlight driver routing. */
    CHECK(strcmp(wubu_backlight_driver_for("acpi"), "acpi-video") == 0,
          "acpi -> acpi-video");
    CHECK(strcmp(wubu_backlight_driver_for("intel"), "intel-backlight") == 0,
          "intel -> intel-backlight");
    CHECK(strcmp(wubu_backlight_driver_for("amdgpu"), "amdgpu-bl") == 0,
          "amdgpu -> amdgpu-bl");
    CHECK(strcmp(wubu_backlight_driver_for("pwm"), "pwm-backlight") == 0,
          "pwm -> pwm-backlight");
    CHECK(strcmp(wubu_backlight_driver_for("nouveau"), "nouveau-backlight") == 0,
          "nouveau -> nouveau-backlight");
    CHECK(strcmp(wubu_backlight_driver_for("unknown"), "drm-backlight") == 0,
          "unknown -> drm-backlight fallback");

    /* WoL routing. */
    CHECK(strcmp(wubu_backlight_wol_for("magic"), "magic-packet") == 0,
          "magic -> magic-packet");
    CHECK(strcmp(wubu_backlight_wol_for("unicast"), "unicast") == 0,
          "unicast -> unicast");
    CHECK(strcmp(wubu_backlight_wol_for("broadcast"), "broadcast") == 0,
          "broadcast -> broadcast");
    CHECK(strcmp(wubu_backlight_wol_for("arp"), "arp") == 0,
          "arp -> arp");
    CHECK(strcmp(wubu_backlight_wol_for("zzz"), "wol") == 0,
          "unknown flag -> wol fallback");

    char s[256];
    wubu_backlight_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "backlight summary generated");

    printf("\n=== BACKLIGHT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
