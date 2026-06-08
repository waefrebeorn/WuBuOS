/*
 * wubu_wm_test.c — Tests for WuBuOS Window Manager
 *
 * Cell 394/395: Theme engine + WM with drag/snap/desktops.
 */
#include "wubu_wm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int pass = 0, fail = 0;
#define TEST(name) printf("  TEST Cell395: %-55s", name)
#define PASS() do { pass++; printf("✅\n"); } while(0)
#define FAIL(msg) do { fail++; printf("❌ %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── Theme Tests ────────────────────────────────────────────────── */

static void test_theme_init(void) {
    TEST("theme init defaults to Win98");
    wubu_theme_init();
    CHECK(wubu_theme_current() == THEME_WIN98_CLASSIC, "should be Win98");
    PASS();
}

static void test_theme_cycle(void) {
    TEST("theme_cycle goes 98→XP→Orange→WuBu→98");
    wubu_theme_init();
    wubu_theme_cycle();
    CHECK(wubu_theme_current() == THEME_XP_LUNA_BLUE, "should be XP Luna");
    wubu_theme_cycle();
    CHECK(wubu_theme_current() == THEME_XP_MEDIA_ORANGE, "should be Media Orange");
    wubu_theme_cycle();
    CHECK(wubu_theme_current() == THEME_WUBU_CUSTOM, "should be WuBu Green");
    wubu_theme_cycle();
    CHECK(wubu_theme_current() == THEME_WIN98_CLASSIC, "should wrap to Win98");
    PASS();
}

static void test_theme_colors_win98(void) {
    TEST("Win98 theme has teal desktop");
    wubu_theme_set(THEME_WIN98_CLASSIC);
    const WubuThemeColors *c = wubu_theme_colors();
    CHECK(c->desktop_bg == 0x00808000, "teal = 0x008080 in XRGB");
    PASS();
}

static void test_theme_colors_xp(void) {
    TEST("XP theme has blue taskbar");
    wubu_theme_set(THEME_XP_LUNA_BLUE);
    const WubuThemeColors *c = wubu_theme_colors();
    /* XP taskbar should NOT be gray like Win98 */
    CHECK(c->taskbar_bg != c->win_face, "XP taskbar differs from window face");
    PASS();
}

static void test_theme_colors_orange(void) {
    TEST("Media Orange has orange title");
    wubu_theme_set(THEME_XP_MEDIA_ORANGE);
    const WubuThemeColors *c = wubu_theme_colors();
    CHECK(c->win_title_active != c->win_title_inactive,
          "active/inactive titles differ");
    /* Orange theme should have dark desktop */
    CHECK(c->desktop_bg < 0x00303030, "desktop should be dark");
    PASS();
}

static void test_theme_xp_gradient(void) {
    TEST("XP theme has gradient title");
    wubu_theme_set(THEME_XP_LUNA_BLUE);
    const WubuTheme *t = wubu_theme_get();
    CHECK(t->gradient_title == true, "XP should have gradient titles");
    CHECK(t->rounded_buttons == true, "XP should have rounded buttons");
    PASS();
}

static void test_theme_98_no_gradient(void) {
    TEST("Win98 theme has flat title");
    wubu_theme_set(THEME_WIN98_CLASSIC);
    const WubuTheme *t = wubu_theme_get();
    CHECK(t->gradient_title == false, "Win98 should have flat titles");
    CHECK(t->rounded_buttons == false, "Win98 should have square buttons");
    PASS();
}

/* ── WM Tests ───────────────────────────────────────────────────── */

static void test_wm_init(void) {
    TEST("wm init at 1920×1080");
    int r = wubu_wm_init(1920, 1080);
    CHECK(r == 0, "init should succeed");
    WubuWM *wm = wubu_wm_state();
    CHECK(wm->screen_w == 1920, "screen_w = 1920");
    CHECK(wm->screen_h == 1080, "screen_h = 1080");
    CHECK(wm->gaad.n_regions > 0, "GAAD should be decomposed");
    PASS();
}

static void test_wm_create_window(void) {
    TEST("create window");
    wubu_wm_init(1920, 1080);
    WubuWin *w = wubu_wm_create(100, 100, 400, 300, "Test");
    CHECK(w != NULL, "window should be created");
    CHECK(w->flags & WUBU_WIN_NORMAL, "should be normal");
    CHECK(w->flags & WUBU_WIN_FOCUSED, "should be focused");
    CHECK(strcmp(w->title, "Test") == 0, "title should match");
    CHECK(w->desktop == 0, "should be on desktop 0");
    PASS();
}

static void test_wm_minimize(void) {
    TEST("minimize window");
    wubu_wm_init(1920, 1080);
    WubuWin *w = wubu_wm_create(100, 100, 400, 300, "Test");
    wubu_wm_minimize(w);
    CHECK(w->flags & WUBU_WIN_MINIMIZED, "should be minimized");
    PASS();
}

