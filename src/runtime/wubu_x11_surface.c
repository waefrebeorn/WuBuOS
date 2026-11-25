/* wubu_x11_surface.c -- see wubu_x11_surface.h. */
#include "wubu_x11_surface.h"

#include <string.h>

void wubu_x11_surface_init(WubuX11Surface *s, uint8_t *pixels, int w, int h) {
    s->pixels = pixels; s->w = w; s->h = h;
    wubu_x11_surface_mark_all(s);
}

void wubu_x11_surface_mark_all(WubuX11Surface *s) {
    s->dirty_x0 = 0; s->dirty_y0 = 0; s->dirty_x1 = s->w; s->dirty_y1 = s->h;
}

void wubu_x11_surface_clear_damage(WubuX11Surface *s) {
    s->dirty_x0 = s->dirty_y0 = s->dirty_x1 = s->dirty_y1 = 0;
}

int wubu_x11_surface_dirty(const WubuX11Surface *s) {
    return s->dirty_x1 > s->dirty_x0 && s->dirty_y1 > s->dirty_y0;
}

void wubu_x11_surface_clear(WubuX11Surface *s, uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < s->w * s->h; i++) {
        s->pixels[i * 4 + 0] = r; s->pixels[i * 4 + 1] = g;
        s->pixels[i * 4 + 2] = b; s->pixels[i * 4 + 3] = 255;
    }
    wubu_x11_surface_mark_all(s);
}

static uint32_t rd32(const uint8_t *p, int lsb) {
    return lsb ? (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24))
               : (uint32_t)(((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

int wubu_x11_surface_put_image(WubuX11Surface *s, int x, int y, int w_, int h_,
                               const uint8_t *data, size_t len, int lsb, int depth) {
    if (!s || !data || w_ <= 0 || h_ <= 0) return -1;
    if (x < 0 || y < 0 || x + w_ > s->w || y + h_ > s->h) return -2;

    size_t need = depth == 8 ? (size_t)w_ * (size_t)h_
                             : (size_t)w_ * (size_t)h_ * 4u;
    if (len < need) return -3;

    for (int row = 0; row < h_; row++) {
        for (int col = 0; col < w_; col++) {
            uint8_t *dst = s->pixels + ((size_t)(y + row) * s->w + (x + col)) * 4u;
            if (depth == 8) {
                /* PseudoColor-ish: treat the byte as a grey level */
                uint8_t v = data[(size_t)row * w_ + col];
                dst[0] = dst[1] = dst[2] = v; dst[3] = 255;
            } else {
                uint32_t px = rd32(data + ((size_t)row * w_ + col) * 4u, lsb);
                /* The server advertises masks 0xff0000/0x00ff00/0x0000ff, so the
                 * wire pixel is 0xRRGGBB whatever the byte order was. */
                dst[0] = (uint8_t)((px >> 16) & 0xff);
                dst[1] = (uint8_t)((px >> 8) & 0xff);
                dst[2] = (uint8_t)(px & 0xff);
                dst[3] = 255;
            }
        }
    }

    if (s->dirty_x1 <= s->dirty_x0) {          /* nothing damaged yet */
        s->dirty_x0 = x; s->dirty_y0 = y; s->dirty_x1 = x + w_; s->dirty_y1 = y + h_;
    } else {
        if (x < s->dirty_x0) s->dirty_x0 = x;
        if (y < s->dirty_y0) s->dirty_y0 = y;
        if (x + w_ > s->dirty_x1) s->dirty_x1 = x + w_;
        if (y + h_ > s->dirty_y1) s->dirty_y1 = y + h_;
    }
    return 0;
}
