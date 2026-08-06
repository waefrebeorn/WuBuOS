/*
 * cmd_test_stub.c -- Minimal stubs for the CMD terminal test. The test only
 * exercises pty spawn/read/history (not rendering), so the vbe/theme draw
 * primitives are no-ops. This keeps the link small and focused on the engine.
 */

#include <stdint.h>
#include <string.h>

/* From kernel/vbe.h (color type) — mirror the minimal fields cmd.c touches. */
typedef struct { uint32_t border_light, border_face, border_dark, border_darkest, btn_face; } WubuThemeColors;

const WubuThemeColors *wubu_theme_colors(void) {
    static WubuThemeColors c;
    return &c;
}

void vbe_fill_rect(int x, int y, int w, int h, uint32_t color) { (void)x;(void)y;(void)w;(void)h;(void)color; }
void vbe_draw_text(int x, int y, const char *s, uint32_t color, int scale) { (void)x;(void)y;(void)s;(void)color;(void)scale; }
void vbe_vline(int x, int y0, int y1, uint32_t color) { (void)x;(void)y0;(void)y1;(void)color; }
void vbe_3d_sunken_colors(int x, int y, int w, int h,
                          uint32_t l, uint32_t f, uint32_t d, uint32_t dd) {
    (void)x;(void)y;(void)w;(void)h;(void)l;(void)f;(void)d;(void)dd;
}
int vbe_text_width(const char *s, int scale) { return s ? (int)strlen(s) * 8 * scale : 0; }

/* Chrome draw stub: cmd.c calls dosgui_chrome_draw_window for its frame;
 * the test is headless, so return a content rect inside the window and
 * skip the real (vbe/theme-heavy) chrome path. Mirrors the real signature
 * from dosgui_wm.h. */
typedef struct DosGuiWindow DosGuiWindow;
typedef struct { int x, y, w, h; } ChromeContentRect;
ChromeContentRect dosgui_chrome_draw_window(DosGuiWindow *win,
                                            uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    ChromeContentRect empty = {0, 0, 0, 0};
    if (!win) return empty;
    /* w/h/x/y accessors not needed here — the real chrome returns
     * content inset by border+titlebar; the test only needs non-zero. */
    empty.w = 80; empty.h = 24;
    return empty;
}
