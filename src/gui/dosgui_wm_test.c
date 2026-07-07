/*
 * dosgui_wm_test.c  --  WuBuOS DosGui Window Manager Test Suite
 *
 * Cell 400: Fable Windowing Agent test suite.
 * Tests window creation, z-order, drag, focus, taskbar, icons.
 */

#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) printf("  TEST %-55s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Lifecycle Tests ------------------------------------------- */

static void test_init(void) {
    TEST("dosgui_wm_init returns 0");
    int rc = dosgui_wm_init(1024, 768);
    CHECK(rc == 0, "init should succeed");
    dosgui_wm_shutdown();
    PASS();
}

static void test_init_shutdown_no_crash(void) {
    TEST("init + shutdown no crash");
    dosgui_wm_init(800, 600);
    dosgui_wm_shutdown();
    PASS();
}

/* -- Window Management Tests ----------------------------------- */

static void test_create_window(void) {
    TEST("dosgui_wm_create returns non-NULL");
    dosgui_wm_init(1024, 768);
    DosGuiWindow *w = dosgui_wm_create(10, 20, 200, 150, "Test");
    CHECK(w != NULL, "window should be created");
    dosgui_wm_shutdown();
    PASS();
}

static void test_create_multiple(void) {
    TEST("create 10 windows");
    dosgui_wm_init(1024, 768);
    int ok = 1;
    for (int i = 0; i < 10; i++) {
        char title[32];
        snprintf(title, sizeof(title), "Win %d", i);
        DosGuiWindow *w = dosgui_wm_create(10 + i * 30, 20 + i * 20, 200, 150, title);
        if (!w) { ok = 0; break; }
    }
    CHECK(ok, "all 10 windows created");
    CHECK(dosgui_wm_window_count() == 10, "count == 10");
    dosgui_wm_shutdown();
    PASS();
}

static void test_window_count(void) {
    TEST("window count tracks creates and destroys");
    dosgui_wm_init(1024, 768);
    DosGuiWindow *w1 = dosgui_wm_create(10, 20, 200, 150, "W1");
    DosGuiWindow *w2 = dosgui_wm_create(50, 60, 200, 150, "W2");
    CHECK(dosgui_wm_window_count() == 2, "count == 2 after creates");
    dosgui_wm_destroy(w1);
    CHECK(dosgui_wm_window_count() == 1, "count == 1 after destroy");
    dosgui_wm_destroy(w2);
    CHECK(dosgui_wm_window_count() == 0, "count == 0 after all destroyed");
    dosgui_wm_shutdown();
    PASS();
}

static void test_focus(void) {
    TEST("focus tracks last created window");
    dosgui_wm_init(1024, 768);
    DosGuiWindow *w1 = dosgui_wm_create(10, 20, 200, 150, "W1");
    DosGuiWindow *w2 = dosgui_wm_create(50, 60, 200, 150, "W2");
    DosGuiWindow *f = dosgui_wm_get_focused();
    CHECK(f == w2, "focused should be w2 (last created)");
    dosgui_wm_set_focus(w1);
    f = dosgui_wm_get_focused();
    CHECK(f == w1, "after set_focus, focused should be w1");
    dosgui_wm_shutdown();
    PASS();
}

static void test_find_by_id(void) {
    TEST("dosgui_wm_find_by_id returns correct window");
    dosgui_wm_init(1024, 768);
    DosGuiWindow *w = dosgui_wm_create(10, 20, 200, 150, "Test");
    int id = w->id;
    DosGuiWindow *found = dosgui_wm_find_by_id(id);
    CHECK(found == w, "find_by_id should return same pointer");
    DosGuiWindow *bad = dosgui_wm_find_by_id(9999);
    CHECK(bad == NULL, "find_by_id with bad id returns NULL");
    dosgui_wm_shutdown();
    PASS();
}

/* -- Input Tests ----------------------------------------------- */

