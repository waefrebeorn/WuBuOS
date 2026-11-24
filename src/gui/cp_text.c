/*
 * cp_text.c -- stride-aware text and fill for Control Panel applets.
 *
 * See cp_text.h for why this exists. It draws with the kernel's own 8x8 font
 * (vbe_font_8x8 for ASCII 32..95, vbe_font_lower_8x8 for a..z), so an applet
 * looks like the rest of the shell instead of inventing its own glyphs.
 */

#include "cp_text.h"
#include "../kernel/vbe.h"

#include <string.h>

void cp_fill(uint32_t *fb, int fb_w, int fb_h,
             int x, int y, int w, int h, uint32_t colour)
{
    if (!fb || fb_w <= 0 || fb_h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb_w) w = fb_w - x;
    if (y + h > fb_h) h = fb_h - y;
    if (w <= 0 || h <= 0) return;
    for (int row = y; row < y + h; row++) {
        uint32_t *p = fb + (size_t)row * (size_t)fb_w + x;
        for (int i = 0; i < w; i++) p[i] = colour;
    }
}

/* One glyph row bit -> pixel. bit 7 is the leftmost pixel. */
static void cp_glyph(uint32_t *fb, int fb_w, int fb_h, int x, int y,
                     const uint8_t *g, uint32_t colour, int scale)
{
    for (int r = 0; r < 8; r++) {
        uint8_t bits = g[r];
        if (!bits) continue;
        for (int c = 0; c < 8; c++) {
            if (!(bits & (0x80u >> c))) continue;
            if (scale == 1) {
                int px = x + c, py = y + r;
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                    fb[(size_t)py * (size_t)fb_w + px] = colour;
            } else {
                cp_fill(fb, fb_w, fb_h, x + c * scale, y + r * scale,
                        scale, scale, colour);
            }
        }
    }
}

int cp_draw_text(uint32_t *fb, int fb_w, int fb_h,
                 int x, int y, const char *s, uint32_t colour, int scale)
{
    if (!s) return 0;
    if (scale < 1) scale = 1;
    int cx = x;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == ' ') { cx += 8 * scale; continue; }
        if (*p == '\n') { y += 10 * scale; cx = x; continue; }
        const uint8_t *g = NULL;
        if (*p >= 32 && *p <= 95)       g = vbe_font_8x8[*p - 32];
        else if (*p > 95 && *p <= 127)  g = vbe_font_lower_8x8[*p - 'a'];
        if (g) cp_glyph(fb, fb_w, fb_h, cx, y, g, colour, scale);
        cx += 8 * scale;
    }
    return cx - x;
}

int cp_text_width(const char *s, int scale)
{
    if (!s) return 0;
    if (scale < 1) scale = 1;
    return (int)strlen(s) * 8 * scale;
}

void cp_text_wrapped(uint32_t *fb, int fb_w, int fb_h, int x, int y, int max_w,
                     const char *s, uint32_t colour, int scale)
{
    if (!s) return;
    if (scale < 1) scale = 1;
    int per_line = max_w / (8 * scale);
    if (per_line < 1) return;
    const char *p = s;
    while (*p) {
        while (*p == ' ') p++;
        const char *line = p;
        int n = 0, last_space = -1;
        while (*p && *p != '\n') {
            if (*p == ' ') last_space = n;
            if (n == per_line) break;
            p++; n++;
        }
        int cut = n;
        if (*p && *p != '\n' && last_space > 0) {
            cut = last_space;              /* break at the last space */
            p = line + cut;
        }
        char buf[128];
        if (cut >= (int)sizeof buf) cut = (int)sizeof buf - 1;
        memcpy(buf, line, (size_t)cut);
        buf[cut] = 0;
        cp_draw_text(fb, fb_w, fb_h, x, y, buf, colour, scale);
        y += 10 * scale;
        if (*p == '\n') p++;
    }
}

void cp_row(uint32_t *fb, int fb_w, int fb_h, int x, int y, int max_w,
            const char *label, const char *value,
            uint32_t label_colour, uint32_t value_colour)
{
    int lw = cp_draw_text(fb, fb_w, fb_h, x, y, label, label_colour, 1);
    if (!value) return;
    int vx = x + lw + 4;
    if (vx >= x + max_w) return;
    cp_draw_text(fb, fb_w, fb_h, vx, y, value, value_colour, 1);
}
