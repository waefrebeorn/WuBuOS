/*
 * gfx_test.c -- behaviour gate for the gui/gfx compositing foundation.
 *
 * Asserts real behaviour against hand-computed references: exact src-over
 * values, clip honouring, AA coverage symmetry/monotonicity, blur invariants
 * (constant field fixed, interior energy conserved, radius-0 no-op).
 * "It compiled" is not a gate.
 */
#include "gfx.h"
#include "gfx_internal.h"
#include "../../kernel/vbe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

static void test_surface(void)
{
    GuiSurface *s = gui_surface_create(16, 12);
    CHECK(s != NULL, "surface create");
    CHECK(gui_surface_width(s) == 16 && gui_surface_height(s) == 12, "dims");
    gui_surface_clear(s, 0xFF102030u);
    CHECK(gui_surface_get(s, 5, 5) == 0xFF102030u, "clear");
    gui_surface_set(s, 2, 3, 0xAABBCCDDu);
    CHECK(gui_surface_get(s, 2, 3) == 0xAABBCCDDu, "set/get");
    CHECK(gui_surface_get(s, -1, 0) == 0 && gui_surface_get(s, 16, 0) == 0,
          "out-of-bounds get is 0");
    gui_surface_set(s, 99, 99, 0xFFFFFFFFu);  /* must not crash */
    gui_surface_fill_rect(s, -4, -4, 8, 8, 0xFF00FF00u);
    CHECK(gui_surface_get(s, 0, 0) == 0xFF00FF00u, "fill clips negative origin");
    CHECK(gui_surface_get(s, 4, 4) == 0xFF102030u, "fill stops at w/h");
    gui_surface_destroy(s);
    CHECK(gui_surface_create(0, 5) == NULL, "bad dims refused");
    CHECK(gui_surface_create(100000, 100000) == NULL, "oversize refused");
}

static void test_blend_math(void)
{
    /* 50%-ish red (a=128) over opaque black: r = 255*128/255 = 128 exact. */
    GuiSurface *s = gui_surface_create(4, 4);
    gui_surface_clear(s, 0xFF000000u);
    gui_surface_blend(s, 1, 1, 0x80FF0000u);
    CHECK(gui_surface_get(s, 1, 1) == 0xFF800000u, "src-over 128/255 red = 128");
    /* transparent leaves dst untouched */
    gui_surface_blend(s, 1, 1, 0x00FFFFFFu);
    CHECK(gui_surface_get(s, 1, 1) == 0xFF800000u, "alpha 0 is a no-op");
    /* opaque replaces */
    gui_surface_blend(s, 1, 1, 0xFF00FF00u);
    CHECK(gui_surface_get(s, 1, 1) == 0xFF00FF00u, "alpha 255 replaces");
    /* white a=128 over black = 128 gray */
    gui_surface_clear(s, 0xFF000000u);
    gui_surface_blend(s, 0, 0, 0x80FFFFFFu);
    CHECK(gui_surface_get(s, 0, 0) == 0xFF808080u, "white 128 over black = 128");
    gui_surface_destroy(s);
}

static void test_vbe_blit(void)
{
    vbe_init(32, 32);
    vbe_clear(0xFF000000u);
    GuiSurface *s = gui_surface_create(8, 8);
    gui_surface_clear(s, 0x00000000u);              /* transparent */
    gui_surface_fill_rect(s, 0, 0, 4, 8, 0xFF0000FFu); /* opaque blue half */
    gui_surface_fill_rect(s, 4, 0, 4, 8, 0x80FF0000u); /* 50% red half */
    gui_blit_to_vbe(s, 4, 4);
    CHECK(vbe_get_pixel(0, 0) == 0xFF000000u, "transparent region untouched");
    CHECK(vbe_get_pixel(5, 5) == 0xFF0000FFu, "opaque blit");
    CHECK(vbe_get_pixel(9, 5) == 0xFF800000u, "alpha blit over black");
    /* clip honoured: only the top-left 6x6 of the screen may change */
    vbe_clear(0xFF000000u);
    vbe_set_clip(0, 0, 6, 6);
    gui_blit_to_vbe(s, 4, 4);
    CHECK(vbe_get_pixel(5, 5) == 0xFF0000FFu, "inside clip written");
    CHECK(vbe_get_pixel(9, 5) == 0xFF000000u, "outside clip untouched");
    vbe_reset_clip();
    /* global alpha: opaque white at 128 over black = 128 gray */
    gui_surface_clear(s, 0xFFFFFFFFu);
    vbe_clear(0xFF000000u);
    gui_blit_to_vbe_alpha(s, 0, 0, 128);
    CHECK(vbe_get_pixel(2, 2) == 0xFF808080u, "global alpha 128");
    gui_blit_to_vbe_alpha(s, 0, 0, 0);
    CHECK(vbe_get_pixel(2, 2) == 0xFF808080u, "global alpha 0 invisible");
    gui_surface_destroy(s);
    vbe_shutdown();
}

