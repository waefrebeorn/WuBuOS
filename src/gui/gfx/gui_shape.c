/*
 * gui_shape.c -- anti-aliased shapes (one concern: coverage).
 *
 * Design ref: rizonesoft/impossible-os@be8c286 src/kernel/gfx/gfx_core.c
 * rounded-rect AA (GPL-3.0-only). Ground-up WuBuOS C11 implementation; no
 * upstream code copied. Divergence: 4x4 integer supersampling per corner
 * pixel (16 samples) instead of the reference's 16x fixed-point remainder;
 * same intent (fractional edge coverage), simpler to gate.
 */
#include "gui_gfx_internal.h"

#include <stdbool.h>

/* Coverage (0..255) of a pixel whose centre is (px8,py8) in 1/8 pixel
 * units, against a corner circle centred (cx8,cy8) with radius r8. */
static uint32_t corner_coverage(int px8, int py8, int cx8, int cy8, int r8)
{
    static const int off[4] = { 1, 3, 5, 7 };  /* sample centres in 1/8 px */
    int inside = 0;
    int64_t r2 = (int64_t)r8 * r8;
    for (int i = 0; i < 4; i++) {
        int64_t dx = px8 + off[i] - cx8;
        for (int j = 0; j < 4; j++) {
            int64_t dy = py8 + off[j] - cy8;
            if (dx * dx + dy * dy <= r2) inside++;
        }
    }
    return (uint32_t)(inside * 255 / 16);
}

void gui_fill_rounded(GuiSurface *s, int x, int y, int w, int h,
                      int radius, uint32_t argb)
{
    if (!s || w <= 0 || h <= 0) return;
    if (radius <= 0) { gui_surface_fill_rect(s, x, y, w, h, argb); return; }
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;

    uint32_t base_a = GUI_A(argb);
    uint32_t rgb = argb & 0x00FFFFFFu;
    int r8 = radius * 8;

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint32_t cov = 255;
            bool top = row < radius, bot = row >= h - radius;
            bool lef = col < radius, rig = col >= w - radius;
            if ((top || bot) && (lef || rig)) {
                int cx = lef ? x + radius : x + w - radius;
                int cy = top ? y + radius : y + h - radius;
                cov = corner_coverage((x + col) * 8, (y + row) * 8,
                                      cx * 8, cy * 8, r8);
            }
            if (cov == 0) continue;
            uint32_t a = base_a;
            if (cov < 255) a = gui_div255(base_a * cov);
            if (a == 0) continue;
            gui_surface_blend(s, x + col, y + row, rgb | (a << 24));
        }
    }
}