static void test_mouse_click_no_crash(void) {
    TEST("mouse click on empty desktop no crash");
    dosgui_wm_init(1024, 768);
    dosgui_wm_handle_mouse(50, 50, 1, 1); /* down */
    dosgui_wm_handle_mouse(50, 50, 0, 2); /* up */
    PASS();
    dosgui_wm_shutdown();
}

static void test_mouse_drag_window(void) {
    TEST("mouse drag moves window");
    dosgui_wm_init(1024, 768);
    DosGuiWindow *w = dosgui_wm_create(100, 100, 200, 150, "DragTest");
    int orig_x = w->x, orig_y = w->y;
    /* Click on title bar */
    dosgui_wm_handle_mouse(110, 105, 1, 1);
    /* Drag */
    dosgui_wm_handle_mouse(150, 145, 1, 0);
    CHECK(w->x != orig_x || w->y != orig_y, "window moved");
    dosgui_wm_handle_mouse(150, 145, 0, 2);
    PASS();
    dosgui_wm_shutdown();
}

/* -- Taskbar Tests --------------------------------------------- */

static void test_taskbar_height(void) {
    TEST("taskbar height == 28");
    CHECK(dosgui_taskbar_height() == 28, "should be 28");
    PASS();
}

static void test_render_no_crash(void) {
    TEST("render desktop with windows no crash");
    dosgui_wm_init(1024, 768);
    dosgui_wm_create(60, 60, 300, 200, "Welcome");
    dosgui_wm_create(400, 100, 250, 180, "Notepad");
    uint32_t *fb = (uint32_t *)calloc(1024 * 768, 4);
    dosgui_wm_render_desktop(fb, 1024, 768);
    free(fb);
    PASS();
    dosgui_wm_shutdown();
}

/* -- Icon Tests ------------------------------------------------ */

static void test_add_icon(void) {
    TEST("dosgui_icon_add returns valid index");
    dosgui_wm_init(1024, 768);
    int idx = dosgui_icon_add("My Computer", 0, 0, NULL);
    CHECK(idx == 0, "first icon index == 0");
    int idx2 = dosgui_icon_add("Temple REPL", 0, 1, NULL);
    CHECK(idx2 == 1, "second icon index == 1");
    dosgui_wm_shutdown();
    PASS();
}

static void test_icon_hit_test(void) {
    TEST("icon hit test finds correct icon");
    dosgui_wm_init(1024, 768);
    dosgui_icon_add("Icon0", 0, 0, NULL);
    dosgui_icon_add("Icon1", 1, 0, NULL);
    /* Icon 0 at (20, 20), size 32 */
    int hit = dosgui_icon_hit_test(30, 30);
    CHECK(hit == 0, "should hit icon 0");
    /* Icon 1 at (20 + 1*(32+8), 20) = (60, 20) */
    hit = dosgui_icon_hit_test(70, 30);
    CHECK(hit == 1, "should hit icon 1");
    /* Miss */
    hit = dosgui_icon_hit_test(500, 500);
    CHECK(hit == -1, "should miss");
    dosgui_wm_shutdown();
    PASS();
}

/* -- Fable Sauce Tests ----------------------------------------- */

static void test_fable_font(void) {
    TEST("Fable 8x8 font has 64 glyphs");
    /* Just verify the font data is accessible */
    const uint8_t *glyph = vbe_font_8x8[0]; /* space */
    CHECK(glyph[0] == 0, "space glyph first byte == 0");
    glyph = vbe_font_8x8[1]; /* '!' */
    CHECK(glyph[0] == 0x20, "! glyph first byte");
    glyph = vbe_font_8x8[33]; /* 'A' */
    CHECK(glyph[0] == 0x70, "A glyph first byte");
    PASS();
}

static void test_fable_text_width(void) {
    TEST("vbe_text_width calculates correctly");
    CHECK(vbe_text_width("ABC", 1) == 18, "3 chars * 6 = 18");
    CHECK(vbe_text_width("Hello", 2) == 60, "5 chars * 6 * 2 = 60");
    CHECK(vbe_text_width("", 1) == 0, "empty string = 0");
    PASS();
}

