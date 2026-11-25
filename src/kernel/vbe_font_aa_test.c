/* Text quality is a RENDERER property, so it is asserted through the renderer's public
 * API -- vbe_draw_text() -- and not through a parallel text module. An earlier draft had
 * a second implementation in src/gui/fonts/wubu_font_draw.c; two copies of one rule is
 * how corners, icons and now glyphs would drift apart. */
#include <stdio.h>
#include <stdint.h>

#include "vbe.h"
#include "../gui/fonts/wubu_font_text.h"

static int fails;

#define CHECK(cond, msg) do { \
    if (cond) printf("  TEST %s \xE2\x9C\x85\n", msg); \
    else { printf("  TEST %s \xE2\x9D\x8C\n", msg); fails++; } \
} while (0)

static int distinct_levels(int x0, int y0, int x1, int y1) {
    int seen[256] = {0};
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) seen[vbe_get_pixel(x, y) & 0xFF] = 1;
    int n = 0;
    for (int i = 0; i < 256; i++) n += seen[i];
    return n;
}

static void extent(int *top, int *bottom) {
    *top = -1; *bottom = -1;
    for (int y = 0; y < 48; y++)
        for (int x = 0; x < 80; x++)
            if (vbe_get_pixel(x, y) != 0) { if (*top < 0) *top = y; if (y > *bottom) *bottom = y; }
}

int main(void) {
    printf("== font AA gate (through vbe_draw_text) ==\n");
    vbe_init(240, 48);

    vbe_fill_rect(0, 0, 240, 48, 0);
    CHECK(vbe_text_width("H", 1) > 0, "H has a width");
    CHECK(vbe_text_width("H", 1) < vbe_text_width("HH", 1), "advance accumulates");

    vbe_draw_text(4, 4, "H", 0xFFFFFF, 1);
    int lv = distinct_levels(0, 0, 40, 40);
    CHECK(lv > 4, "H is antialiased (more than 4 intensity levels)");
    CHECK(lv < 200, "H is not a solid block");

    int top, bottom;
    extent(&top, &bottom);
    CHECK(bottom == 4 + vbe_text_baseline_off() - 1, "H sits exactly on the baseline");

    vbe_fill_rect(0, 0, 240, 48, 0);
    vbe_draw_text(4, 4, "g", 0xFFFFFF, 1);
    extent(&top, &bottom);
    CHECK(bottom > 4 + vbe_text_baseline_off() - 1, "g descends below the baseline");

    /* One character the face lacks must not drag the whole line back to the bitmap. */
    vbe_fill_rect(0, 0, 240, 48, 0);
    vbe_draw_text(4, 4, "H\x01H", 0xFFFFFF, 1);
    CHECK(distinct_levels(0, 0, 60, 40) > 4, "an unknown glyph does not make the line blocky");

    printf("\n%d check(s) failed\n", fails);
    return fails ? 1 : 0;
}