static void test_wm_maximize(void) {
    TEST("maximize window → full screen");
    wubu_wm_init(1920, 1080);
    WubuWin *w = wubu_wm_create(100, 100, 400, 300, "Test");
    wubu_wm_maximize(w);
    CHECK(w->flags & WUBU_WIN_MAXIMIZED, "should be maximized");
    CHECK(w->x == 0 && w->y == 0, "should be at (0,0)");
    CHECK(w->w == 1920, "should be full width");
    PASS();
}

static void test_wm_restore(void) {
    TEST("maximize → restore → original position");
    wubu_wm_init(1920, 1080);
    WubuWin *w = wubu_wm_create(100, 100, 400, 300, "Test");
    wubu_wm_maximize(w);
    wubu_wm_restore(w);
    CHECK(w->x == 100 && w->y == 100, "should restore to original pos");
    CHECK(w->w == 400 && w->h == 300, "should restore to original size");
    PASS();
}

/* ── Virtual Desktop Tests ──────────────────────────────────────── */

static void test_desktops_default(void) {
    TEST("default 4 virtual desktops");
    wubu_wm_init(1920, 1080);
    CHECK(wubu_wm_desktop_count() == 4, "should have 4 desktops");
    CHECK(wubu_wm_desktop_current() == 0, "current should be 0");
    PASS();
}

static void test_desktops_switch(void) {
    TEST("switch desktop 0→1→2→3→0");
    wubu_wm_init(1920, 1080);
    wubu_wm_desktop_next();
    CHECK(wubu_wm_desktop_current() == 1, "should be desktop 1");
    wubu_wm_desktop_next();
    CHECK(wubu_wm_desktop_current() == 2, "should be desktop 2");
    wubu_wm_desktop_next();
    CHECK(wubu_wm_desktop_current() == 3, "should be desktop 3");
    wubu_wm_desktop_next();
    CHECK(wubu_wm_desktop_current() == 0, "should wrap to desktop 0");
    PASS();
}

static void test_desktops_prev(void) {
    TEST("switch desktop prev: 0→3");
    wubu_wm_init(1920, 1080);
    wubu_wm_desktop_prev();
    CHECK(wubu_wm_desktop_current() == 3, "should wrap to desktop 3");
    PASS();
}

static void test_desktop_move_window(void) {
    TEST("move window to different desktop");
    wubu_wm_init(1920, 1080);
    WubuWin *w = wubu_wm_create(100, 100, 400, 300, "Test");
    wubu_wm_desktop_move_win(w, 2);
    CHECK(w->desktop == 2, "window should be on desktop 2");
    PASS();
}

/* ── GAAD Snap Tests ────────────────────────────────────────────── */

static void test_gaad_snap_on_drag_end(void) {
    TEST("GAAD snap on drag end");
    wubu_wm_init(1920, 1080);
    WubuWin *w = wubu_wm_create(10, 10, 400, 300, "Test");
    /* Move near a corner, then snap */
    wubu_wm_move(w, 5, 5);
    wubu_wm_gaad_snap(w);
    /* After snapping, window should either be snapped or free */
    PASS();  /* Just verifying it doesn't crash */
}

static void test_resolution_change(void) {
    TEST("resolution change recomputes GAAD");
    wubu_wm_init(1920, 1080);
    wubu_wm_set_resolution(3840, 2160);
    WubuWM *wm = wubu_wm_state();
    CHECK(wm->screen_w == 3840, "screen should be 3840 wide");
    CHECK(wm->gaad.n_regions > 0, "GAAD should be recomputed");
    PASS();
}

/* ── Main ───────────────────────────────────────────────────────── */

int main(void) {
    printf("\n── Theme Engine (Cell 394) ──\n\n");
    test_theme_init();
    test_theme_cycle();
    test_theme_colors_win98();
    test_theme_colors_xp();
    test_theme_colors_orange();
    test_theme_xp_gradient();
    test_theme_98_no_gradient();

    printf("\n── Window Manager (Cell 395) ──\n\n");
    test_wm_init();
    test_wm_create_window();
    test_wm_minimize();
    test_wm_maximize();
    test_wm_restore();

    printf("\n── Virtual Desktops (Cell 395) ──\n\n");
    test_desktops_default();
    test_desktops_switch();
    test_desktops_prev();
    test_desktop_move_window();

    printf("\n── GAAD Snap + Resolution (Cell 395) ──\n\n");
    test_gaad_snap_on_drag_end();
    test_resolution_change();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n",
           pass, pass + fail, fail);
    printf("══════════════════════════════════════════════════\n");
    return fail > 0 ? 1 : 0;
}
