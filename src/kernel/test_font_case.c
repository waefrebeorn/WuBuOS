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

    /* vbe_draw_char is the bitmap renderer: it owns these tables and is still used directly by the chrome
     * (and as the fallback for a codepoint the face lacks). Draw through IT, not through vbe_draw_text --
     * at scale 2 vbe_draw_text now amplifies the AA face, so a check there passed with the glyphs removed. */
    memset(vbe_state()->back, 0, (size_t)stride * vbe_height() * sizeof(uint32_t));
    for (int i = 0; i < 5; i++) {
        vbe_draw_char(i * 8, 0, extra_chars[i], 0x00FFFFFF, 1);
    }
    vbe_swap();
    for (int i = 0; i < 5; i++) {
        char nm[48];
        snprintf(nm, sizeof nm, "'%c' draws ink through vbe_draw_char", extra_chars[i]);
        TEST(cell_ink(fb, i * 8, 0) > 2, nm);
    }

    /* a11y text scale: the AA face is AMPLIFIED, so an accent survives scaling (bitmap tables had no
     * Latin-1 and turned "é" into '?'). */
    vbe_set_text_scale(2);
    vbe_set_clip(0, 0, 200, 100);
    memset(vbe_state()->back, 0, (size_t)stride * vbe_height() * sizeof(uint32_t));
    vbe_draw_text(0, 0, "\xC3\xA9", 0x00FFFFFF, 1);
    vbe_swap();
    {
        int ink = 0;
        for (int yy = 0; yy < 64; yy++)
            for (int xx = 0; xx < 64; xx++)
                if (vbe_get_pixel(xx, yy) != 0) ink++;
        TEST(ink > 8, "at text scale 2 an accented glyph still draws (amplified face, not a bitmap hole)");

        /* Bilinear, not replication: with block replication every 2x2 destination block was uniform. Count
         * blocks that carry more than one value -- a replicated glyph has NONE. */
        int varied_blocks = 0;
        for (int yy = 0; yy + 1 < 24; yy += 2)
            for (int xx = 0; xx + 1 < 20; xx += 2) {
                uint32_t a = vbe_get_pixel(xx, yy), b = vbe_get_pixel(xx + 1, yy);
                uint32_t cc = vbe_get_pixel(xx, yy + 1), d = vbe_get_pixel(xx + 1, yy + 1);
                if (a != b || a != cc || a != d) varied_blocks++;
            }
        TEST(varied_blocks > 6, "2x text is interpolated, not block-replicated (no uniform 2x2 stamp)");

        /* the measure must match the renderer it feeds: boxes sized from vbe_text_width at a11y scale used to
         * assume 8 px per glyph while the face advances by its own metrics. */
        TEST(vbe_text_width("ii", 1) < vbe_text_width("WW", 1),
             "at text scale 2 the measure is proportional, like the drawing");
    }
    vbe_set_text_scale(1);

    /* ---- 1d. the BOLD face is a real second face ------------------- */
    /* Windows' type guidance: "use regular weight for most text, use Semibold for titles". We ship weight 700
     * (segoeuib.ttf) because that is the bold face in the reference tree; what must hold either way is that a
     * title looks DIFFERENT and keeps the same line box, or every layout that assumed a height breaks. */
    {
        static uint32_t reg[400 * 40];
        vbe_set_text_scale(1);
        vbe_clear(0);
        vbe_draw_text(0, 0, "Start", 0x00FFFFFF, 1);
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < 400; x++) reg[y * 400 + x] = vbe_get_pixel(x, y);
        vbe_clear(0);
        vbe_draw_text_bold(0, 0, "Start", 0x00FFFFFF, 1);
        int diff = 0, ink = 0;
        for (int y = 0; y < 40; y++)
            for (int x = 0; x < 400; x++) {
                uint32_t b = vbe_get_pixel(x, y);
                if (b != reg[y * 400 + x]) diff++;
                if (b) ink++;
            }
        TEST(ink > 20, "the bold face draws ink");
        TEST(diff > 20, "and it is VISUALLY different from the regular face (a second real weight)");
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
