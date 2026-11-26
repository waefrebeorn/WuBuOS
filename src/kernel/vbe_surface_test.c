/*
 * vbe_surface_test.c -- "the buffer I was handed" must be "the buffer vbe_* writes".
 *
 * This is the gate for a whole class of invisible UI: a renderer that indexes a framebuffer pointer
 * directly (rather than calling vbe_*) draws into whatever it was given, and if that is not g_vbe.back
 * the pixels land in a buffer nothing presents. Measured consequence: the Control Panel window drew its
 * chrome (vbe_* -> back) and an empty body (direct writes -> vbe_state()->fb), and a full-rect magenta
 * fill through the callback never reached a single presented frame.
 *
 * The assertion is deliberately physical, not structural: write a pixel with the DRAWING API, then read
 * that pixel back through the ACCESSOR. If they are not the same memory, this fails.
 *
 * C11.
 */
#include "vbe.h"

#include <stdio.h>
#include <stdint.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

int main(void)
{
    printf("=== vbe: the draw surface is the drawing surface ===\n");

    vbe_init(320, 200);          /* hosted mode: allocates its own buffer */
    vbe_reset_clip();

    int stride = 0, height = 0;
    uint32_t *surf = vbe_draw_surface(&stride, &height);

    CHECK(surf != NULL, "the accessor returns a surface");
    CHECK(height == 200, "the accessor reports the real height");
    CHECK(stride >= 320, "the accessor reports a stride wide enough for the width");

    /* THE ASSERTION: draw through the API, read through the accessor. */
    const uint32_t colour = 0x00112233u;
    vbe_fill_rect(5, 7, 1, 1, colour);

    if (surf && stride >= 320) {
        uint32_t through_surface = surf[(size_t)7 * (size_t)stride + 5];
        uint32_t through_api = vbe_get_pixel(5, 7);
        CHECK(through_surface == colour,
              "a pixel drawn with vbe_fill_rect is visible through vbe_draw_surface");
        CHECK(through_api == colour, "and through vbe_get_pixel");
        CHECK(through_surface == through_api, "the two agree on the same pixel");
    }

    /* A second, distant pixel: a stride mix-up usually shows up away from the origin. */
    const uint32_t colour2 = 0x00AABBCCu;
    vbe_fill_rect(300, 190, 4, 4, colour2);
    if (surf && stride >= 320) {
        CHECK(surf[(size_t)190 * (size_t)stride + 300] == colour2,
              "the bottom-right region also agrees (stride, not width)");
    }

    /* The clip must apply to the drawing API regardless of who holds the pointer. */
    vbe_set_clip(0, 0, 10, 10);
    vbe_fill_rect(50, 50, 4, 4, 0x00FF00FFu);
    vbe_reset_clip();
    CHECK(vbe_get_pixel(52, 52) != 0x00FF00FFu, "the clip still governs the drawing API");

    printf("\n  vbe_surface: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
