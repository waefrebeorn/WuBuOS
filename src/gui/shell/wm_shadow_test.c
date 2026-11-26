/*
 * wm_shadow_test.c -- behaviour gate for the cached elevation shadow.
 * Asserts real pixels (soft falloff beside the frame), the cache oracle
 * (no re-blur on unchanged geometry), theme gating and high-contrast gating.
 */
#include "wm_shadow.h"
#include "../gfx/gfx.h"
#include "../wubu_theme.h"
#include "../../kernel/vbe.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

static unsigned red(uint32_t c) { return (c >> 16) & 0xFF; }

int main(void)
{
    vbe_init(256, 256);
    wubu_theme_init();
    wubu_theme_set(THEME_DARK);

    DosGuiWindow win;
    memset(&win, 0, sizeof win);
    win.alive = 1; win.id = 0;
    win.x = 60; win.y = 50; win.w = 100; win.h = 80;

    vbe_clear(0xFF808080u);
    wm_shadow_render(&win);
    unsigned b1 = wm_shadow_blur_count();
    CHECK(b1 == 1, "first render blurs the mask once");

    /* Beside the right frame edge, mid-height: darkened but not black. */
    uint32_t near = vbe_get_pixel(win.x + win.w + 4, win.y + win.h / 2);
    uint32_t far  = vbe_get_pixel(win.x + win.w + 24, win.y + win.h / 2);
    CHECK(red(near) < 0x80 && red(near) > 0, "shadow darkens the backdrop");
    CHECK(red(near) < red(far), "falloff: closer to the frame is darker");
    CHECK(vbe_get_pixel(2, 2) == 0xFF808080u, "far field untouched");
    /* Above the top edge (light from above: shadow still wraps the top). */
    CHECK(red(vbe_get_pixel(win.x + win.w / 2, win.y - 4)) < 0x80,
          "shadow wraps the top edge");

    /* Cache: same geometry again must not re-blur. */
    wm_shadow_render(&win);
    CHECK(wm_shadow_blur_count() == b1, "unchanged geometry reuses the cache");
    /* Moved window: the shadow SHAPE is unchanged, so the cache must be REUSED and painted at
     * the new position. Re-blurring on every move is what held the renderer at 40 ms/frame with
     * two windows open; the cache is keyed on size because the bitmap is position-independent.
     * What must be true is the PIXELS, not the blur count. */
    win.x += 10;
    wm_shadow_render(&win);
    CHECK(wm_shadow_blur_count() == b1, "moving a window reuses the size cache");
    uint32_t moved = vbe_get_pixel(win.x + win.w + 4, win.y + win.h / 2);
    CHECK(red(moved) < 0x80 && red(moved) > 0, "and the shadow is painted at the NEW position");

    /* Classic theme: no shadow at all. The window is FIRST resized, because a size that is
     * already in the cache cannot prove anything -- the old assertion passed trivially off the
     * cache whether classic blurred or not. An uncached size makes a classic blur visible. */
    wubu_theme_set(THEME_WIN98_CLASSIC);
    win.w += 32; win.h += 32;
    vbe_clear(0xFF808080u);
    wm_shadow_render(&win);
    CHECK(vbe_get_pixel(win.x + win.w + 4, win.y + win.h / 2) == 0xFF808080u,
          "classic theme draws no shadow");
    /* NO blur is the contract: wm_shadow_render returns for a non-dark theme BEFORE touching the
     * cache, so a classic desktop must pay for nothing. This used to assert b1 + 1 -- i.e. it
     * demanded exactly the wasted blur it was named after not happening, so it failed against
     * correct code. */
    CHECK(wm_shadow_blur_count() == b1, "classic theme does not blur");

    /* High contrast: shadow suppressed, then restored when toggled off. */
    wubu_theme_set(THEME_DARK);
    wubu_theme_set_high_contrast(1);
    vbe_clear(0xFF808080u);
    wm_shadow_render(&win);
    CHECK(vbe_get_pixel(win.x + win.w + 4, win.y + win.h / 2) == 0xFF808080u,
          "high contrast suppresses the shadow");
    wubu_theme_set_high_contrast(0);
    wm_shadow_render(&win);
    CHECK(red(vbe_get_pixel(win.x + win.w + 4, win.y + win.h / 2)) < 0x80,
          "shadow returns when high contrast is off");

    /* Dead window: no-op. */
    win.alive = 0;
    vbe_clear(0xFF808080u);
    wm_shadow_render(&win);
    CHECK(vbe_get_pixel(win.x + win.w + 4, win.y + win.h / 2) == 0xFF808080u,
          "dead window draws nothing");

    vbe_shutdown();
    if (failures) { printf("wm_shadow: %d FAILURES\n", failures); return 1; }
    printf("wm_shadow: ALL PASS\n");
    return 0;
}