/*
 * dosgui_startmenu_test.c  --  WuBuOS DosGui Start Menu Test Suite (Cell 402)
 */

#include "dosgui_startmenu.h"
#include "../kernel/vbe.h"
#include <stdio.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-50s", name); g_total++;
#define PASS() do { printf("\u2705\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("\u274C %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static void test_init(void) {
    TEST("dosgui_startmenu_init initializes state");
    dosgui_startmenu_init();
    CHECK(dosgui_startmenu_is_open() == 0, "should be closed");
    PASS();
}

static void test_open_close(void) {
    TEST("dosgui_startmenu_open/close/toggle");
    dosgui_startmenu_init();
    dosgui_startmenu_open();
    CHECK(dosgui_startmenu_is_open() == 1, "should be open");
    dosgui_startmenu_close();
    CHECK(dosgui_startmenu_is_open() == 0, "should be closed");
    dosgui_startmenu_toggle();
    CHECK(dosgui_startmenu_is_open() == 1, "toggle opens");
    dosgui_startmenu_toggle();
    CHECK(dosgui_startmenu_is_open() == 0, "toggle closes");
    PASS();
}

static void test_render_no_crash(void) {
    TEST("dosgui_startmenu_render no crash (with VBE)");
    dosgui_startmenu_init();
    vbe_init(320, 200);
    dosgui_startmenu_open();
    dosgui_startmenu_render(NULL, 320, 200);
    dosgui_startmenu_close();
    dosgui_startmenu_render(NULL, 320, 200);
    vbe_shutdown();
    PASS();
}

static void test_multiple_toggles(void) {
    TEST("Multiple open/close/toggle cycles");
    dosgui_startmenu_init();
    for (int i = 0; i < 10; i++) {
        dosgui_startmenu_toggle();
        CHECK(dosgui_startmenu_is_open() == 1, "toggle opens");
        dosgui_startmenu_toggle();
        CHECK(dosgui_startmenu_is_open() == 0, "toggle closes");
    }
    PASS();
}

int main(void) {
    printf("+========================================================+\n");
    printf("|  WuBuOS DosGui Start Menu Test Suite (Cell 402)        |\n");
    printf("+========================================================+\n\n");

    test_init();
    test_open_close();
    test_render_no_crash();
    test_multiple_toggles();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}