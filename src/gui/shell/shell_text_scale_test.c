/* Gate for the vbe <-> text-face bridge.
 *
 * Two claims, and the second is the one that keeps me honest:
 *   1. When a face is attached, magnified text is re-rasterised at 13 * scale (crisp outlines).
 *   2. When no face is attached, the output is BYTE-IDENTICAL to the old bilinear resample -- so this
 *      change added a path instead of silently altering the fallback every bare-metal build uses.
 *
 * The dispatch itself is checked with a stub rather than by inspecting pixels: the only thing that can
 * go wrong in "ask the hook first" is asking it with the wrong ppem, or drawing anyway after it
 * declined.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "shell_text.h"
#include "shell_text_vbe.h"
#include "../kernel/vbe.h"
#include "wubu_tokens.h"          /* caption-bar height: the unit the canvas is derived from */
#include "../fonts/wubu_font_text.h"   /* WUBU_FONT_TEXT_PX: the baked size the ppem must scale from */

/* Nothing here is a bare geometry literal: the canvas derives from the caption token, the expected
 * ppem from the baked size, and "soft" is a fraction of full coverage rather than 31/225. */
#define H        (WUBU_TITLE_BTN_H * 4)
#define W        (H * 4)
#define PIXEL_MAX 0xFF
#define SOFT_LO  (PIXEL_MAX / 8)                    /* core ink: darkest eighth */
#define SOFT_HI  (PIXEL_MAX - PIXEL_MAX / 8)        /* core ink: lightest eighth */

static int g_pass, g_fail;

static void ok(int cond, const char *what)
{
    if (cond) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL %s\n", what);
    }
}

/* Draws go to the DRAW SURFACE, which carries a stride; reading vbe_framebuffer() looks at a
 * different buffer and reports zero ink for text that was drawn correctly. */
static uint32_t *surface(int *stride)
{
    int h = 0;
    uint32_t *fb = vbe_draw_surface(stride, &h);
    return fb;
}

static long mass(void)
{
    int stride = 0;
    const uint32_t *fb = surface(&stride);
    if (!fb || stride <= 0) return -1;
    long m = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) m += (long)(fb[y * stride + x] & 0xFF);
    return m;
}

static void clear(void)
{
    int stride = 0;
    uint32_t *fb = surface(&stride);
    if (!fb || stride <= 0) return;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) fb[y * stride + x] = 0;
}

static void snapshot(uint32_t *dst)
{
    int stride = 0;
    const uint32_t *fb = surface(&stride);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) dst[y * W + x] = fb[y * stride + x];
}

static int same_as(const uint32_t *want)
{
    int stride = 0;
    const uint32_t *fb = surface(&stride);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (fb[y * stride + x] != want[y * W + x]) return 0;
    return 1;
}

/* --- stub renderer: records what it was asked for, draws nothing --- */
static int g_stub_calls, g_stub_ppem, g_stub_declines;
static int stub(int x, int y, const char *s, uint32_t colour, int ppem)
{
    (void)x; (void)y; (void)s; (void)colour;
    g_stub_calls++;
    g_stub_ppem = ppem;
    if (g_stub_declines) return -1;
    return WUBU_TITLE_BTN_H;   /* any positive advance: the stub draws nothing */
}

int main(void)
{
    vbe_init(W, H);
    vbe_set_text_scale(2);

    /* --- baseline: no face, the bilinear resample --- */
    clear();
    vbe_draw_text(10, 10, "Settings", 0xFFFFFF, 1);
    const long bilinear = mass();
    ok(bilinear > 0, "the fallback path draws text");

    static uint32_t baseline[H * W];
    snapshot(baseline);

    /* --- stub: it must be asked at 13 * scale and nothing drawn if it declines --- */
    g_stub_calls = g_stub_ppem = 0;
    vbe_set_face_text(stub);
    clear();
    vbe_draw_text(10, 10, "Settings", 0xFFFFFF, 1);
    ok(g_stub_calls > 0, "the scaled path asks the face renderer first");
    ok(g_stub_ppem == WUBU_FONT_TEXT_PX * 2, "it asks for ppem = baked size * magnification");
    ok(mass() == 0, "it does not draw on the fallback path after the renderer accepts");

    g_stub_declines = 1;
    clear();
    vbe_draw_text(10, 10, "Settings", 0xFFFFFF, 1);
    ok(mass() == bilinear, "a declining renderer leaves the fallback to draw the whole string");
    g_stub_declines = 0;

    /* --- the real adapter: same string, crisp, and visibly not the bilinear result --- */
    vbe_set_face_text(NULL);
    ok(!shell_text_ready(), "no face before init");
    if (!shell_text_init("third_party/fonts/selawik.ttf")) {
        printf("  FAIL cannot open the vendored face\n");
        return 1;
    }
    ok(shell_text_vbe_attach() == 1, "attach succeeds once a face is loaded");
    ok(vbe_get_face_text() != NULL, "attach registers the renderer with vbe");

    clear();
    vbe_draw_text(10, 10, "Settings", 0xFFFFFF, 1);
    const long crisp = mass();
    ok(crisp > 0, "the crisp path draws text");
    ok(crisp != bilinear, "the crisp path is not the bilinear result");

    /* Crispness, measured at the moment the crisp image exists: a bilinear upscale spreads each edge
     * across ~2px, so it spends MORE of its ink on intermediate coverage than a re-rasterised outline
     * at the same size. Ink mass cannot tell them apart against the 26px oracle (0.90 vs 0.91). */
    long csoft = 0, cink = 0, bsoft = 0, bink = 0;
    {
        int stride = 0;
        const uint32_t *fb = surface(&stride);
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                unsigned v = fb[y * stride + x] & 0xFF;
                if (v) { cink++; if (v > SOFT_LO && v < SOFT_HI) csoft++; }
                unsigned b = baseline[y * W + x] & 0xFF;
                if (b) { bink++; if (b > SOFT_LO && b < SOFT_HI) bsoft++; }
            }
        }
    }
    ok(!same_as(baseline), "pixels differ from the upscale");

    /* --- detach restores the old output exactly --- */
    shell_text_vbe_detach();
    clear();
    vbe_draw_text(10, 10, "Settings", 0xFFFFFF, 1);
    ok(same_as(baseline), "after detach the fallback is byte-identical to before the change");

    printf("  soft/inked pixels: crisp %ld/%ld, bilinear %ld/%ld\n", csoft, cink, bsoft, bink);
    ok(cink > 0 && bink > 0 && csoft * bink < bsoft * cink,
       "the crisp path spends a smaller share of its ink on soft edge pixels than the upscale");
    printf("  bilinear mass=%ld crisp mass=%ld (ratio %.2f)\n",
           bilinear, crisp, bilinear ? (double)crisp / (double)bilinear : 0.0);

    shell_text_shutdown();
    printf("shell_text_vbe: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
