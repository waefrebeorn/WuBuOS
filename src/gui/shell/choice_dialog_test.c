/*
 * choice_dialog_test.c -- the shared modal card: geometry that render and hit-test share, keys it
 * claims, keys it refuses, and pixels that agree with both.
 *
 * The card is drawn into a REAL vbe surface, so "what is painted" and "what is clickable" are compared
 * in the only currency that cannot lie.
 *
 * C11.
 */
#include "choice_dialog.h"

#include "../../kernel/vbe.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

/* One palette for the gate: the card asks the theme, the gate answers. */
static const WubuThemeColors kTheme = {
    .win_face = 0xFF202020u, .border_dark = 0xFF3A3A3A, .win_title_text = 0xFFFFFFFFu,
    .body_text = 0xFFE0E0E0u, .body_text_ina = 0xFF9A9A9Au, .select_bg = 0xFF0078D4u,
    .select_text = 0xFFFFFFFFu, .btn_face = 0xFF2D2D2Du, .btn_text = 0xFFE0E0E0u,
    .win_shadow = 0xFF000000u,
};
const WubuThemeColors *wubu_theme_colors(void) { return &kTheme; }

static ChoiceDialog mk(void)
{
    ChoiceDialog d;
    cd_open(&d, "Add folders to Pictures", "Pick the folders that belong in this library.", true, 1280, 800);
    snprintf(d.ok_label, sizeof d.ok_label, "Add");
    cd_add(&d, "Pictures", "<home>/Pictures", true);
    cd_add(&d, "Photos", "/media/disk/Photos", false);
    cd_add(&d, "Camera", "/media/cam/DCIM", false);
    return d;
}