static void test_rounded(void)
{
    GuiSurface *s = gui_surface_create(20, 20);
    gui_surface_clear(s, 0x00000000u);
    gui_fill_rounded(s, 0, 0, 20, 20, 5, 0xFFFFFFFFu);
    uint32_t corner = GUI_A(gui_surface_get(s, 0, 0));
    uint32_t inner  = GUI_A(gui_surface_get(s, 2, 2));
    uint32_t centre = GUI_A(gui_surface_get(s, 10, 10));
    CHECK(corner == 0, "far corner fully outside r=5");
    CHECK(inner == 255, "inner corner pixel fully inside");
    CHECK(centre == 255, "centre opaque");
    /* an edge pixel must have fractional coverage (the AA signature) */
    int found_fractional = 0;
    for (int i = 0; i < 5; i++) {
        uint32_t a = GUI_A(gui_surface_get(s, i, 0));
        if (a > 0 && a < 255) found_fractional = 1;
    }
    CHECK(found_fractional, "corner edge has fractional coverage");
    /* symmetry across both axes */
    int sym = 1;
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 20; x++) {
            if (gui_surface_get(s, x, y) != gui_surface_get(s, 19 - x, y) ||
                gui_surface_get(s, x, y) != gui_surface_get(s, x, 19 - y))
                sym = 0;
        }
    CHECK(sym, "coverage symmetric on both axes");
    /* radius 0 == plain fill */
    gui_surface_clear(s, 0x00000000u);
    gui_fill_rounded(s, 0, 0, 20, 20, 0, 0xFF123456u);
    CHECK(gui_surface_get(s, 0, 0) == 0xFF123456u, "radius 0 fills square");
    gui_surface_destroy(s);
}

static void test_blur(void)
{
    /* constant field is a fixed point (clamp-to-edge, no darkening) */
    GuiSurface *s = gui_surface_create(12, 12);
    gui_surface_clear(s, 0xFF306090u);
    gui_blur_box(s, 0, 0, 12, 12, 3);
    int constant = 1;
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 12; x++)
            if (gui_surface_get(s, x, y) != 0xFF306090u) constant = 0;
    CHECK(constant, "blur fixes a constant field (edges included)");
    /* radius 0 no-op */
    gui_surface_set(s, 6, 6, 0xFFABCDEFu);
    gui_blur_box(s, 0, 0, 12, 12, 0);
    CHECK(gui_surface_get(s, 6, 6) == 0xFFABCDEFu, "radius 0 no-op");
    /* single interior impulse: 3x3 box spreads 255 over 9 pixels */
    gui_surface_clear(s, 0xFF000000u);
    gui_surface_set(s, 6, 6, 0xFFFFFFFFu);
    gui_blur_box(s, 0, 0, 12, 12, 1);
    CHECK(GUI_R(gui_surface_get(s, 6, 6)) == 255 / 9, "impulse centre = 255/9");
    CHECK(gui_surface_get(s, 0, 0) == 0xFF000000u, "far corner untouched");
    uint32_t sum = 0;
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 12; x++) sum += GUI_R(gui_surface_get(s, x, y));
    /* Integer division truncates: each of the <=9 touched pixels can lose
     * at most 1 per pass (2 passes), so energy is conserved within 18. */
    CHECK(sum <= 255 && sum >= 255 - 18,
          "interior impulse energy conserved within integer rounding");
    /* gauss approx keeps a constant field too */
    gui_surface_clear(s, 0xFF204060u);
    gui_blur_gauss_approx(s, 0, 0, 12, 12, 2);
    CHECK(gui_surface_get(s, 0, 0) == 0xFF204060u &&
          gui_surface_get(s, 11, 11) == 0xFF204060u,
          "gauss approx fixes constant field");
    gui_surface_destroy(s);
}

int main(void)
{
    test_surface();
    test_blend_math();
    test_vbe_blit();
    test_rounded();
    test_blur();
    if (failures) { printf("gui_gfx: %d FAILURES\n", failures); return 1; }
    printf("gui_gfx: ALL PASS\n");
    return 0;
}