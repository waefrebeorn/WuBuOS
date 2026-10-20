/*
 * wubu_caption.c -- Luna-style caption gradient + focus flash (C11, integer).
 */
#include "wubu_caption.h"

static int ch(int rgb, int shift) { return (rgb >> shift) & 0xFF; }

static uint32_t rgb_of(int r, int g, int b) {
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* lerp toward `to` by num/den, integer only. */
static int lerp_ch(int from, int to, int num, int den) {
    return from + (to - from) * num / den;
}

uint32_t wubu_caption_px(const WubuThemeColors *c, int y, int h, bool active) {
    if (!c) return 0;
    if (!active) return c->win_title_inactive;
    if (h <= 1) return c->win_title_active;
    if (y < 0) y = 0;
    if (y >= h) y = h - 1;

    /* Luna: pale at the top edge, full colour at the bottom edge. */
    const uint32_t base = c->win_title_active;
    const int t = y * 255 / (h - 1);          /* 0 top .. 255 bottom */
    const int mix = 255 - t;                  /* how much white at top */

    int r = lerp_ch(ch(base, 16), 255, mix * 45 / 255, 100);
    int g = lerp_ch(ch(base, 8),  255, mix * 45 / 255, 100);
    int b = lerp_ch(ch(base, 0),  255, mix * 45 / 255, 100);

    /* And a slight darkening toward the bottom edge, which is what gives the
     * caption its glassy two-tone look rather than a plain white-to-colour fade. */
    r = lerp_ch(r, 0, t * 18 / 255, 100);
    g = lerp_ch(g, 0, t * 18 / 255, 100);
    b = lerp_ch(b, 0, t * 18 / 255, 100);
    return rgb_of(r, g, b);
}

uint32_t wubu_caption_focus_glow(uint32_t base, uint32_t accent, double t) {
    if (t <= 0.0 || t >= 1.0) return base;    /* stopped == no animation */
    /* Parabola: 0 at the ends, 1 in the middle; no libm in the draw path. */
    const double env = 4.0 * t * (1.0 - t);
    const int k = (int)(env * 60.0);          /* up to 60% toward the accent */
    int r = lerp_ch(ch(base, 16), ch(accent, 16), k, 100);
    int g = lerp_ch(ch(base, 8),  ch(accent, 8),  k, 100);
    int b = lerp_ch(ch(base, 0),  ch(accent, 0),  k, 100);
    return rgb_of(r, g, b);
}

int wubu_caption_luma(uint32_t rgb) {
    /* Rec.601 integer weights. */
    const int r = ch(rgb, 16), g = ch(rgb, 8), b = ch(rgb, 0);
    return (r * 299 + g * 587 + b * 114) / 1000;
}