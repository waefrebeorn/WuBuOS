/* wubu_canvas_blend.c -- Canvas blend-compositing subsystem (self-contained).
 *
 * blend_channel + wubu_blend: per-channel blend modes (multiply/screen/
 * overlay/etc) with opacity. Uses WubuBlendMode / BLEND_* (wubu_canvas.h).
 * Minimal includes.
 */

#include "wubu_canvas.h"

static inline uint8_t blend_channel(uint8_t dst, uint8_t src,
                                     uint8_t opacity, WubuBlendMode mode) {
    int a = src, b = dst;
    int result;
    switch (mode) {
        case BLEND_MULTIPLY:   result = a * b / 255; break;
        case BLEND_SCREEN:     result = 255 - (255 - a) * (255 - b) / 255; break;
        case BLEND_DIFFERENCE: result = abs(a - b); break;
        case BLEND_ADDITION:   result = a + b; if (result > 255) result = 255; break;
        case BLEND_SUBTRACT:   result = a - b; if (result < 0) result = 0; break;
        case BLEND_DARKEN:    result = a < b ? a : b; break;
        case BLEND_LIGHTEN:   result = a > b ? a : b; break;
        case BLEND_OVERLAY:
            result = b < 128 ? (2 * a * b / 255) : (255 - 2 * (255 - a) * (255 - b) / 255);
            break;
        case BLEND_COLOR_DODGE:
            result = (a == 255) ? 255 : (b * 255 / (255 - a));
            if (result > 255) result = 255; break;
        case BLEND_COLOR_BURN:
            result = (a == 0) ? 0 : (255 - (255 - b) * 255 / a);
            if (result < 0) result = 0; break;
        case BLEND_HARD_LIGHT:
            result = a < 128 ? (2 * a * b / 255) : (255 - 2 * (255 - a) * (255 - b) / 255);
            break;
        case BLEND_SOFT_LIGHT:
            result = b + (2 * a - 255) * b * (255 - b) / (255 * 255);
            if (result < 0) result = 0; if (result > 255) result = 255;
            break;
        default: result = a; break;  /* NORMAL */
    }
    /* Apply opacity */
    return (uint8_t)(opacity * result / 255 + (255 - opacity) * b / 255);
}

uint32_t wubu_blend(uint32_t dst, uint32_t src, uint8_t opacity,
                     WubuBlendMode mode) {
    uint8_t dr = dst & 0xFF, dg = (dst >> 8) & 0xFF, db = (dst >> 16) & 0xFF;
    uint8_t sr = src & 0xFF, sg = (src >> 8) & 0xFF, sb = (src >> 16) & 0xFF;
    uint8_t r = blend_channel(dr, sr, opacity, mode);
    uint8_t g = blend_channel(dg, sg, opacity, mode);
    uint8_t b = blend_channel(db, sb, opacity, mode);
    return r | (g << 8) | (b << 16);
}
