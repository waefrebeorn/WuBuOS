/*
 * gfx_blend.c -- alpha compositing (one concern: the blend).
 *
 * Design ref: rizonesoft/impossible-os@be8c286 src/kernel/gfx/gfx_blend.c
 * (GPL-3.0-only). Ground-up WuBuOS C11 implementation; no upstream code
 * copied. Divergence: straight alpha sources (upstream's hot path is
 * premultiplied); the screen side goes through vbe_* so the VBE clip and
 * the render/present boundary are honoured.
 */
#include "gfx_internal.h"
#include "../../kernel/vbe.h"

uint32_t gui_blend_pixel_argb(uint32_t dst, uint32_t src)
{
    uint32_t sa = GUI_A(src);
    if (sa == 0) return dst;
    if (sa == 0xFF) return src;
    uint32_t inv = 255u - sa;
    uint32_t r = gui_div255(GUI_R(src) * sa + GUI_R(dst) * inv);
    uint32_t g = gui_div255(GUI_G(src) * sa + GUI_G(dst) * inv);
    uint32_t b = gui_div255(GUI_B(src) * sa + GUI_B(dst) * inv);
    uint32_t a = sa + gui_div255(GUI_A(dst) * inv);
    return GUI_ARGB(a, r, g, b);
}

/* Write one composited pixel to the screen through the VBE owner. */
static void put(int x, int y, uint32_t src, uint32_t global_a)
{
    uint32_t sa = GUI_A(src);
    if (global_a != 0xFF) sa = gui_div255(sa * global_a);
    if (sa == 0) return;
    uint32_t dst = vbe_get_pixel(x, y);
    if (sa == 0xFF) { vbe_set_pixel(x, y, src); return; }
    uint32_t inv = 255u - sa;
    uint32_t r = gui_div255(GUI_R(src) * sa + GUI_R(dst) * inv);
    uint32_t g = gui_div255(GUI_G(src) * sa + GUI_G(dst) * inv);
    uint32_t b = gui_div255(GUI_B(src) * sa + GUI_B(dst) * inv);
    /* The screen is XRGB: alpha is not presented, keep it opaque. */
    vbe_set_pixel(x, y, GUI_ARGB(0xFF, r, g, b));
}

void gui_blit_to_vbe(const GuiSurface *s, int dx, int dy)
{
    gui_blit_to_vbe_alpha(s, dx, dy, 0xFF);
}

void gui_blit_to_vbe_alpha(const GuiSurface *s, int dx, int dy, uint8_t alpha)
{
    if (!s || alpha == 0) return;
    gui_blit_rect_to_vbe_alpha(s, 0, 0, s->w, s->h, dx, dy, alpha);
}

/* Blit a SUB-RECT of a surface, so a caller can skip pixels it knows are covered. The window
 * shadow only shows in the ring OUTSIDE the window; compositing the whole padded rect every
 * frame was the single most expensive thing in the renderer (40 ms/frame, 23 fps). */
void gui_blit_rect_to_vbe_alpha(const GuiSurface *s, int sx, int sy, int w, int h,
                               int dx, int dy, uint8_t alpha)
{
    if (!s || alpha == 0 || w <= 0 || h <= 0) return;
    if (sx < 0) { w += sx; dx -= sx; sx = 0; }
    if (sy < 0) { h += sy; dy -= sy; sy = 0; }
    if (sx + w > s->w) w = s->w - sx;
    if (sy + h > s->h) h = s->h - sy;
    for (int row = 0; row < h; row++) {
        int y = dy + row;
        const uint32_t *sp = s->px + (size_t)(sy + row) * s->w + (size_t)sx;
        for (int col = 0; col < w; col++)
            put(dx + col, y, sp[col], alpha);
    }
}
/*
 * A row-pointer alpha blit for large, static overlays (the elevation shadow).
 *
 * The generic path above calls vbe_get_pixel()/vbe_set_pixel() for EVERY pixel, and each of those
 * recomputes the row from the base pointer and re-checks the clip. A shadow is ~700k pixels at 1200x800,
 * measured at 8.7 ms of a 10.9 ms frame -- 80% of the frame, and the reason the desktop felt laggy under the
 * pointer. This resolves the clip and the destination row ONCE per row and blends inline. Same maths, same
 * clip semantics, no per-pixel function call.
 */
static void blit_fast_rect(const GuiSurface *s, int sx, int sy, int w, int h,
                           int dx, int dy, uint8_t alpha)
{
    if (!s || alpha == 0 || w <= 0 || h <= 0) return;
    if (sx < 0) { w += sx; dx -= sx; sx = 0; }
    if (sy < 0) { h += sy; dy -= sy; sy = 0; }
    if (sx + w > s->w) w = s->w - sx;
    if (sy + h > s->h) h = s->h - sy;
    if (w <= 0 || h <= 0) return;
    int stride = 0, surf_h = 0;
    uint32_t *dst = vbe_draw_surface(&stride, &surf_h);
    if (!dst || stride <= 0) { gui_blit_to_vbe_alpha(s, dx, dy, alpha); return; }

    VBEState *st = vbe_state();
    int cx0 = 0, cy0 = 0, cx1 = st ? st->width : stride, cy1 = surf_h;
    if (st && st->clip_w > 0 && st->clip_h > 0) {
        cx0 = st->clip_x; cy0 = st->clip_y;
        cx1 = st->clip_x + st->clip_w; cy1 = st->clip_y + st->clip_h;
    }
    if (cx0 < 0) cx0 = 0;
    if (cy0 < 0) cy0 = 0;
    if (cx1 > stride) cx1 = stride;
    if (cy1 > surf_h) cy1 = surf_h;

    for (int row = 0; row < h; row++) {
        int y = dy + row;
        if (y < cy0 || y >= cy1) continue;
        const uint32_t *sp = s->px + (size_t)(sy + row) * (size_t)s->w + (size_t)sx;
        uint32_t *dp = dst + (size_t)y * (size_t)stride;
        for (int col = 0; col < w; col++) {
            int x = dx + col;
            if (x < cx0 || x >= cx1) continue;
            uint32_t src = sp[col];
            uint32_t sa = GUI_A(src);
            if (!sa) continue;
            if (alpha != 0xFF) sa = gui_div255(sa * alpha);
            if (!sa) continue;
            uint32_t d = dp[x];
            uint32_t inv = 255u - sa;
            uint32_t r = gui_div255(GUI_R(src) * sa + GUI_R(d) * inv);
            uint32_t g = gui_div255(GUI_G(src) * sa + GUI_G(d) * inv);
            uint32_t b = gui_div255(GUI_B(src) * sa + GUI_B(d) * inv);
            dp[x] = GUI_ARGB(0xFF, r, g, b);
        }
    }
}

/* A source RECTANGLE, for compositing the visible parts of a large overlay (a shadow's ring: the middle is
 * covered by the window that casts it). */
void gui_blit_rect_alpha_fast(const GuiSurface *s, int sx, int sy, int w, int h,
                              int dx, int dy, uint8_t alpha)
{
    blit_fast_rect(s, sx, sy, w, h, dx, dy, alpha);
}

void gui_blit_alpha_fast(const GuiSurface *s, int dx, int dy, uint8_t alpha)
{
    if (!s) return;
    blit_fast_rect(s, 0, 0, s->w, s->h, dx, dy, alpha);
}
