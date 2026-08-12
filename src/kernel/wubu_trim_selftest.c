/*
 * wubu_trim_selftest.c -- verifies kernel-owned TRIM/alt-mode routing.
 */
#include "wubu_trim.h"
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
    printf("=== wubu_trim_selftest ===\n\n");

    wubu_hw_detect();
    wubu_trim_probe();

    printf("  trim=%d fstrim=%d discard=%d altmode=%d tb=%d\n",
           wubu_trim_supported(), wubu_trim_fstrim(), wubu_trim_discard(),
           wubu_trim_altmode(), wubu_trim_thunderbolt());

    /* TRIM mode routing. */
    CHECK(strcmp(wubu_trim_mode_for("ext4"), "ext4-discard") == 0,
          "ext4 -> ext4-discard");
    CHECK(strcmp(wubu_trim_mode_for("btrfs"), "btrfs-discard") == 0,
          "btrfs -> btrfs-discard");
    CHECK(strcmp(wubu_trim_mode_for("xfs"), "xfs-discard") == 0,
          "xfs -> xfs-discard");
    CHECK(strcmp(wubu_trim_mode_for("nvme"), "nvme-deallocate") == 0,
          "nvme -> nvme-deallocate");
    CHECK(strcmp(wubu_trim_mode_for("ata"), "ata-trim") == 0,
          "ata -> ata-trim");
    CHECK(strcmp(wubu_trim_mode_for("unknown"), "trim") == 0,
          "unknown -> trim fallback");

    /* Alt-mode routing. */
    CHECK(strcmp(wubu_trim_altmode_for("dp"), "displayport-alt") == 0,
          "dp -> displayport-alt");
    CHECK(strcmp(wubu_trim_altmode_for("tb"), "thunderbolt-alt") == 0,
          "tb -> thunderbolt-alt");
    CHECK(strcmp(wubu_trim_altmode_for("usb4"), "usb4") == 0,
          "usb4 -> usb4");
    CHECK(strcmp(wubu_trim_altmode_for("unknown"), "typec-altmode") == 0,
          "unknown -> typec-altmode fallback");

    char s[256];
    wubu_trim_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "trim summary generated");

    printf("\n=== TRIM TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
