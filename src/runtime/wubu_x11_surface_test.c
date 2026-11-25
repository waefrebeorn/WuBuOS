/* wubu_x11_surface_test.c -- the pixels a client sends, and the damage they cause. */
#include "wubu_x11_surface.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (c) printf("  [PASS] %s\n", m); else { printf("  [FAIL] %s\n", m); failures++; } } while (0)

int main(void) {
    printf("x11 surface\n");
    uint8_t px[32 * 24 * 4];
    WubuX11Surface s;
    wubu_x11_surface_init(&s, px, 32, 24);

    wubu_x11_surface_clear(&s, 0, 0, 0);
    CHECK(wubu_x11_surface_dirty(&s), "a fresh surface is dirty");
    CHECK(s.pixels[3] == 255, "cleared pixels are opaque");

    /* a frame has been presented: damage resets, so the NEXT write is measured
     * from nothing rather than from the initial full-surface clear */
    wubu_x11_surface_clear_damage(&s);
    CHECK(!wubu_x11_surface_dirty(&s), "cleared damage leaves nothing dirty");

    /* PutImage a 2x2 block of red at (4,5), little-endian client */
    uint8_t img[2 * 2 * 4];
    for (int i = 0; i < 4; i++) { img[i*4+0]=0x00; img[i*4+1]=0x00; img[i*4+2]=0xff; img[i*4+3]=0x00; }
    CHECK(wubu_x11_surface_put_image(&s, 4, 5, 2, 2, img, sizeof img, 1, 24) == 0,
          "PutImage inside the surface is accepted");
    size_t off = ((size_t)5 * 32 + 4) * 4;
    CHECK(s.pixels[off] == 255 && s.pixels[off+1] == 0 && s.pixels[off+2] == 0,
          "wire 0x00RRGGBB arrives as RGBA red");
    CHECK(s.dirty_x0 == 4 && s.dirty_y0 == 5 && s.dirty_x1 == 6 && s.dirty_y1 == 7,
          "damage covers exactly the written rectangle");

    /* a second block grows the damage rect rather than replacing it */
    CHECK(wubu_x11_surface_put_image(&s, 10, 2, 1, 1, img, 4, 1, 24) == 0, "second block accepted");
    CHECK(s.dirty_x0 == 4 && s.dirty_y0 == 2 && s.dirty_x1 == 11 && s.dirty_y1 == 7,
          "damage is the union of both writes");

    /* refusing is better than drawing half a window */
    CHECK(wubu_x11_surface_put_image(&s, 31, 23, 4, 4, img, sizeof img, 1, 24) < 0,
          "a rectangle that overflows is refused");
    CHECK(wubu_x11_surface_put_image(&s, 0, 0, 2, 2, img, 4, 1, 24) < 0,
          "a short payload is refused");

    /* depth 8: one byte per pixel, grey */
    uint8_t mono[2] = { 0x40, 0x80 };
    CHECK(wubu_x11_surface_put_image(&s, 0, 0, 2, 1, mono, 2, 1, 8) == 0, "depth 8 accepted");
    CHECK(s.pixels[0] == 0x40 && s.pixels[4] == 0x80, "depth 8 bytes become grey pixels");

    printf("x11 surface: %s (%d failure(s))\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
