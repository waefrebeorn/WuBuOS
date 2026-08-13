/*
 * wubu_a11y_test.c -- Regression tests for the accessibility cluster.
 *
 * Verifies the four GameCube-face controls resolve to the right action:
 *   A (green orb)   -> drag+move
 *   Y (yellow pill) -> click minimize, drag rotate
 *   B (red orb X)   -> click close, drag purge
 *   ◆ (corner)      -> resize
 */

#include "wubu_a11y.h"
#include "dosgui_wm.h"
#include "wubu_theme.h"
#include "wubu_trash.h"
#include <assert.h>
#include <stdio.h>

/* Window geometry used in the tests. The cluster anchors at the window's
 * top-left corner, offset by (-8,-8). With PANEL_OFF=-8 and the orb
 * centers at panel+(26,52) and +(58,52):
 *   green A center = (w.x + 26, w.y + 52)   [since panel = w.x-8, 26-8=-8.. no:  w.x-8+26 = w.x+18]
 */
#define TEST_WX    100
#define TEST_WY    100
#define TEST_W     300
#define TEST_H     200

/* Cluster orb centers (screen-space) for a window at (wx,wy) of size
 * TEST_W x TEST_H. Reference-proportional (reference_trace.svg): green A
 * LEFT of red B on the SAME row (0.363w,0.401h) / (0.579w,0.402h), yellow
 * crescent up-left (0.289w,0.216h). Radii 2.6:1 (real GC: A 16.747mm,
 * B 6.449mm). Purple resize crescents at the WINDOW's bottom corners.
 * Mirrors the geometry in wubu_a11y.c. */
static void a_center(int wx, int wy, int *cx, int *cy) {
    *cx = wx + 5 + 60;  *cy = wy + 24 + 56;   /* A = anchor + (60,56) */
}
static void b_center(int wx, int wy, int *cx, int *cy) {
    *cx = wx + 5 + 106; *cy = wy + 24 + 56;   /* B = anchor + (106,56) */
}
static void y_center(int wx, int wy, int *cx, int *cy) {
    *cx = wx + 5 + 16;  *cy = wy + 24 + 16;   /* Y = anchor + (16,16) */
}
static void p_bl(int wx, int wy, int w, int h, int *cx, int *cy) { *cx = wx + 23; *cy = wy + h - 23; }
static void p_br(int wx, int wy, int w, int h, int *cx, int *cy) { *cx = wx + w - 23; *cy = wy + h - 23; }

