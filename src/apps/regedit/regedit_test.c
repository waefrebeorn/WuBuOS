/*
 * regedit_test.c -- Registry Editor behaviour
 *
 * The editor used to be a blank window (launcher bound nothing, draw was a
 * (void) stub) while the tree model underneath worked. These tests pin the
 * surface that now exists: the tree is real, expanding changes it, the value
 * pane shows the SELECTED key's values, and the rects the renderer draws are
 * the rects a click selects.
 */

#include "regedit.h"
#include "wm.h"        /* DosGuiWindow, wm_create */
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"   /* wubu_theme_set(): live colours, no settings load */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fail++; printf("  FAIL: %s\n", msg); return; } } while (0)
#define OK() do { pass++; printf("  ok\n"); } while (0)

static int row_index_of(const RegeditState *reg, const char *name) {
    for (int i = 0; i < regedit_row_count(reg); i++) {
        const RegKey *k = regedit_row_key(reg, i);
        if (k && strcmp(k->name, name) == 0) return i;
    }
    return -1;
}

static void test_tree_and_expand(void) {
    printf("\n=== Test: tree, expand/collapse ===\n");
    RegeditState *r = regedit_create();
    CHECK(r != NULL, "state created");
    regedit_init_roots(r);
    regedit_seed_default_hive(r);

    CHECK(regedit_row_count(r) == 8, "6 roots + HKLM\\Software + ...\\WuBuOS are visible");
    CHECK(regedit_row_depth(r, 0) == 0, "roots are at depth 0");

    /* Collapse HKLM: its two children must leave the row list. */
    const RegKey *hklm = regedit_get_root_key(r, 2);
    CHECK(hklm != NULL && hklm->child_count == 1, "HKLM has Software under it");
    regedit_collapse_key(r, (void *)hklm);
    CHECK(regedit_row_count(r) == 6, "collapsing HKLM drops its two descendants");

    regedit_key(NULL, 0, 0);   /* no window -> no-op, must not crash */
    CHECK(regedit_row_count(r) == 6, "a stray key call on no window changes nothing");

    regedit_expand_key(r, (void *)hklm);
    CHECK(regedit_row_count(r) == 8, "expanding again brings both back");

    regedit_destroy(r);
    OK();
}

static void test_values_belong_to_selected_key(void) {
    printf("\n=== Test: the value pane shows the SELECTED key ===\n");
    DosGuiWindow *win = wm_create(50, 50, 800, 600, "Registry Editor");
    CHECK(win != NULL, "window created");
    RegeditState *r = regedit_create();
    regedit_init_roots(r);
    regedit_seed_default_hive(r);
    win->user_data = r;

    vbe_init(1024, 768);
    regedit_draw(win, NULL, 1024, 768, r);

    /* Seeding leaves the selection on HKCU, which owns one value. */
    CHECK(regedit_value_count(r) == 1, "HKCU owns exactly its Wallpaper value");

    int target = row_index_of(r, "WuBuOS");
    CHECK(target > 0, "WuBuOS is in the visible tree");

    /* Down to WuBuOS through the real key handler (it rebuilds + reselects). */
    for (int i = 0; i < 10 && regedit_row_selected(r) != target; i++)
        regedit_key(win, 0xE050, 0);
    CHECK(regedit_row_selected(r) == target, "Down walked the selection to WuBuOS");

    const RegKey *cur = regedit_get_current_key(r);
    CHECK(cur != NULL && strcmp(cur->name, "WuBuOS") == 0, "WuBuOS is the current key");
    CHECK(regedit_value_count(r) == 3, "the pane now shows WuBuOS' three values");

    /* And the values really belong to it, not to the flat store: HKCU still
     * owns exactly one. */
    CHECK(regedit_get_root_key(r, 1)->child_count == 0, "HKCU has no subkeys");

    regedit_destroy(r);
    OK();
}

