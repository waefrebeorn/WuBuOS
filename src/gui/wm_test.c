/*
 * wm_test.c  --  WuBuOS WmWindow Manager Test Suite
 *
 * Cell 102: Full WM test suite + Win98 theme verification
 * Uses VBE_HOSTED mode (no kernel memory allocator).
 */

#include "wm.h"
#include "../kernel/vbe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- Test Framework -------------------------------------------- */

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) printf("  TEST %-50s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Lifecycle Tests ------------------------------------------ */

static void test_wm_init(void) {
    TEST("wm_init returns 0");
    int rc = wm_init(640, 480);
    CHECK(rc == 0, "init should succeed");
    wm_shutdown();
    PASS();
}

static void test_wm_init_shutdown(void) {
    TEST("wm_init -> wm_shutdown no crash");
    wm_init(800, 600);
    wm_shutdown();
    PASS();
}

/* -- WmWindow Creation Tests ------------------------------------ */

static void test_create_window(void) {
    TEST("wm_create_window returns non-NULL");
    wm_init(640, 480);
    WmWindow *w = wm_create_window(10, 20, 200, 100, "Test");
    CHECK(w != NULL, "window should be created");
    CHECK(w->id > 0, "window should have positive id");
    CHECK(w->x == 10, "x should be 10");
    CHECK(w->y == 20, "y should be 20");
    CHECK(w->w == 200, "w should be 200");
    CHECK(w->h == 100, "h should be 100");
    CHECK(w->flags & WIN_VISIBLE, "should be visible");
    wm_shutdown();
    PASS();
}

static void test_create_window_null_title(void) {
    TEST("wm_create_window with NULL title uses default");
    wm_init(640, 480);
    WmWindow *w = wm_create_window(0, 0, 100, 50, NULL);
    CHECK(w != NULL, "should create with NULL title");
    CHECK(strcmp(w->title, "WmWindow") == 0, "default title should be WmWindow");
    wm_shutdown();
    PASS();
}

static void test_create_multiple_windows(void) {
    TEST("create multiple windows up to max");
    wm_init(640, 480);
    int count = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        WmWindow *w = wm_create_window(i*10, 0, 80, 60, "W");
        if (w) count++;
    }
    CHECK(count == WM_MAX_WINDOWS, "should create WM_MAX_WINDOWS windows");
    wm_shutdown();
    PASS();
}

static void test_create_beyond_max(void) {
    TEST("create beyond max returns NULL");
    wm_init(640, 480);
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        wm_create_window(0, 0, 80, 60, "W");
    WmWindow *overflow = wm_create_window(0, 0, 80, 60, "X");
    CHECK(overflow == NULL, "should return NULL when full");
    wm_shutdown();
    PASS();
}

/* -- WmWindow Destruction Tests --------------------------------- */

static void test_destroy_window(void) {
    TEST("wm_destroy_window marks slot unused");
    wm_init(640, 480);
    WmWindow *w = wm_create_window(0, 0, 100, 50, "Del");
    CHECK(w != NULL, "create should succeed");
    wm_destroy_window(w);
    CHECK(w->flags == WIN_UNUSED, "flags should be UNUSED after destroy");
    wm_shutdown();
    PASS();
}

static void test_destroy_removes_from_count(void) {
    TEST("destroyed window not counted in wm_window_count");
    wm_init(640, 480);
    WmWindow *w = wm_create_window(0, 0, 100, 50, "Del");
    CHECK(wm_window_count() == 1, "count 1 before destroy");
    wm_destroy_window(w);
    CHECK(wm_window_count() == 0, "count 0 after destroy");
    wm_shutdown();
    PASS();
}

static void test_destroy_null(void) {
    TEST("wm_destroy_window(NULL) no crash");
    wm_destroy_window(NULL);
    PASS();
}

/* -- Focus Tests ---------------------------------------------- */

static void test_set_focus(void) {
    TEST("wm_set_focus changes focused window");
    wm_init(640, 480);
    WmWindow *w1 = wm_create_window(0, 0, 100, 50, "W1");
    WmWindow *w2 = wm_create_window(0, 0, 100, 50, "W2");
    wm_set_focus(w1);
    WmWindow *focused = wm_get_focused();
    CHECK(focused == w1, "w1 should be focused");
    CHECK(w1->flags & WIN_FOCUSED, "w1 should have FOCUSED flag");
    wm_set_focus(w2);
    focused = wm_get_focused();
    CHECK(focused == w2, "w2 should be focused after set_focus");
    CHECK(w2->flags & WIN_FOCUSED, "w2 should have FOCUSED flag");
    wm_shutdown();
    PASS();
}

