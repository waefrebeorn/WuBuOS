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