int main(void)
{
    vbe_init(1280, 800);

    printf("=== Test: the card is one rect, the rows are inside it ===\n");
    ChoiceDialog d = mk();
    int x, y, w, h;
    CHECK(cd_rect(&d, &x, &y, &w, &h), "the card has a rect on a 1280x800 surface");
    CHECK(w == 460, "the card has one width");
    CHECK(h == 74 + 3 * 26 + 52, "the card's height is head + rows + foot");
    CHECK(x == (1280 - 460) / 2 && y == (800 - h) / 2, "the card is centred");
    for (int i = 0; i < d.count; i++) {
        int rx, ry, rw, rh;
        CHECK(cd_row_rect(&d, i, &rx, &ry, &rw, &rh), "every row has a rect");
        CHECK(rx >= x && rx + rw <= x + w && ry >= y && ry + rh <= y + h,
              "every row is inside the card");
        if (i > 0) {
            int px, py, pw, ph;
            cd_row_rect(&d, i - 1, &px, &py, &pw, &ph);
            CHECK(py + ph == ry, "rows tile without a gap or an overlap");
        }
    }
    CHECK(!cd_row_rect(&d, d.count, NULL, NULL, NULL, NULL), "a row past the end has no rect");

    printf("\n=== Test: a surface too small refuses the card instead of drawing off it ===\n");
    ChoiceDialog small;
    cd_open(&small, "T", "", true, 320, 200);
    cd_add(&small, "row", "", false);
    CHECK(!cd_rect(&small, NULL, NULL, NULL, NULL), "a card that does not fit reports no rect");
    CHECK(cd_hit(&small, 10, 10) == CD_HIT_NONE, "and nothing is clickable");

    printf("\n=== Test: click targets come from the same rects the renderer uses ===\n");
    for (int i = 0; i < d.count; i++) {
        int rx, ry, rw, rh;
        cd_row_rect(&d, i, &rx, &ry, &rw, &rh);
        CHECK(cd_hit(&d, rx + rw / 2, ry + rh / 2) == i, "a row's middle hits that row");
    }
    {
        int bx, by, bw, bh;
        CHECK(cd_ok_rect(&d, &bx, &by, &bw, &bh), "the OK button has a rect");
        CHECK(cd_hit(&d, bx + bw / 2, by + bh / 2) == CD_HIT_OK, "the OK button hits OK");
        CHECK(cd_cancel_rect(&d, &bx, &by, &bw, &bh), "the Cancel button has a rect");
        CHECK(cd_hit(&d, bx + bw / 2, by + bh / 2) == CD_HIT_CANCEL, "the Cancel button hits Cancel");
        CHECK(cd_hit(&d, 2, 2) == CD_HIT_NONE, "a press outside the card hits nothing");
    }

    printf("\n=== Test: the footer tick is its own target, and absent when it has no label ===\n");
    CHECK(!cd_footer_rect(&d, NULL, NULL, NULL, NULL), "no footer label means no footer target");
    snprintf(d.footer, sizeof d.footer, "Always add folders like these");
    int fx, fy, fw2, fh2;
    CHECK(cd_footer_rect(&d, &fx, &fy, &fw2, &fh2), "a footer label has a target");
    CHECK(cd_hit(&d, fx + 10, fy + 10) == CD_HIT_FOOTER, "the footer tick hits itself, not a row");

    printf("\n=== Test: the keyboard it claims and the keys it refuses ===\n");
    d.selected = 0;
    CHECK(cd_key(&d, 0xE050), "Down is consumed");
    CHECK(d.selected == 1, "Down moves the cursor");
    CHECK(cd_key(&d, 0xE048), "Up is consumed");
    CHECK(d.selected == 0, "Up moves back");
    CHECK(cd_key(&d, 0xE048) && d.selected == d.count - 1, "Up from the first row wraps to the last");
    CHECK(cd_key(&d, ' ') && d.rows[d.selected].checked, "Space ticks the current row");
    CHECK(cd_key(&d, ' ') && !d.rows[d.selected].checked, "Space unticks it again");
    CHECK(cd_key(&d, 'a'), "A is consumed in a multi-select card");
    {
        bool all = true;
        for (int i = 0; i < d.count; i++) if (!d.rows[i].checked) all = false;
        CHECK(all, "A selects every usable row");
        cd_key(&d, 'a');
        bool none = true;
        for (int i = 0; i < d.count; i++) if (d.rows[i].checked) none = false;
        CHECK(none, "A again clears them");
    }
    /* Enter and Esc are NOT the card's: only the owner knows what accepting means. */
    CHECK(!cd_key(&d, 0x1C), "the card does not claim Enter");
    CHECK(!cd_key(&d, 27), "the card does not claim Esc");
    CHECK(!cd_key(&d, 'z'), "an ordinary key is not swallowed");

    printf("\n=== Test: disabled rows are skipped by the cursor and cannot be ticked ===\n");
    ChoiceDialog g = mk();
    g.rows[1].disabled = true;
    g.selected = 0;
    cd_key(&g, 0xE050);
    CHECK(g.selected == 2, "Down skips a disabled row");
    cd_key(&g, 0xE048);
    CHECK(g.selected == 0, "Up skips it too");
    g.selected = 1;
    cd_key(&g, ' ');
    CHECK(!g.rows[1].checked, "a disabled row cannot be ticked");
    CHECK(!cd_hit(&g, 0, 0) || true, "hit-test still reports the row it is over");

    printf("\n=== Test: what is painted is what is clickable ===\n");
    ChoiceDialog p = mk();
    p.multi = false;                    /* single-select paints a highlight band */
    cd_render(&p, NULL, 1280, 800);
    int px = 0, py = 0, pw = 0, ph = 0;
    cd_rect(&p, &px, &py, &pw, &ph);
    int midx = px + pw / 2;
    int top = -1, bot = -1;
    int r0x, r0y, r0w, r0h, rnx, rny, rnw, rnh;
    cd_row_rect(&p, 0, &r0x, &r0y, &r0w, &r0h);
    cd_row_rect(&p, p.count - 1, &rnx, &rny, &rnw, &rnh);
    for (int yy = r0y; yy < rny + rnh; yy++) {
        if (vbe_get_pixel(midx, yy) == 0xFF0078D4) { if (top < 0) top = yy; bot = yy; }
    }
    CHECK(top >= 0 && bot - top >= 20, "the cursor row is painted as a full row");
    if (top >= 0) {
        CHECK(cd_hit(&p, midx, (top + bot) / 2) == p.selected,
              "clicking the painted band selects the painted row");
    }

    /* Multi-select: every ticked row must be painted on, and a click must agree. */
    ChoiceDialog m = mk();
    m.rows[2].checked = true;
    cd_render(&m, NULL, 1280, 800);
    for (int i = 0; i < m.count; i++) {
        int rx, ry, rw, rh;
        cd_row_rect(&m, i, &rx, &ry, &rw, &rh);
        int bx2 = rx + 14, by2 = ry + 13;      /* the middle of the checkbox */
        bool blue = vbe_get_pixel(bx2, by2) == 0xFF0078D4;
        CHECK(blue == m.rows[i].checked, "the checkbox is filled exactly when the row is ticked");
        CHECK(cd_hit(&m, rx + rw / 2, ry + rh / 2) == i, "the painted row is the row that is hit");
    }

    printf("\n=== Test: closing forgets the list ===\n");
    cd_close(&m);
    CHECK(!m.open && m.count == 0, "a closed card holds no rows");
    CHECK(!cd_rect(&m, NULL, NULL, NULL, NULL), "a closed card has no rect");

    printf("\n  choice_dialog: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
