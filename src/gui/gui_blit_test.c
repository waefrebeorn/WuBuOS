/* gui_blit_test.c -- the shared blit, on cases the DOS box cannot reach. */
#include "gui_blit.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (c) printf("  [PASS] %s\n", m); else { printf("  [FAIL] %s\n", m); failures++; } } while (0)

int main(void) {
    printf("gui blit\n");
    uint32_t dst[8 * 6];
    memset(dst, 0, sizeof dst);
    uint8_t src[2 * 2 * 4];
    for (int i = 0; i < 4; i++) { src[i*4+0]=0x10; src[i*4+1]=0x20; src[i*4+2]=0x30; src[i*4+3]=0xff; }

    CHECK(wubu_blit_rgba(dst, 8, 6, src, 2, 2, 1, 1, 4, 4) == 16, "a 2x2 source fills a 4x4 rectangle (16 pixels)");
    CHECK((dst[1*8+1] & 0xffffff) == 0x302010, "pixel carries the source RGB in 0x00RRGGBB order");
    CHECK((dst[0] & 0xffffff) == 0, "outside the rectangle is untouched");
    CHECK((dst[5*8+5] & 0xffffff) == 0, "the far corner is untouched");

    /* clipped: half the destination is off-surface, and it must not write outside */
    memset(dst, 0, sizeof dst);
    int n = wubu_blit_rgba(dst, 8, 6, src, 2, 2, 7, 5, 4, 4);
    CHECK(n == 1, "a rectangle hanging off the surface writes only the visible pixel");
    CHECK((dst[5*8+7] & 0xffffff) == 0x302010, "the visible pixel is the right one");

    CHECK(wubu_blit_rgba(NULL, 8, 6, src, 2, 2, 0, 0, 2, 2) == 0, "NULL destination writes nothing");
    CHECK(wubu_blit_rgba(dst, 8, 6, NULL, 2, 2, 0, 0, 2, 2) == 0, "NULL source writes nothing");
    CHECK(wubu_blit_rgba(dst, 8, 6, src, 0, 0, 0, 0, 2, 2) == 0, "a degenerate source writes nothing");

    /* a 1x1 source stretched to the whole surface: every pixel is the source pixel */
    uint8_t one[4] = { 0xff, 0x00, 0x00, 0xff };
    memset(dst, 0, sizeof dst);
    CHECK(wubu_blit_rgba(dst, 8, 6, one, 1, 1, 0, 0, 8, 6) == 48, "1x1 stretched fills the surface");
    int all = 1;
    for (int i = 0; i < 48; i++) if ((dst[i] & 0xffffff) != 0x0000ff) all = 0;
    CHECK(all, "every stretched pixel is red");

    printf("gui blit: %s (%d failure(s))\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
