/*
 * control_panel_text.c -- stride-aware text and fill for Control Panel applets.
 *
 * See control_panel_text.h for why this exists. It draws with the kernel's own 8x8 font
 * (vbe_font_8x8 for ASCII 32..95, vbe_font_lower_8x8 for a..z), so an applet
 * looks like the rest of the shell instead of inventing its own glyphs.
 */

#include "control_panel_text.h"
#include "shell/shell_text.h"
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

/* ONE TYPE STACK: the same baked AA face (Latin-1 included) every other window uses. The panel used to draw
 * with the 8x8 bitmap tables, so it was literally a different typeface from the rest of the shell. */
int cp_draw_text(uint32_t *fb, int fb_w, int fb_h,
                 int x, int y, const char *s, uint32_t colour, int scale)
{
    if (!s) return 0;
    if (scale < 1) scale = 1;
    int cy = y;
    const char *start = s;
    while (*start) {
        const char *nl = strchr(start, '\n');
        int len = nl ? (int)(nl - start) : (int)strlen(start);
        if (len > 0) {
            char line[256];
            int n = len < (int)sizeof(line) - 1 ? len : (int)sizeof(line) - 1;
            memcpy(line, start, (size_t)n);
            line[n] = '\0';
            vbe_draw_text_on(fb, fb_w, fb_h, x, cy, line, colour, scale, 0);
        }
        if (!nl) break;
        cy += cp_text_line_h() * scale;
        start = nl + 1;
    }
    return cp_text_width(s, scale);
}

int cp_draw_text_fit(uint32_t *fb, int fb_w, int fb_h, int x, int y, int max_w,
                     const char *s, uint32_t colour, int scale)
{
    if (!s || max_w <= 0) return 0;
    char buf[256];
    vbe_ellipsize(s, max_w, buf, sizeof(buf));
    return cp_draw_text(fb, fb_w, fb_h, x, y, buf, colour, scale);
}

void cp_page_title(uint32_t *fb, int fb_w, int fb_h, int x, int y, const char *s, uint32_t colour)
{
    if (s && shell_text_draw_at(fb, fb_w, fb_h, x, y, CP_PAGE_TITLE_BAND, s,
                                SHELL_TEXT_TITLE_PX, colour) > 0)
        return;
    cp_draw_text(fb, fb_w, fb_h, x, y + CP_PAGE_TITLE_FALLBACK_DY, s, colour, 2);
}

int cp_draw_text_bold(uint32_t *fb, int fb_w, int fb_h,
                      int x, int y, const char *s, uint32_t colour, int scale)
{
    if (!s) return 0;
    if (scale < 1) scale = 1;
    vbe_draw_text_on(fb, fb_w, fb_h, x, y, s, colour, scale, 1);
    return cp_text_width(s, scale);
}

/* The line box a page must reserve for one line of the CURRENT text. Pages used bottom-anchored footers at a
 * fixed 20 px, which a taller face pushed off the pane. */
int cp_text_line_h(void) { return vbe_line_height(); }

int cp_text_width(const char *s, int scale)
{
    if (!s) return 0;
    if (scale < 1) scale = 1;
    return vbe_text_width(s, scale);   /* the face's own advances */
}

void cp_text_wrapped(uint32_t *fb, int fb_w, int fb_h, int x, int y, int max_w,
                     const char *s, uint32_t colour, int scale)
{
    if (!s || max_w <= 0) return;
    if (scale < 1) scale = 1;
    /* Wrap by MEASURED width. The previous version divided max_w by 8 * scale, i.e. it assumed a fixed
     * 8px character, which the AA face does not have and which is wrong again once text is scaled (a
     * re-rasterised 26px glyph is ~13px wide, not 16). That assumption is why wrapping was added and
     * panels still overflowed at 200% text. */
    const int line_h = cp_text_line_h() * scale;
    char buf[256];
    const char *p = s;

    while (*p) {
        if (*p == '\n') { p++; y += line_h; continue; }
        if (*p == ' ')  { p++; continue; }

        int n = 0, last_space = -1;
        while (p[n] && p[n] != '\n' && n < (int)sizeof(buf) - 1) {
            buf[n] = p[n];
            buf[n + 1] = 0;
            if (p[n] == ' ') last_space = n;
            if (n > 0 && cp_text_width(buf, scale) > max_w) break;
            n++;
        }

        if (!p[n] || p[n] == '\n' || n >= (int)sizeof(buf) - 1) {
            cp_draw_text(fb, fb_w, fb_h, x, y, buf, colour, scale);
            p += n;
        } else if (last_space > 0) {
            buf[last_space] = 0;
            cp_draw_text(fb, fb_w, fb_h, x, y, buf, colour, scale);
            p += last_space + 1;
        } else {
            cp_draw_text(fb, fb_w, fb_h, x, y, buf, colour, scale);
            p += n;   /* a single word wider than the line: hard-break it */
        }
        y += line_h;
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
    /* Fit to what is LEFT of the row: at 200% text the value alone can be wider than the panel, and
     * drawing it unclipped is what pushed content past the right edge. */
    cp_draw_text_fit(fb, fb_w, fb_h, vx, y, x + max_w - vx, value, value_colour, 1);
}