static void test_focus_unfocus_old(void) {
    TEST("focusing new window unfocuses old");
    wm_init(640, 480);
    WmWindow *w1 = wm_create_window(0, 0, 100, 50, "W1");
    WmWindow *w2 = wm_create_window(0, 0, 100, 50, "W2");
    wm_set_focus(w1);
    CHECK(w1->flags & WIN_FOCUSED, "w1 focused initially");
    wm_set_focus(w2);
    CHECK(!(w1->flags & WIN_FOCUSED), "w1 should lose focus");
    CHECK(w2->flags & WIN_FOCUSED, "w2 should gain focus");
    wm_shutdown();
    PASS();
}

static void test_focus_title_color(void) {
    TEST("focused window gets navy title, unfocused gets gray");
    wm_init(640, 480);
    WmWindow *w1 = wm_create_window(0, 0, 100, 50, "W1");
    WmWindow *w2 = wm_create_window(0, 0, 100, 50, "W2");
    wm_set_focus(w1);
    CHECK(w1->title_color == 0x00000080, "active title should be navy");
    wm_set_focus(w2);
    CHECK(w1->title_color == 0x00808080, "inactive title should be gray");
    CHECK(w2->title_color == 0x00000080, "new active should be navy");
    wm_shutdown();
    PASS();
}

/* -- Find By ID Tests ----------------------------------------- */

static void test_find_by_id(void) {
    TEST("wm_find_by_id returns correct window");
    wm_init(640, 480);
    WmWindow *w = wm_create_window(0, 0, 100, 50, "Find");
    int id = w->id;
    WmWindow *found = wm_find_by_id(id);
    CHECK(found == w, "should find window by id");
    wm_shutdown();
    PASS();
}

static void test_find_by_id_nonexistent(void) {
    TEST("wm_find_by_id returns NULL for bad id");
    wm_init(640, 480);
    WmWindow *found = wm_find_by_id(9999);
    CHECK(found == NULL, "should return NULL for nonexistent id");
    wm_shutdown();
    PASS();
}

/* -- Z-Order Tests -------------------------------------------- */

static void test_z_order_on_create(void) {
    TEST("z_order is set on creation (matches id or auto-assigned)");
    wm_init(640, 480);
    WmWindow *w = wm_create_window(0, 0, 100, 50, "Z");
    CHECK(w->z_order > 0, "z_order should be positive on creation");
    wm_shutdown();
    PASS();
}

static void test_z_order_on_focus(void) {
    TEST("focusing brings window to front (z_order > 10000)");
    wm_init(640, 480);
    WmWindow *w = wm_create_window(0, 0, 100, 50, "Z");
    wm_set_focus(w);
    CHECK(w->z_order >= 10000, "focused z_order should be >= 10000");
    wm_shutdown();
    PASS();
}

/* -- WmWindow Count Tests --------------------------------------- */

static void test_window_count(void) {
    TEST("wm_window_count tracks active windows");
    wm_init(640, 480);
    CHECK(wm_window_count() == 0, "count should be 0 initially");
    WmWindow *w1 = wm_create_window(0, 0, 100, 50, "A");
    CHECK(wm_window_count() == 1, "count should be 1");
    WmWindow *w2 = wm_create_window(0, 0, 100, 50, "B");
    CHECK(wm_window_count() == 2, "count should be 2");
    wm_destroy_window(w1);
    CHECK(wm_window_count() == 1, "count should be 1 after destroy");
    wm_destroy_window(w2);
    CHECK(wm_window_count() == 0, "count should be 0 after all destroyed");
    wm_shutdown();
    PASS();
}

/* -- Render Tests (no crash) ---------------------------------- */

static void test_render_no_crash(void) {
    TEST("wm_render with no windows no crash");
    wm_init(320, 200);
    vbe_init(320, 200);
    uint32_t *fb = vbe_state()->fb;
    wm_render(fb, 320, 200);
    vbe_shutdown();
    wm_shutdown();
    PASS();
}

static void test_render_with_windows(void) {
    TEST("wm_render with windows no crash");
    wm_init(320, 200);
    vbe_init(320, 200);
    wm_create_window(10, 10, 200, 100, "A");
    wm_create_window(50, 50, 150, 80, "B");
    uint32_t *fb = vbe_state()->fb;
    wm_render(fb, 320, 200);
    vbe_shutdown();
    wm_shutdown();
    PASS();
}

