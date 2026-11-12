/*
 * wubu_mascot.c -- WuBu Buddy sprite blitter.
 *
 * One concern: alpha-composite the baked clipart onto the desktop.
 * The sprite table is generated (wubu_buddy_sprite.h), so the WM needs
 * no image decoder and no asset loader -- the same discipline as an
 * embedded font.
 *
 * Draws through vbe_* (back buffer + clipping), matching the rest of
 * the mascot drawing.
 *
 * C11, opaque API (wubu_mascot.h).
 */

#include "wubu_mascot.h"
#include "wubu_buddy_sprite.h"
#include "../kernel/vbe.h"

#include <stddef.h>

int wubu_mascot_size(void) {
    return WUBU_BUDDY_SPRITE_SIZE;
}

void wubu_mascot_blit_rgba(int x, int y, const unsigned char *rgba, int size) {
    if (!rgba || size <= 0) return;
    const int n = size;
    const unsigned char *src = rgba;

    for (int dy = 0; dy < n; dy++) {
        int py = y + dy;
        if (py < 0) continue;
        const unsigned char *row = src + (size_t)dy * n * 4;
        for (int dx = 0; dx < n; dx++) {
            int px = x + dx;
            if (px < 0) continue;

            int a = row[dx * 4 + 3];
            if (a == 0) continue;

            if (a >= 255) {
                vbe_set_pixel(px, py, 0xFF000000u |
                                     ((uint32_t)row[dx * 4 + 0] << 16) |
                                     ((uint32_t)row[dx * 4 + 1] << 8) |
                                     (uint32_t)row[dx * 4 + 2]);
                continue;
            }

            /* Source-over composite; the desktop is opaque. */
            uint32_t dst = vbe_get_pixel(px, py);
            uint32_t dr = (dst >> 16) & 0xFFu;
            uint32_t dg = (dst >> 8) & 0xFFu;
            uint32_t db = dst & 0xFFu;
            uint32_t inv = (uint32_t)(255 - a);
            uint32_t sr = row[dx * 4 + 0];
            uint32_t sg = row[dx * 4 + 1];
            uint32_t sb = row[dx * 4 + 2];
            uint32_t rr = (sr * (uint32_t)a + dr * inv) / 255u;
            uint32_t gg = (sg * (uint32_t)a + dg * inv) / 255u;
            uint32_t bb = (sb * (uint32_t)a + db * inv) / 255u;
            vbe_set_pixel(px, py, 0xFF000000u | (rr << 16) | (gg << 8) | bb);
        }
    }
}

void wubu_mascot_blit(int x, int y) {
    wubu_mascot_blit_rgba(x, y, wubu_buddy_rgba, WUBU_BUDDY_SPRITE_SIZE);
}
