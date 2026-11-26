/*
 * open_with_dialog_test.c -- the resolver: who can open this file, what does choosing do, and can a row
 * be clicked exactly where it was drawn.
 *
 * Pure model + real render against the real mime system, with HOME pointed at a temp dir so the test
 * cannot write the user's associations.
 *
 * C11.
 */
#include "open_with_dialog.h"
#include "../wubu_mime.h"
#include "../wubu_theme.h"
#include "../../kernel/vbe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_pass, g_fail;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c %s\n", msg); } \
} while (0)

/* The dialog draws through the shell's theme + vbe; this gate is not about those, so they are stubs
 * that record what was painted. */
static int g_fill_calls;
static int g_text_calls;
const WubuThemeColors *wubu_theme_colors(void)
{
    static WubuThemeColors c;
    static int init;
    if (!init) {
        c.win_face = 0xFF202020; c.win_shadow = 0xFF000000;
        c.border_dark = 0xFF3A3A3A; c.win_title_text = 0xFFFFFFFF;
        c.body_text = 0xFFDDDDDD; c.body_text_ina = 0xFF999999;
        c.select_bg = 0xFF0078D4; c.select_text = 0xFFFFFFFF;
        c.btn_face = 0xFF2D2D2D; c.btn_text = 0xFFEEEEEE;
        init = 1;
    }
    return &c;
}

