/*
 * gfx_blur.c -- separable box blur (one concern: the blur).
 *
 * Design ref: rizonesoft/impossible-os@be8c286 src/kernel/gfx/gfx_blur.c
 * (GPL-3.0-only). Ground-up WuBuOS C11 implementation; no upstream code
 * copied. Divergence: CLAMP-to-edge sampling (the reference shrinks its
 * window at region edges, which darkens acrylic borders); running sums keep
 * each pass O(w*h) regardless of radius. Three passes approximate a
 * gaussian, the reference's acrylic trick.
 */
#include "gfx_internal.h"

#include <stdlib.h>

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* One separable box pass over the clipped region, via a scratch buffer. */
static void box_pass(GuiSurface *s, int x0, int y0, int x1, int y1,
                     int radius, uint32_t *tmp)
{
    int rw = x1 - x0, rh = y1 - y0;
    int diam = 2 * radius + 1;

    /* Horizontal: surface -> tmp (clamped columns). */
    for (int row = y0; row < y1; row++) {
        const uint32_t *src = s->px + (size_t)row * s->w;
        uint32_t sa = 0, sr = 0, sg = 0, sb = 0;
        for (int k = -radius; k <= radius; k++) {
            uint32_t p = src[clampi(x0 + k, x0, x1 - 1)];
            sa += GUI_A(p); sr += GUI_R(p); sg += GUI_G(p); sb += GUI_B(p);
        }
        for (int col = x0; col < x1; col++) {
            uint32_t *o = tmp + (size_t)(row - y0) * rw + (col - x0);
            *o = GUI_ARGB(sa / diam, sr / diam, sg / diam, sb / diam);
            uint32_t add = src[clampi(col + radius + 1, x0, x1 - 1)];
            uint32_t rem = src[clampi(col - radius, x0, x1 - 1)];
            sa += GUI_A(add) - GUI_A(rem);
            sr += GUI_R(add) - GUI_R(rem);
            sg += GUI_G(add) - GUI_G(rem);
            sb += GUI_B(add) - GUI_B(rem);
        }
    }

    /* Vertical: tmp -> surface (clamped rows). */
    for (int col = x0; col < x1; col++) {
        uint32_t sa = 0, sr = 0, sg = 0, sb = 0;
        for (int k = -radius; k <= radius; k++) {
            uint32_t p = tmp[(size_t)clampi(y0 + k, y0, y1 - 1) * rw + (col - x0)];
            sa += GUI_A(p); sr += GUI_R(p); sg += GUI_G(p); sb += GUI_B(p);
        }
        for (int row = y0; row < y1; row++) {
            uint32_t *o = s->px + (size_t)row * s->w + col;
            *o = GUI_ARGB(sa / diam, sr / diam, sg / diam, sb / diam);
            uint32_t add = tmp[(size_t)clampi(row + radius + 1, y0, y1 - 1) * rw + (col - x0)];
            uint32_t rem = tmp[(size_t)clampi(row - radius, y0, y1 - 1) * rw + (col - x0)];
            sa += GUI_A(add) - GUI_A(rem);
            sr += GUI_R(add) - GUI_R(rem);
            sg += GUI_G(add) - GUI_G(rem);
            sb += GUI_B(add) - GUI_B(rem);
        }
    }
}

void gui_blur_box(GuiSurface *s, int x, int y, int w, int h, int radius)
{
    if (!s || radius <= 0 || w <= 0 || h <= 0) return;
    int x0 = clampi(x, 0, s->w), y0 = clampi(y, 0, s->h);
    int x1 = clampi(x + w, 0, s->w), y1 = clampi(y + h, 0, s->h);
    if (x0 >= x1 || y0 >= y1) return;
    size_t n = (size_t)(x1 - x0) * (size_t)(y1 - y0);
    uint32_t *tmp = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!tmp) return;  /* degrade: unblurred, never crash */
    box_pass(s, x0, y0, x1, y1, radius, tmp);
    free(tmp);
}

void gui_blur_gauss_approx(GuiSurface *s, int x, int y, int w, int h,
                           int radius)
{
    for (int pass = 0; pass < 3; pass++)
        gui_blur_box(s, x, y, w, h, radius);
}