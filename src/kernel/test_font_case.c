/*
 * test_font_case.c -- lower-case text must render as lower case.
 *
 * Regression cover for laundry-list item 23.  The 64-glyph upper-case table
 * stops at '_' (95); until a lower-case table existed, vbe_draw_char() folded
 * 'a'-'z' upward, so every string in the OS rendered in capitals -- including
 * source strings nobody had typed.  The fix is a second table, and this test
 * pins the contract: each lower-case letter draws ITS OWN bitmap, not the
 * upper-case one, and not blank.
 */

#include "../kernel/vbe.h"

#include <stdio.h>
#include <string.h>

static int g_checks = 0, g_fails = 0;
#define TEST(cond, msg) do {                                            \
        g_checks++;                                                     \
        if (!(cond)) { g_fails++;                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); }    \
    } while (0)

#define FB_W 128
#define FB_H 32

/* Non-background pixels in a cell: the antialiased renderer's equivalent of "a glyph is
 * here". Exact-bit comparison belongs to the bitmap tables (section 1), not to a frame. */
static int cell_ink(const uint32_t *fb, int x0, int y0)
{
    int n = 0;
    for (int r = 0; r < 14; r++)
        for (int c = 0; c < 10; c++)
            if (x0 + c < FB_W && fb[(y0 + r) * FB_W + (x0 + c)] != 0) n++;
    return n;
}

/* Compare one drawn cell against the bitmap we expect it to be. */
static int cell_matches(const uint32_t *fb, int x0, int y0,
                        const uint8_t *glyph, const char *what)
{
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            uint32_t px = fb[(y0 + r) * FB_W + (x0 + c)];
            int want = (glyph[r] >> (7 - c)) & 1;
            if ((px != 0) != (want != 0)) {
                printf("  mismatch for '%s' at row %d col %d "
                       "(pixel=%06X want=%d)\n", what, r, c, px & 0xFFFFFF, want);
                return 0;
            }
        }
    }
    return 1;
}

