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

    /* ---- 2. drawing uses the right table -------------------------- */
    /* "a A": advance is 6px per glyph, so 'a' sits at x=0, the space at 6,
     * and 'A' at x=12.  The gap keeps the two cells from overwriting each
     * other (glyphs are 8 wide, advance is 6). */
    memset(vbe_state()->back, 0, (size_t)stride * vbe_height() * sizeof(uint32_t));
    vbe_draw_text(0, 0, "a A", 0x00FFFFFF, 1);
    vbe_swap();

    TEST(cell_matches(fb, 0, 0, vbe_font_lower_8x8['a' - 'a'], "a"),
          "'a' renders the LOWER-CASE glyph");
    TEST(cell_matches(fb, 12, 0, vbe_font_8x8['A' - 32], "A"),
          "'A' renders the UPPER-CASE glyph");

    /* A lower-case render must not silently equal the upper-case one. */
    int differed = 0;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if (fb[r * stride + c] != fb[r * stride + 12 + c]) differed = 1;
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
