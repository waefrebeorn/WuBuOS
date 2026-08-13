/*
 * wubu_display_selftest.c -- verifies kernel-owned display driver routing.
 */
#include "wubu_display.h"
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
    printf("=== wubu_display_selftest ===\n\n");

    wubu_hw_detect();
    wubu_display_probe();

    printf("  driver=%s chip=%s card=%d render=%d xe=%d\n",
           wubu_display_driver() ? wubu_display_driver() : "(none)",
           wubu_display_chip_name() ? wubu_display_chip_name() : "(none)",
           wubu_display_card_index(),
           wubu_display_has_render_node(),
           wubu_display_xe_preferred());

    CHECK(wubu_hw_is_wsl() || wubu_display_driver() != NULL,
          "display driver resolved (or WSL2-host-owned)");
    CHECK(wubu_hw_is_wsl() || wubu_display_chip_name() != NULL,
          "display chip name resolved (or WSL2-host-owned)");

    char s[256];
    wubu_display_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "display summary generated");

    /* KMS paths non-null. */
    CHECK(wubu_display_card_path() != NULL, "KMS card path generated");
    CHECK(wubu_display_render_path() != NULL, "render node path generated");

    /* Feature flags (present on real hardware, 0 on WSL2-host). */
    printf("  atomic=%d edid=%d mst=%d dsc=%d hdcp=%d vrr=%d\n",
           wubu_display_atomic_modeset(), wubu_display_has_edid(),
           wubu_display_has_mst(), wubu_display_has_dsc(),
           wubu_display_has_hdcp(), wubu_display_has_vrr());

    printf("\n=== DISPLAY TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
