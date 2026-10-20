/*
 * wubu_caption_test.c -- checks for the Luna caption gradient and focus flash.
 */
#include <stdio.h>
#include "wubu_caption.h"

static int checks, failures;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) { failures++; printf("  FAIL %s\n", what); }
}

int main(void) {
    WubuThemeColors c = {0};
    c.win_title_active   = 0x0A246A;   /* Luna navy */
    c.win_title_inactive = 0x808080;

    printf("== caption gradient\n");

    /* Inactive is flat: every row the same, and it is the inactive colour. */
    int flat = 1;
    for (int y = 0; y < 24; y++)
        if (wubu_caption_px(&c, y, 24, false) != c.win_title_inactive) flat = 0;
    check(flat, "inactive caption is flat and equals win_title_inactive");

    /* Active gradient: top is lighter than bottom. */
    uint32_t top = wubu_caption_px(&c, 0, 24, true);
    uint32_t bot = wubu_caption_px(&c, 23, 24, true);
    check(wubu_caption_luma(top) > wubu_caption_luma(bot),
          "top of active caption is lighter than bottom");
    check(top != bot, "active caption is a real gradient, not a flat fill");

    /* Monotonic: luminance never rises as we go down. */
    int mono = 1, prev = wubu_caption_luma(top) + 1;
    for (int y = 0; y < 24; y++) {
        int l = wubu_caption_luma(wubu_caption_px(&c, y, 24, true));
        if (l > prev) mono = 0;
        prev = l;
    }
    check(mono, "gradient luminance is monotonic top to bottom");

    /* Degenerate heights and out-of-range rows must not divide by zero or wrap. */
    check(wubu_caption_px(&c, 0, 1, true) == c.win_title_active,
          "height 1 returns the base colour");
    check(wubu_caption_px(&c, -7, 24, true) == wubu_caption_px(&c, 0, 24, true),
          "negative y clamps to the top row");
    check(wubu_caption_px(&c, 999, 24, true) == wubu_caption_px(&c, 23, 24, true),
          "oversized y clamps to the bottom row");
    check(wubu_caption_px(NULL, 0, 24, true) == 0, "NULL colours return 0");

    printf("== focus flash\n");
    uint32_t accent = 0xFFB060;
    check(wubu_caption_focus_glow(c.win_title_active, accent, 0.0) == c.win_title_active,
          "t=0 returns the base colour exactly (no animation)");
    check(wubu_caption_focus_glow(c.win_title_active, accent, 1.0) == c.win_title_active,
          "t=1 returns the base colour exactly (animation finished)");
    check(wubu_caption_focus_glow(c.win_title_active, accent, -1.0) == c.win_title_active,
          "t<0 returns the base colour");
    uint32_t mid = wubu_caption_focus_glow(c.win_title_active, accent, 0.5);
    check(mid != c.win_title_active, "mid-animation colour differs from base");
    check(wubu_caption_luma(mid) > wubu_caption_luma(c.win_title_active),
          "focus flash brightens the caption");

    printf("== luma\n");
    check(wubu_caption_luma(0xFFFFFF) == 255, "white luma is 255");
    check(wubu_caption_luma(0x000000) == 0, "black luma is 0");

    printf("=== wubu_caption: %d checks, %d failures ===\n", checks, failures);
    return failures ? 1 : 0;
}