int main(void) {
    dosgui_wm_init(800, 600);

    /* --- Disabled: nothing is hit when the cluster is off ---------- */
    wubu_a11y_set_enabled(false);
    DosGuiWindow *w = dosgui_wm_create(TEST_WX, TEST_WY, TEST_W, TEST_H, "Test");
    assert(w && w->id >= 0);
    dosgui_wm_set_focus(w);
    int acx, acy; a_center(TEST_WX, TEST_WY, &acx, &acy);
    assert(wubu_a11y_hit(w, acx, acy) == WUBU_A11Y_NONE);

    /* --- Enabled: the four controls map correctly ----------------- */
    wubu_a11y_set_enabled(true);

    int bcx, bcy; b_center(TEST_WX, TEST_WY, &bcx, &bcy);
    int ycx, ycy; y_center(TEST_WX, TEST_WY, &ycx, &ycy);
    int pblx, pbly; p_bl(TEST_WX, TEST_WY, TEST_W, TEST_H, &pblx, &pbly);
    int pbrx, pbry; p_br(TEST_WX, TEST_WY, TEST_W, TEST_H, &pbrx, &pbry);

    assert(wubu_a11y_hit(w, acx, acy) == WUBU_A11Y_GRAB);   /* A green */
    assert(wubu_a11y_hit(w, bcx, bcy) == WUBU_A11Y_CLOSE);   /* B red */
    assert(wubu_a11y_hit(w, ycx, ycy) == WUBU_A11Y_ROTATE);  /* Y yellow */
    assert(wubu_a11y_hit(w, pblx, pbly) == WUBU_A11Y_RESIZE); /* purple BL */
    assert(wubu_a11y_hit(w, pbrx, pbry) == WUBU_A11Y_RESIZE); /* purple BR */
    /* Purple reveals only near the window's bottom corners: far = NONE. */
    assert(wubu_a11y_hit(w, pblx - 60, pbly) == WUBU_A11Y_NONE);

    /* --- Click Y (no move) -> minimize ----------------------------- */
    wubu_a11y_mouse(w, ycx, ycy, 1, 1);
    wubu_a11y_mouse(w, ycx, ycy, 1, 2);
    assert(dosgui_wm_is_minimized(w));

    /* Restore + drag-rotate Y -> w/h swap */
    dosgui_wm_restore(w);
    int w0 = w->w, h0 = w->h;
    wubu_a11y_mouse(w, ycx, ycy, 1, 1);
    wubu_a11y_mouse(w, ycx + 20, ycy + 20, 1, 0);  /* > 6px drag */
    wubu_a11y_mouse(w, ycx + 20, ycy + 20, 1, 2);
    assert(w->w == h0 && w->h == w0);  /* swapped */

    /* --- Click B (no move) -> close/destroy ----------------------- */
    w = dosgui_wm_create(200, 200, TEST_W, TEST_H, "CloseTest");
    dosgui_wm_set_focus(w);
    b_center(200, 200, &bcx, &bcy);
    wubu_a11y_mouse(w, bcx, bcy, 1, 1);
    wubu_a11y_mouse(w, bcx, bcy, 1, 2);
    assert(!w->alive);  /* destroyed */

    /* --- Click A (no move) -> MAXIMIZE (GC A = the big action) ---- */
    w = dosgui_wm_create(300, 300, TEST_W, TEST_H, "MoveTest");
    dosgui_wm_set_focus(w);
    a_center(300, 300, &acx, &acy);
    wubu_a11y_mouse(w, acx, acy, 1, 1);
    wubu_a11y_mouse(w, acx, acy, 1, 2);
    assert(w->alive);
    assert(dosgui_wm_is_maximized(w));   /* A click = maximize */
    dosgui_wm_restore(w);

    /* --- Purple resize drag (bottom-left crescent) -> w/h grow ---- */
    p_bl(300, 300, TEST_W, TEST_H, &pblx, &pbly);
    wubu_a11y_mouse(w, pblx, pbly, 1, 1);
    wubu_a11y_mouse(w, pblx + 30, pbly + 30, 1, 0);
    wubu_a11y_mouse(w, pblx + 30, pbly + 30, 1, 2);
    assert(w->w > TEST_W && w->h > TEST_H);

    /* --- Drag B orb FULL distance -> purge + destroy ------------- */
    w = dosgui_wm_create(400, 400, TEST_W, TEST_H, "PurgeTest");
    dosgui_wm_set_focus(w);
    b_center(400, 400, &bcx, &bcy);
    wubu_a11y_mouse(w, bcx, bcy, 1, 1);
    wubu_a11y_mouse(w, bcx + 35, bcy + 35, 1, 0);  /* ≥30px = FULL drag */
    wubu_a11y_mouse(w, bcx + 35, bcy + 35, 1, 2);
    assert(!w->alive);  /* destroyed after purge */

    /* --- Drag B orb PARTIAL distance -> STIM, window stays alive - */
    w = dosgui_wm_create(500, 100, TEST_W, TEST_H, "StimTest");
    dosgui_wm_set_focus(w);
    b_center(500, 100, &bcx, &bcy);
    wubu_a11y_mouse(w, bcx, bcy, 1, 1);
    wubu_a11y_mouse(w, bcx + 15, bcy + 15, 1, 0);  /* < 30px = partial */
    wubu_a11y_mouse(w, bcx + 15, bcy + 15, 1, 2);
    assert(w->alive);   /* stim is feedback, NOT a close */

    /* --- Keyboard navigation (P0 a11y) ---------------------------- */
    DosGuiWindow *kw = dosgui_wm_create(600, 100, TEST_W, TEST_H, "KeyNav");
    dosgui_wm_set_focus(kw);
    /* Arrow keys cycle focus; Enter/Space activate. Start at none. */
    assert(wubu_a11y_key(kw, 0xE04D, 0));       /* Right -> focus A (GRAB) */
    assert(wubu_a11y_key(kw, 0xE048, 0));       /* Up   -> focus B (CLOSE) */
    assert(wubu_a11y_key(kw, 0xE04B, 0));       /* Left -> focus Y (ROTATE) */
    assert(wubu_a11y_key(kw, 0xE050, 0));       /* Down -> focus purple */
    /* Enter on focused purple = no-op (resize needs a drag); window stays. */
    assert(kw->alive);
    /* Tab/other keys are NOT consumed (fall through to WM). */
    assert(!wubu_a11y_key(kw, 0x0F, 0));        /* Tab */
    /* Restore + Enter activates rotate (minimize) on the focused control. */
    dosgui_wm_restore(kw);
    for (int i = 0; i < 3; i++) wubu_a11y_key(kw, 0xE04D, 0); /* -> Y again */
    assert(wubu_a11y_key(kw, 0x1C, 0));         /* Enter activates Y */
    assert(dosgui_wm_is_minimized(kw));          /* Y click = minimize */
    dosgui_wm_destroy(kw);

    /* --- WCAG contrast audit (UXA-44): palette must meet AA -------- */
    {
        double worst = wubu_a11y_min_contrast();
        printf("  [a11y] WCAG min contrast ratio = %.2f:1\n", worst);
        assert(worst >= 3.0);   /* AA UI-component minimum (WCAG 1.4.11) */
    }

    printf("[ok] wubu_a11y: all cluster controls verified\n");

    /* --- Full-GUI WCAG audit (all themes, all rendered text/bg pairs) */
    {
        double worst = wubu_theme_contrast_audit();
        printf("  [theme] WCAG min contrast across all themes = %.2f:1\n", worst);
        assert(worst >= 3.0);   /* AA UI-component minimum (WCAG 1.4.11) */
    }

    dosgui_wm_shutdown();
    return 0;
}
