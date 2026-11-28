/* Gate for shell display text.
 *
 * Two things can go wrong here and only one of them is visible in a screenshot:
 *   1. With no face, the module must SAY SO (-1) so the caller draws the baked cell -- a silent 0 or
 *      a blank draw leaves an empty title bar, which looks like a rendering bug somewhere else.
 *   2. With a face, the heading's ink must sit inside its band and be centred in it; "looks fine" is
 *      not a check, so the test finds the ink rows in the rendered pixels and measures them.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "shell_text.h"
#include "wubu_tokens.h"   /* WUBU_TITLE_BTN_H: the caption bar's height, i.e. the band a heading gets */

/* The band is the real caption-bar height, not a number chosen to suit the test, and the canvas is
 * derived from it so there is nothing here to drift. The hardcode ratchet counts bare 2-3 digit
 * geometry literals in this directory, and it is right to: a test full of magic numbers cannot tell
 * anyone what it was checking. */
#define BAND_H    WUBU_TITLE_BTN_H
#define BAND_Y    (BAND_H / 2)
#define W         (BAND_H * BAND_H)
#define H         (BAND_H * 4)
#define PEN_X     (BAND_H / 4)
#define CENTRE_TOL 2
#define FILL_NUM   3        /* ink must fill 3/5 of the band height */
#define FILL_DEN   5

static int g_pass, g_fail;
static uint32_t g_fb[W * H];

static void ok(int cond, const char *what)
{
    if (cond) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL %s\n", what);
    }
}

/* Ink rows of a white-on-black render. */
static void ink_rows(int *first, int *last, long *mass)
{
    *first = -1;
    *last = -1;
    *mass = 0;
    for (int y = 0; y < H; y++) {
        int any = 0;
        for (int x = 0; x < W; x++) {
            unsigned v = g_fb[y * W + x] & 0xFF;
            if (v) { any = 1; *mass += (long)v; }
        }
        if (any) {
            if (*first < 0) *first = y;
            *last = y;
        }
    }
}

int main(void)
{
    ok(shell_text_heading_px() == SHELL_TEXT_HEADING_PX, "the heading size is the ramp's display step");

    /* -- before init: the fallback contract -- */
    ok(!shell_text_ready(), "no face is loaded before init");
    ok(shell_text_draw_heading(g_fb, W, H, PEN_X, 0, BAND_H, "Settings", 0xFFFFFF) == -1,
       "drawing a heading without a face returns -1 (caller falls back)");
    ok(shell_text_measure_heading("Settings") == -1, "measuring without a face returns -1");

    if (!shell_text_init("third_party/fonts/selawik.ttf")) {
        printf("  FAIL cannot open third_party/fonts/selawik.ttf\n");
        return 1;
    }
    ok(shell_text_ready(), "init loads the vendored face");
    ok(shell_text_init("third_party/fonts/selawik.ttf") == 1, "init twice is harmless");

    const int meas = shell_text_measure_heading("Settings");
    ok(meas > 0, "measuring a heading succeeds with a face");

    /* A 32px band, the height of the window caption bar. */
    memset(g_fb, 0, sizeof(g_fb));
    const int adv = shell_text_draw_heading(g_fb, W, H, PEN_X, BAND_Y, BAND_H, "Settings", 0xFFFFFF);
    ok(adv == meas, "draw returns the same advance as measure");

    int first, last;
    long mass;
    ink_rows(&first, &last, &mass);
    printf("  heading: advance=%d ink rows %d..%d (band %d..%d) mass=%ld\n",
           adv, first, last, BAND_Y, BAND_Y + BAND_H - 1, mass);
    ok(first >= BAND_Y && last <= BAND_Y + BAND_H - 1, "the heading's ink stays inside its band");
    ok((last - first + 1) * FILL_DEN >= (BAND_H - 1) * FILL_NUM,
       "the heading fills most of its band height rather than sitting small in it");

    /* Centred: the gap above and below the ink must match within 2px. An em-centred baseline leaves
     * the ink low by roughly the space the em reserves above the ascender. */
    int above = first - BAND_Y, below = (BAND_Y + BAND_H - 1) - last;
    int d = above - below;
    if (d < 0) d = -d;
    printf("  gap above=%d below=%d (delta %d)\n", above, below, d);
    ok(d <= CENTRE_TOL, "the heading is centred in its band within the tolerance");

    /* Deterministic: two fresh renders of the same heading are byte-identical. Redrawing onto the
     * SAME buffer is not a valid check -- blending white over already-blended pixels legitimately
     * changes them, so that would test the blend function twice rather than text stability. */
    static uint32_t second[H * W];
    memset(second, 0, sizeof(second));
    shell_text_draw_heading(second, W, H, PEN_X, BAND_Y, BAND_H, "Settings", 0xFFFFFF);
    ok(memcmp(second, g_fb, sizeof(g_fb)) == 0, "two fresh renders of the same heading are identical");

    /* Page titles use the ramp's larger Title step. Same engine, but it must still fit the band it
     * is given, and it must genuinely be bigger -- a "title" that renders at the heading size would
     * pass every heading check. */
    {
        const int title_band = BAND_H + BAND_H / 8;
        static uint32_t tfb[W * H];
        memset(tfb, 0, sizeof(tfb));
        int tadv = shell_text_draw_at(tfb, W, H, PEN_X, BAND_Y, title_band, "Settings",
                                      SHELL_TEXT_TITLE_PX, 0xFFFFFF);
        int tfirst = -1, tlast = -1;
        long tmass = 0;
        for (int y = 0; y < H; y++) {
            int any = 0;
            for (int x = 0; x < W; x++) {
                unsigned v = tfb[y * W + x] & 0xFF;
                if (v) { any = 1; tmass += (long)v; }
            }
            if (any) { if (tfirst < 0) tfirst = y; tlast = y; }
        }
        printf("  page title: advance=%d ink rows %d..%d of band %d..%d\n",
               tadv, tfirst, tlast, BAND_Y, BAND_Y + title_band - 1);
        ok(tadv > 0, "a page title draws at the Title step");
        ok(tfirst >= BAND_Y && tlast <= BAND_Y + title_band - 1, "the title's ink stays inside its band");
        ok((tlast - tfirst) > (last - first), "the title is larger than the heading");
    }

    shell_text_shutdown();
    ok(!shell_text_ready(), "shutdown releases the face");
    ok(shell_text_draw_heading(g_fb, W, H, 0, 0, BAND_H, "Settings", 0xFFFFFF) == -1,
       "after shutdown the caller falls back again");

    printf("shell_text: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