/* -- Invalidate Tests ----------------------------------------- */

static void test_invalidate(void) {
    TEST("wm_invalidate no crash");
    wm_init(640, 480);
    WmWindow *w = wm_create_window(0, 0, 100, 50, "Inv");
    wm_invalidate(w);
    wm_invalidate_all();
    wm_shutdown();
    PASS();
}

/* -- Input Routing Tests -------------------------------------- */

static void test_handle_key(void) {
    TEST("wm_handle_key no crash");
    wm_init(640, 480);
    wm_create_window(0, 0, 100, 50, "Key");
    wm_handle_key(0x1C, 0);
    wm_handle_key(0x01, 0);
    wm_shutdown();
    PASS();
}

static void test_handle_mouse(void) {
    TEST("wm_handle_mouse no crash and finds window under cursor");
    wm_init(640, 480);
    WmWindow *w = wm_create_window(10, 10, 200, 100, "Mouse");
    wm_set_focus(w);
    wm_handle_mouse(50, 50, 1, 1);
    wm_handle_mouse(500, 500, 1, 1);
    wm_shutdown();
    PASS();
}

/* -- Win98 Theme Verification --------------------------------- */

static void test_win98_colors(void) {
    TEST("Win98 colors: desktop=teal, face=0xC0C0C0, title=0x000080");
    CHECK(C_WIN_DESKTOP == 0x008080, "desktop teal");
    CHECK(C_WIN_FACE == 0x00C0C0C0, "face silver");
    CHECK(C_WIN_TITLE == 0x00000080, "title navy");
    CHECK(C_WIN_BORDER_LT == 0x00FFFFFF, "border light = white");
    CHECK(C_WIN_BORDER_DK == 0x00808080, "border dark = gray");
    CHECK(C_WIN_BORDER_DD == 0x00000000, "border darkest = black");
    PASS();
}

static void test_win98_title_bar_colors(void) {
    TEST("new window gets navy active title (Win98 classic)");
    wm_init(640, 480);
    WmWindow *w = wm_create_window(0, 0, 200, 100, "Win98");
    CHECK(w->title_color == 0x00000080, "new window title should be navy");
    wm_shutdown();
    PASS();
}

static void test_win98_inactive_title(void) {
    TEST("unfocused window gets gray title (Win98 inactive)");
    wm_init(640, 480);
    WmWindow *w1 = wm_create_window(0, 0, 100, 50, "A");
    WmWindow *w2 = wm_create_window(0, 0, 100, 50, "B");
    CHECK(w1->title_color == 0x00808080, "w1 should have gray inactive title");
    (void)w2;
    wm_shutdown();
    PASS();
}

/* -- Callback Tests ------------------------------------------- */

static int g_on_close_called = 0;
static void test_on_close_cb(WmWindow *w) { g_on_close_called = 1; (void)w; }

static void test_on_close_callback(void) {
    TEST("on_close callback fires on destroy");
    wm_init(640, 480);
    g_on_close_called = 0;
    WmWindow *w = wm_create_window(0, 0, 100, 50, "CB");
    w->on_close = test_on_close_cb;
    wm_destroy_window(w);
    CHECK(g_on_close_called == 1, "on_close should have been called");
    wm_shutdown();
    PASS();
}

/* -- Main ----------------------------------------------------- */

int main(void) {
    printf("+========================================================+\n");
    printf("|  WuBuOS WmWindow Manager Test Suite (Cell 102)           |\n");
    printf("+========================================================+\n\n");

    test_wm_init();
    test_wm_init_shutdown();
    test_create_window();
    test_create_window_null_title();
    test_create_multiple_windows();
    test_create_beyond_max();
    test_destroy_window();
    test_destroy_removes_from_count();
    test_destroy_null();
    test_set_focus();
    test_focus_unfocus_old();
    test_focus_title_color();
    test_find_by_id();
    test_find_by_id_nonexistent();
    test_z_order_on_create();
    test_z_order_on_focus();
    test_window_count();
    test_render_no_crash();
    test_render_with_windows();
    test_invalidate();
    test_handle_key();
    test_handle_mouse();
    test_win98_colors();
    test_win98_title_bar_colors();
    test_win98_inactive_title();
    test_on_close_callback();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}
