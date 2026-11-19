/*
 * wm_shadow_test.c -- behaviour gate for the cached elevation shadow.
 * Asserts real pixels (soft falloff beside the frame), the cache oracle
 * (no re-blur on unchanged geometry), theme gating and high-contrast gating.
 */
#include "wm_shadow.h"
#include "../gfx/gui_gfx.h"
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
    /* Moved window: rebuild exactly once. */
    win.x += 10;
    wm_shadow_render(&win);
    CHECK(wm_shadow_blur_count() == b1 + 1, "moved geometry re-blurs once");

    /* Classic theme: no shadow at all. */
    wubu_theme_set(THEME_WIN98_CLASSIC);
    vbe_clear(0xFF808080u);
    wm_shadow_render(&win);
    CHECK(vbe_get_pixel(win.x + win.w + 4, win.y + win.h / 2) == 0xFF808080u,
          "classic theme draws no shadow");
    CHECK(wm_shadow_blur_count() == b1 + 1, "classic theme does not blur");

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