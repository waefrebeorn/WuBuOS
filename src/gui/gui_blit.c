/* gui_blit.c -- see gui_blit.h. Behaviour is identical to the static function this
 * was extracted from, deliberately: the DOS box was already correct and a refactor
 * that changes what it draws is not a refactor. */
#include "gui_blit.h"

int wubu_blit_rgba(uint32_t *dst, int dst_w, int dst_h,
                   const uint8_t *src, int src_w, int src_h,
                   int dx, int dy, int dw, int dh) {
    if (!dst || !src || src_w <= 0 || src_h <= 0 || dw <= 0 || dh <= 0) return 0;
    int written = 0;
    for (int y = 0; y < dh; y++) {
        int sy = (y * src_h) / dh;
        if (sy >= src_h) sy = src_h - 1;
        for (int x = 0; x < dw; x++) {
            int sx = (x * src_w) / dw;
            if (sx >= src_w) sx = src_w - 1;
            int px = dx + x, py = dy + y;
            if (px < 0 || py < 0 || px >= dst_w || py >= dst_h) continue;
            const uint8_t *p = src + (sy * src_w + sx) * 4;
            dst[py * dst_w + px] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                                   ((uint32_t)p[2] << 16) | ((uint32_t)255 << 24);
            written++;
        }
    }
    return written;
}
