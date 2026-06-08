/*
 * startmenu_test.c — WuBuOS Start Menu Test Suite (Cell 104)
 */

#include "startmenu.h"
#include "../kernel/vbe.h"
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-50s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ── Tests ──────────────────────────────────────────────────── */

static int g_action_called = 0;
static void test_action(void) { g_action_called = 1; }

static void test_init(void) {
    TEST("startmenu_init sets count=0 and closed");
    startmenu_init();
    CHECK(startmenu_count() == 0, "count should be 0");
    CHECK(startmenu_is_open() == 0, "should be closed");
    PASS();
}

static void test_add_entry(void) {
    TEST("startmenu_add_entry adds entries");
    startmenu_init();
    int i1 = startmenu_add_entry("Programs", SM_PROGRAM, NULL);
    int i2 = startmenu_add_entry("Documents", SM_PROGRAM, NULL);
    int i3 = startmenu_add_entry("Shutdown", SM_SYSTEM, test_action);
    CHECK(i1 == 0, "first entry index 0");
    CHECK(i2 == 1, "second entry index 1");
    CHECK(i3 == 2, "third entry index 2");
    CHECK(startmenu_count() == 3, "count should be 3");
    PASS();
}

static void test_add_separator(void) {
    TEST("startmenu_add_entry with SEPARATOR type");
    startmenu_init();
    startmenu_add_entry("Programs", SM_PROGRAM, NULL);
    startmenu_add_entry(NULL, SM_SEPARATOR, NULL);
    startmenu_add_entry("Shutdown", SM_SYSTEM, NULL);
    CHECK(startmenu_count() == 3, "count includes separator");
    StartMenuEntry *e = startmenu_get_entry(1);
    CHECK(e != NULL, "separator entry exists");
    CHECK(e->type == SM_SEPARATOR, "type is SEPARATOR");
    PASS();
}

static void test_remove_entry(void) {
    TEST("startmenu_remove_entry shifts entries");
    startmenu_init();
    startmenu_add_entry("A", SM_PROGRAM, NULL);
    startmenu_add_entry("B", SM_PROGRAM, NULL);
    startmenu_add_entry("C", SM_PROGRAM, NULL);
    startmenu_remove_entry(1);
    CHECK(startmenu_count() == 2, "count should be 2 after remove");
    StartMenuEntry *e = startmenu_get_entry(1);
    CHECK(e != NULL && strcmp(e->label, "C") == 0, "C shifted to index 1");
    PASS();
}

static void test_open_close(void) {
    TEST("startmenu_open/close/toggle");
    startmenu_init();
    startmenu_open(0, 400);
    CHECK(startmenu_is_open() == 1, "should be open");
    startmenu_close();
    CHECK(startmenu_is_open() == 0, "should be closed");
    startmenu_toggle(0, 400);
    CHECK(startmenu_is_open() == 1, "toggle opens");
    startmenu_toggle(0, 400);
    CHECK(startmenu_is_open() == 0, "toggle closes");
    PASS();
}

static void test_set_hover(void) {
    TEST("startmenu_set_hover/get_hover");
    startmenu_init();
    startmenu_add_entry("A", SM_PROGRAM, NULL);
    startmenu_add_entry("B", SM_PROGRAM, NULL);
    startmenu_set_hover(1);
    CHECK(startmenu_get_hover() == 1, "hover should be 1");
    startmenu_set_hover(-1);
    CHECK(startmenu_get_hover() == -1, "hover cleared");
    PASS();
}

static void test_click_action(void) {
    TEST("startmenu_click fires action and closes menu");
    startmenu_init();
    g_action_called = 0;
    startmenu_add_entry("Shutdown", SM_SYSTEM, test_action);
    startmenu_open(0, 400);
    int result = startmenu_click(0);
    CHECK(result == 0, "click returns index 0");
    CHECK(g_action_called == 1, "action was called");
    CHECK(startmenu_is_open() == 0, "menu closed after click");
    PASS();
}

static void test_click_disabled(void) {
    TEST("click on disabled entry returns -1");
    startmenu_init();
    startmenu_add_entry("Disabled", SM_PROGRAM, NULL);
    StartMenuEntry *e = startmenu_get_entry(0);
    e->enabled = 0;
    startmenu_open(0, 400);
    int result = startmenu_click(0);
    CHECK(result == -1, "disabled click should return -1");
    PASS();
}

static void test_handle_mouse_inside(void) {
    TEST("handle_mouse inside menu returns entry index");
    startmenu_init();
    startmenu_add_entry("A", SM_PROGRAM, NULL);
    startmenu_add_entry("B", SM_PROGRAM, NULL);
    startmenu_open(0, 0);
    /* Entry 0 is at y=2 to y=26, entry 1 at y=26 to y=50 */
    int idx = startmenu_handle_mouse(50, 30);
    CHECK(idx == 1, "click at y=30 should be entry 1");
    PASS();
}

static void test_handle_mouse_outside(void) {
    TEST("handle_mouse outside menu closes and returns -1");
    startmenu_init();
    startmenu_add_entry("A", SM_PROGRAM, NULL);
    startmenu_open(0, 0);
    int idx = startmenu_handle_mouse(500, 500);
    CHECK(idx == -1, "outside click returns -1");
    CHECK(startmenu_is_open() == 0, "menu should close");
    PASS();
}

static void test_draw_no_crash(void) {
    TEST("startmenu_draw no crash (with VBE)");
    startmenu_init();
    startmenu_add_entry("Programs", SM_PROGRAM, NULL);
    startmenu_add_entry(NULL, SM_SEPARATOR, NULL);
    startmenu_add_entry("Shutdown", SM_SYSTEM, NULL);
    vbe_init(320, 200);
    startmenu_open(4, 100);
    startmenu_draw();
    startmenu_close();
    vbe_shutdown();
    PASS();
}

static void test_query_dims(void) {
    TEST("startmenu_get_width/height");
    startmenu_init();
    CHECK(startmenu_get_width() > 0, "width should be positive");
    startmenu_add_entry("A", SM_PROGRAM, NULL);
    startmenu_add_entry("B", SM_PROGRAM, NULL);
    CHECK(startmenu_get_height() > 0, "height should be positive");
    PASS();
}

static void test_is_inside(void) {
    TEST("startmenu_is_inside boundary check");
    startmenu_init();
    startmenu_add_entry("A", SM_PROGRAM, NULL);
    startmenu_open(10, 10);
    vbe_init(640, 480);
    vbe_shutdown();
    CHECK(startmenu_is_inside(10, 10) == 1, "corner should be inside");
    CHECK(startmenu_is_inside(0, 0) == 0, "outside should be 0");
    startmenu_close();
    CHECK(startmenu_is_inside(10, 10) == 0, "closed menu always 0");
    PASS();
}

/* ── Main ───────────────────────────────────────────────────── */

int main(void) {
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║  WuBuOS Start Menu Test Suite (Cell 104)               ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");

    test_init();
    test_add_entry();
    test_add_separator();
    test_remove_entry();
    test_open_close();
    test_set_hover();
    test_click_action();
    test_click_disabled();
    test_handle_mouse_inside();
    test_handle_mouse_outside();
    test_draw_no_crash();
    test_query_dims();
    test_is_inside();

    printf("\n════════════════════════════════════════════════════════\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("════════════════════════════════════════════════════════\n");

    return g_fail > 0 ? 1 : 0;
}