static void test_fable_primitives_exist(void) {
    TEST("Fable primitives compile and link");
    /* Just call them to verify they exist */
    vbe_init(320, 200);
    vbe_vgradient(0, 0, 100, 50, 0xFF0000, 0x0000FF);
    vbe_hgradient(0, 0, 100, 50, 0x00FF00, 0xFF00FF);
    vbe_fill_circle(50, 50, 10, 0xFFFFFF);
    vbe_shade_rect(10, 10, 80, 40);
    vbe_draw_cursor(100, 100);
    vbe_title_bar(0, 0, 200, 20, 1);
    vbe_close_box(180, 4, 1);
    vbe_draw_text(10, 10, "FABLE", 0xFFFFFF, 1);
    vbe_draw_char(10, 30, 'X', 0xFF0000, 2);
    vbe_shutdown();
    PASS();
}

/* -- Icon Persistence Tests (Stream 2) ------------------------- */

static void test_icon_layout_persist_restore(void) {
    TEST("icon layout persists across save/restore");
    /* Isolate settings to a temp dir. */
    setenv("XDG_CONFIG_HOME", "/tmp/wubu_icontest", 1);
    wubu_settings_init();
    dosgui_wm_init(1024, 768);

    int a = dosgui_icon_add("PersistA", 0, 0, NULL);
    int b = dosgui_icon_add("PersistB", 1, 0, NULL);
    CHECK(a >= 0 && b >= 0, "two icons added");

    /* Move B to a different grid cell (simulating a drag). */
    DosGuiIcon *ic = dosgui_icon_get(b);
    CHECK(ic != NULL, "icon_get returns icon B");
    ic->grid_x = 3; ic->grid_y = 5;
    ic->x = 20 + 3 * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    ic->y = 20 + 5 * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);

    dosgui_wm_save_icon_layout();          /* persist */
    const WubuSettings *s = wubu_settings_get();
    CHECK(s->theme.icon_layout_count >= 2, "settings recorded >= 2 layout entries");

    /* Simulate a fresh boot: new WM, re-add icons at default positions. */
    dosgui_wm_shutdown();
    wubu_settings_init();                  /* reload from disk */
    dosgui_wm_init(1024, 768);
    dosgui_icon_add("PersistA", 0, 0, NULL);
    dosgui_icon_add("PersistB", 1, 0, NULL);
    dosgui_wm_restore_icon_layout();       /* restore persisted positions */

    DosGuiIcon *rb = dosgui_icon_get(1);
    CHECK(rb != NULL, "restored icon B present");
    CHECK(rb->grid_x == 3 && rb->grid_y == 5, "icon B restored to persisted grid (3,5)");
    CHECK(rb->x == 20 + 3 * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP), "icon B x recomputed from grid");

    dosgui_wm_shutdown();
    PASS();
}

static void test_icon_get_bounds(void) {
    TEST("icon_get returns NULL out of bounds");
    dosgui_wm_init(800, 600);
    dosgui_icon_add("X", 0, 0, NULL);
    CHECK(dosgui_icon_get(0) != NULL, "icon_get(0) valid");
    CHECK(dosgui_icon_get(99) == NULL, "icon_get(99) NULL");
    CHECK(dosgui_icon_get(-1) == NULL, "icon_get(-1) NULL");
    dosgui_wm_shutdown();
    PASS();
}

/* -- Main ------------------------------------------------------ */

int main(void) {
    printf("+========================================================+\n");
    printf("|  WuBuOS DosGui WM Test Suite (Cell 400)                |\n");
    printf("|  Fable Windowing Agent — Mythos Fable sauce             |\n");
    printf("+========================================================+\n\n");

    test_init();
    test_init_shutdown_no_crash();
    test_create_window();
    test_create_multiple();
    test_window_count();
    test_focus();
    test_find_by_id();
    test_mouse_click_no_crash();
    test_mouse_drag_window();
    test_taskbar_height();
    test_render_no_crash();
    test_add_icon();
    test_icon_hit_test();
    test_icon_layout_persist_restore();
    test_icon_get_bounds();
    test_fable_font();
    test_fable_text_width();
    test_fable_primitives_exist();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}