int main(void)
{
    if (vbe_init(FB_W, FB_H) != 0) {
        printf("SKIP: could not allocate a test framebuffer\n");
        return 0;
    }
    uint32_t *fb = vbe_framebuffer();
    TEST(fb != NULL, "framebuffer allocated");
    /* Drawing lands in the BACK buffer; the visible one only updates on swap. */
    int stride = vbe_width();
    TEST(stride == FB_W, "requested width honoured");

    /* ---- 1. the tables themselves --------------------------------- */
    for (int i = 0; i < 26; i++) {
        const uint8_t *lo = vbe_font_lower_8x8[i];
        const uint8_t *up = vbe_font_8x8[('A' + i) - 32];

        int nonempty = 0;
        for (int r = 0; r < 8; r++) nonempty |= (lo[r] != 0);

        char name[8];
        snprintf(name, sizeof(name), "%c", 'a' + i);
        TEST(nonempty, name);
        TEST(memcmp(lo, up, 8) != 0,
             "lower-case letter differs from its upper-case form");
    }

    /* ---- 1b. the codes the two tables do not cover ---------------- */
    /* '`', '{', '|', '}', '~' had no glyph and were drawn as a SPACE, so JSON and shell code shown in an
     * editor or terminal rendered with holes in it. Each must exist, be non-empty, and differ from space. */
    const char extra_chars[5] = { '`', '{', '|', '}', '~' };
    for (int i = 0; i < 5; i++) {
        int idx = vbe_font_extra_index(extra_chars[i]);
        char nm[32];
        snprintf(nm, sizeof nm, "extra glyph '%c' exists", extra_chars[i]);
        TEST(idx == i && memcmp(vbe_font_extra_8x8[idx], vbe_font_8x8[0], 8) != 0, nm);
    }
    TEST(vbe_font_extra_index('a') == -1 && vbe_font_extra_index(' ') == -1,
         "the extra table claims only its own characters");

    /* Draw through the BITMAP path, not the AA face. At scale 1 vbe_draw_text goes to aa_draw_text, whose
     * baked face already covers 0x20..0x7E -- so a scale-1 check here proved nothing and passed with the fix
     * removed. Text scale 2 falls back to vbe_draw_char, which is the path that lacked these glyphs. */
    vbe_set_text_scale(2);
    memset(vbe_state()->back, 0, (size_t)stride * vbe_height() * sizeof(uint32_t));
    vbe_draw_text(0, 0, "{|}~`", 0x00FFFFFF, 1);
    vbe_swap();
    {
        int pen = 0;
        for (int i = 0; i < 5; i++) {
            char nm[48];
            snprintf(nm, sizeof nm, "'%c' draws ink in the BITMAP path (scale 2)", extra_chars[i]);
            TEST(cell_ink(fb, pen, 0) > 2, nm);
            pen += vbe_text_width("{", 1);
        }
    }
    vbe_set_text_scale(1);

    /* ---- 1c. Latin-1: UTF-8 decodes, and an undrawable cp is NOT a blank ------------ */
    /* Every string used to lose anything above ASCII: vbe.c drew it as a space, so "café" was "caf " and an
     * em dash vanished. The face now carries 0xA0..0xFF and the decoder maps C2/C3 sequences to it. */
    {
        vbe_set_text_scale(1);
        memset(vbe_state()->back, 0, (size_t)stride * vbe_height() * sizeof(uint32_t));
        vbe_draw_text(0, 0, "caf\xC3\xA9", 0x00FFFFFF, 1);      /* "café" */
        vbe_swap();
        int pen_e = vbe_text_width("caf", 1);
        TEST(cell_ink(fb, pen_e, 0) > 2, "'\xC3\xA9' (e-acute) draws ink, not a blank");
        /* and it must be the LATIN-1 glyph, not the placeholder: ink alone would pass for '?' */
        {
            static uint32_t ref_cell[64 * 64];
            int cw = 16, chh = 16;
            for (int yy = 0; yy < chh; yy++)
                for (int xx = 0; xx < cw; xx++)
                    ref_cell[yy * cw + xx] = vbe_get_pixel(pen_e + xx, yy);
            memset(vbe_state()->back, 0, (size_t)stride * vbe_height() * sizeof(uint32_t));
            vbe_draw_text(0, 0, "?", 0x00FFFFFF, 1);
            vbe_swap();
            int diff = 0;
            for (int yy = 0; yy < chh; yy++)
                for (int xx = 0; xx < cw; xx++)
                    if (vbe_get_pixel(xx, yy) != ref_cell[yy * cw + xx]) diff++;
            TEST(diff > 4, "e-acute is its own glyph, not the '?' placeholder");
        }

        /* a codepoint outside the face must render a VISIBLE placeholder */
        memset(vbe_state()->back, 0, (size_t)stride * vbe_height() * sizeof(uint32_t));
        vbe_draw_text(0, 0, "\xE2\x80\x94", 0x00FFFFFF, 1);    /* em dash, U+2014 */
        vbe_swap();
        TEST(cell_ink(fb, 0, 0) > 2, "an undrawable codepoint draws a placeholder, not a space");
    }

    /* ---- 2. drawing puts the right GLYPH at the right PEN ---------- */
    /* The renderer is antialiased now: a cell no longer equals a bitmap row, so asserting
     * exact bits would assert the old rasteriser instead of the behaviour. What must stay
     * true is positional: the lower-case form lands at pen 0, the upper-case form lands
     * exactly where the pen has advanced to, and the two are not the same picture. */
    memset(vbe_state()->back, 0, (size_t)stride * vbe_height() * sizeof(uint32_t));
    vbe_draw_text(0, 0, "a A", 0x00FFFFFF, 1);
    vbe_swap();

    int penA = vbe_text_width("a ", 1);        /* advance of 'a' + the space */

    TEST(cell_ink(fb, 0, 0) > 4, "'a' draws ink at pen 0");
    TEST(cell_ink(fb, penA, 0) > 4, "'A' draws ink where the pen has advanced to");

    int differed = 0;
    for (int r = 0; r < 12; r++)
        for (int c = 0; c < 10; c++)
            if (fb[r * stride + c] != fb[r * stride + penA + c]) differed = 1;
    TEST(differed, "'a' and 'A' are visibly different on screen");

    /* ---- 3. everything still legible ------------------------------ */
    memset(vbe_state()->back, 0, (size_t)stride * vbe_height() * sizeof(uint32_t));
    vbe_draw_text(0, 0, "the quick brown fox", 0x00FFFFFF, 1);
    vbe_swap();
    int lit = 0;
    for (int i = 0; i < stride * vbe_height(); i++) lit += (fb[i] != 0);
    TEST(lit > 100, "a mixed-case sentence produces plenty of lit pixels");

    vbe_shutdown();
    printf("=== font_case: %d checks, %d failed ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