int main(void)
{
    /* Never touch the real association store. */
    char home[] = "/tmp/owtestXXXXXX";
    if (!mkdtemp(home)) { printf("mkdtemp failed\n"); return 1; }
    setenv("HOME", home, 1);
    wubu_mime_init();

    printf("=== Test: candidates ===\n");
    OpenWithApp apps[OPEN_WITH_MAX_APPS];
    int n = open_with_candidates("photo.png", apps, OPEN_WITH_MAX_APPS);
    CHECK(n >= 1, "an image has at least one candidate");
    CHECK(n >= 1 && apps[0].id[0] && apps[0].name[0], "the first candidate is named");
    if (n >= 1) printf("  first candidate for photo.png: %s (%s)\n", apps[0].name, apps[0].id);

    /* No duplicate rows: a resolver that lists the same app twice is two choices with one outcome. */
    int dup = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(apps[i].id, apps[j].id) == 0) dup++;
    CHECK(dup == 0, "no candidate is listed twice");

    printf("\n=== Test: opening ===\n");
    CHECK(!open_with_open(NULL), "a NULL path does not open a dialog");
    CHECK(!open_with_open(""), "an empty path does not open a dialog");
    CHECK(!open_with_open("mystery.zzz"), "an unknown type with no handler does not open a dialog");
    CHECK(!open_with_state()->open, "the dialog stays closed when there is nothing to choose");

    CHECK(open_with_open("/tmp/notes.txt"), "a text file opens the dialog");
    const OpenWithState *st = open_with_state();
    CHECK(st->open, "the dialog is open");
    CHECK(st->count >= 1, "the dialog has candidates");
    CHECK(st->selected == 0, "the first candidate is selected");
    CHECK(strcmp(st->ext, ".txt") == 0, "the extension is recorded");
    CHECK(st->mime[0] != '\0', "the mime type is recorded");
    CHECK(st->remember, "the first choice is remembered by default");

    printf("\n=== Test: selection ===\n");
    if (st->count >= 2) {
        const char *id1 = open_with_move(1);
        CHECK(open_with_state()->selected == 1, "down moves the selection");
        CHECK(id1 && strcmp(id1, open_with_state()->apps[1].id) == 0, "move returns the new id");
        open_with_move(1);
        CHECK(open_with_state()->selected == 0 || open_with_state()->count > 2,
              "the selection wraps");
        open_with_move(-1);
        CHECK(open_with_state()->selected == open_with_state()->count - 1, "up wraps backwards");
        open_with_move(1);
        CHECK(open_with_state()->selected == 0, "back to the first");
    } else {
        CHECK(open_with_move(1) != NULL, "a single candidate still reports itself");
    }

    printf("\n=== Test: geometry, hit-test and the drawn row are one expression ===\n");
    int x, y, w, h;
    bool have = open_with_rect(1600, 900, &x, &y, &w, &h);
    CHECK(have, "the card fits a normal screen");
    if (have) {
        CHECK(x > 0 && y > 0 && x + w <= 1600 && y + h <= 900, "the card is fully on screen");
        CHECK(open_with_hit(0, 0, 1600, 900) == -1, "a click outside the card hits nothing");

        /* Every row must be hit-testable at its own centre, and every hit must be inside the card. */
        int ok_rows = 1, contiguous = 1;
        for (int i = 0; i < st->count; i++) {
            /* Ask the dialog where row i IS, then click that point. Recomputing the pitch in the test
             * would hide a drift between the drawing and the hit-test -- the exact bug this asserts. */
            int rx = 0, ry = 0, rw = 0, rh = 0;
            if (!open_with_row_rect(1600, 900, i, &rx, &ry, &rw, &rh)) { ok_rows = 0; break; }
            if (rx < x || ry < y || rx + rw > x + w || ry + rh > y + h) { ok_rows = 0; break; }
            if (open_with_hit(rx + rw / 2, ry + rh / 2, 1600, 900) != i) { ok_rows = 0; break; }
            if (i > 0) {
                int px = 0, py = 0, pw = 0, ph = 0;
                open_with_row_rect(1600, 900, i - 1, &px, &py, &pw, &ph);
                if (py + ph != ry) contiguous = 0;   /* rows tile: no dead gap, no overlap */
            }
        }
        CHECK(ok_rows, "each row is hit-testable inside its own published rect");
        CHECK(contiguous, "rows tile without gaps or overlap");
        CHECK(!open_with_row_rect(1600, 900, st->count, NULL, NULL, NULL, NULL),
              "a row past the end has no rect");

        int cb = open_with_hit(x + 20, y + h - 52 + 10, 1600, 900);
        CHECK(cb == -2, "the Remember checkbox is hit");
        int ca = open_with_hit(x + w - 16 - 88 + 10, y + h - 52 + 10, 1600, 900);
        CHECK(ca == -3, "Cancel is hit");
        int ok = open_with_hit(x + w - 16 - 88 - 8 - 88 + 10, y + h - 52 + 10, 1600, 900);
        CHECK(ok == -4, "Open is hit");

        CHECK(!open_with_rect(300, 200, &x, &y, &w, &h), "the card refuses a screen too small for it");
    }

    printf("\n=== Test: choosing applies the association it promises ===\n");
    bool opened = open_with_open("/tmp/report.png");
    CHECK(opened, "an image file opens the resolver");
    if (!opened) { printf("  cannot continue the choice test without a dialog\n"); }
    OpenWithState before = *open_with_state();
    const char *chosen = open_with_accept();
    CHECK(chosen && chosen[0], "accepting returns a handler id");
    CHECK(!open_with_state()->open, "accepting closes the dialog");
    if (chosen && chosen[0]) {
        const char *assoc = wubu_mime_get_association(before.ext, MIME_ACTION_OPEN);
        CHECK(assoc && strcmp(assoc, chosen) == 0,
              "a remembered choice is written as the extension's association");
        const char *def = wubu_mime_get_default(before.mime);
        CHECK(def && strcmp(def, chosen) == 0,
              "a remembered choice becomes the type's default handler");
    }

    printf("\n=== Test: cancelling changes nothing ===\n");
    bool opened2 = open_with_open("/tmp/notes.txt");
    CHECK(opened2, "a text file opens the resolver for the cancel test");
    OpenWithState pre = *open_with_state();
    const char *pre_assoc = wubu_mime_get_association(pre.ext, MIME_ACTION_OPEN);
    char pre_copy[128] = {0};
    if (pre_assoc) snprintf(pre_copy, sizeof pre_copy, "%s", pre_assoc);
    /* Untick "remember", then Cancel through the click path. */
    open_with_toggle_remember();
    int rc = open_with_click(pre_copy[0] ? 0 : 1, 1, 1600, 900);   /* a click that does nothing */
    (void)rc;
    int x2 = 0, y2 = 0, w2 = 0, h2 = 0;
    bool have2 = open_with_rect(1600, 900, &x2, &y2, &w2, &h2);
    CHECK(have2, "the cancel test has a card to click");
    if (have2) {
        int hit = open_with_click(x2 + w2 - 16 - 88 + 10, y2 + h2 - 52 + 10, 1600, 900);
        CHECK(hit == 2, "the Cancel button reports a cancel");
    }
    CHECK(!open_with_state()->open, "cancelling closes the dialog");
    const char *post_assoc = wubu_mime_get_association(pre.ext, MIME_ACTION_OPEN);
    char post_copy[128] = {0};
    if (post_assoc) snprintf(post_copy, sizeof post_copy, "%s", post_assoc);
    CHECK(strcmp(pre_copy, post_copy) == 0, "cancelling leaves the association untouched");
    CHECK(open_with_accept() == NULL, "accepting a closed dialog returns nothing");

    printf("\n=== Test: what is drawn is what is clickable ===\n");
    vbe_init(1600, 900);
    open_with_open("/tmp/notes.txt");
    open_with_render(NULL, 1600, 900);

    int cx = 0, cy = 0, cw = 0, ch = 0;
    open_with_rect(1600, 900, &cx, &cy, &cw, &ch);
    int midx = cx + cw / 2;
    /* Scan exactly the rows' own area, derived from the published rects -- a hand-picked bound is how
     * this probe cut its own band at 92 px and reported a hairline. */
    int r0x = 0, r0y = 0, r0w = 0, r0h = 0;
    open_with_row_rect(1600, 900, 0, &r0x, &r0y, &r0w, &r0h);
    int rnx = 0, rny = 0, rnw = 0, rnh = 0;
    open_with_row_rect(1600, 900, open_with_state()->count - 1, &rnx, &rny, &rnw, &rnh);
    int scan_from = r0y, scan_to = rny + rnh;
    int band_top = -1, band_bot = -1;
    for (int yy = scan_from; yy < scan_to; yy++) {
        if (vbe_get_pixel(midx, yy) == 0xFF0078D4) {
            if (band_top < 0) band_top = yy;
            band_bot = yy;
        }
    }
    printf("  probe: card=%d,%d %dx%d midx=%d band=%d..%d\n", cx, cy, cw, ch, midx, band_top, band_bot);
    CHECK(band_top >= 0 && band_bot > band_top, "the selected row is painted as a highlight band");
    if (band_top >= 0 && band_bot > band_top) {
        CHECK(open_with_hit(midx, (band_top + band_bot) / 2, 1600, 900)
                  == open_with_state()->selected,
              "clicking the painted band selects the row that was painted");
        CHECK((band_bot - band_top) >= 20, "the highlight is a row, not a hairline");
    }

    /* Move the selection to another row, re-render: the band must follow it. */
    if (open_with_state()->count >= 2) {
        int rx = 0, ry = 0, rw = 0, rh = 0;
        open_with_row_rect(1600, 900, 1, &rx, &ry, &rw, &rh);
        open_with_click(rx + rw / 2, ry + rh / 2, 1600, 900);
        open_with_render(NULL, 1600, 900);
        int top2 = -1, bot2 = -1;
        for (int yy = scan_from; yy < scan_to; yy++) {
            if (vbe_get_pixel(midx, yy) == 0xFF0078D4) { if (top2 < 0) top2 = yy; bot2 = yy; }
        }
        CHECK(top2 != band_top, "the highlight follows the selection");
        CHECK(top2 >= 0 && open_with_hit(midx, (top2 + bot2) / 2, 1600, 900) == 1,
              "the moved highlight is the row a click selects");
    }

    printf("\n=== Test: the accessor input routing depends on ===\n");
    open_with_close();                          /* the draw test above left it open */
    CHECK(!open_with_is_open(), "a closed dialog reports itself closed");
    CHECK(open_with_open("/tmp/notes.txt"), "reopen for the accessor test");
    CHECK(open_with_is_open(), "an open dialog reports itself open (wm_input routes on this)");
    open_with_close();

    printf("\n=== Test: memory hygiene ===\n");
    open_with_close();
    CHECK(!open_with_state()->open && open_with_state()->count == 0,
          "closing clears the whole state, not just the flag");
    CHECK(open_with_move(1) == NULL, "moving a closed dialog returns nothing");

    printf("\n  open_with: %d passed, %d failed\n", g_pass, g_fail);

    char rm[600];
    snprintf(rm, sizeof rm, "rm -rf '%s'", home);
    if (system(rm) != 0) { /* best effort */ }
    return g_fail ? 1 : 0;
}