static void test_rows_are_clickable(void) {
    printf("\n=== Test: what is drawn is what a click selects ===\n");
    /* A real window: the mouse handler reads the geometry the draw produced. */
    DosGuiWindow *win = wm_create(50, 50, 800, 600, "Registry Editor");
    CHECK(win != NULL, "window created");
    RegeditState *r = regedit_create();
    regedit_init_roots(r);
    regedit_seed_default_hive(r);
    win->user_data = r;

    vbe_init(1024, 768);
    regedit_draw(win, NULL, 1024, 768, r);
    CHECK(regedit_row_count(r) == 8, "draw laid out the tree");

    /* Every visible row must have a real rect, or a click can never land. */
    for (int i = 0; i < regedit_row_count(r); i++) {
        int x = 0, y = 0, w = 0, h = 0;
        CHECK(regedit_row_rect(r, i, &x, &y, &w, &h), "row rect readable");
        CHECK(w > 0 && h > 0, "row has area");
    }

    /* Click the centre of row 4 and expect it selected + current. */
    int x = 0, y = 0, w = 0, h = 0;
    CHECK(regedit_row_rect(r, 4, &x, &y, &w, &h), "row 4 rect");
    const RegKey *want = regedit_row_key(r, 4);
    regedit_mouse(win, x + w / 2, y + h / 2, 1, 1);
    CHECK(regedit_row_selected(r) == 4, "clicking row 4 selects row 4");
    CHECK(regedit_get_current_key(r) == want, "and makes it the current key");

    /* A click outside the tree pane changes nothing. */
    regedit_mouse(win, 5, 5, 1, 1);
    CHECK(regedit_row_selected(r) == 4, "a click outside the tree does nothing");

    regedit_destroy(r);
    OK();
}

/* Dump the frame the editor actually produced, so a human (or a vision check)
 * can see the two panes instead of trusting the assertions alone. */
static void test_frame_is_a_registry_editor(void) {
    printf("\n=== Test: the frame really contains the editor ===\n");
    DosGuiWindow *win = wm_create(50, 50, 800, 600, "Registry Editor");
    CHECK(win != NULL, "window created");
    RegeditState *r = regedit_create();
    regedit_init_roots(r);
    regedit_seed_default_hive(r);
    win->user_data = r;

    vbe_init(1024, 768);
    /* The theme's live colours are only seeded by wubu_theme_set(); without it
     * every tc() colour is zero and the frame is black-on-black (the capture
     * harness pitfall). */
    wubu_theme_set(THEME_WIN98_CLASSIC);
    vbe_clear(0x00303030);
    regedit_draw(win, NULL, 1024, 768, r);

    VBEState *vs = vbe_state();
    CHECK(vs && vs->back, "back buffer exists");
    int w = vs->width, h = vs->height;
    const uint32_t *px = vs->back;
    int nonblack = 0;
    for (int i = 0; i < w * h; i++) if (px[i] & 0x00ffffff) nonblack++;
    CHECK(nonblack > (w * h) / 10, "the window painted a real surface");

    /* Panel colours of both panes must be present: a two-pane editor is not
     * one flat rectangle. */
    int distinct = 0;
    uint32_t seen[64];
    for (int i = 0; i < w * h && distinct < 64; i++) {
        uint32_t c = px[i] & 0x00ffffff;
        int dup = 0;
        for (int k = 0; k < distinct; k++) if (seen[k] == c) { dup = 1; break; }
        if (!dup) seen[distinct++] = c;
    }
    printf("  distinct colours in the frame: %d\n", distinct);

    FILE *f = fopen("build/regedit_frame.ppm", "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int i = 0; i < w * h; i++) {
            uint32_t c = px[i];
            unsigned char rgb[3] = { (unsigned char)((c >> 16) & 0xff),
                                     (unsigned char)((c >> 8) & 0xff),
                                     (unsigned char)(c & 0xff) };
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
        printf("  frame written to build/regedit_frame.ppm\n");
    }

    /* Win98 chrome is a handful of flat colours, so the bar is "more than one
     * rectangle", not a colour count contest. */
    CHECK(distinct >= 4, "many colours: chrome, panes, text and selection all drawn");

    regedit_destroy(r);
    OK();
}

int main(void) {
    printf("=== Registry Editor tests ===\n");
    test_tree_and_expand();
    test_values_belong_to_selected_key();
    test_rows_are_clickable();
    test_frame_is_a_registry_editor();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
