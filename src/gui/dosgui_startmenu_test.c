/*
 * dosgui_startmenu_test.c  --  WuBuOS DosGui Start Menu Test Suite (Cell 402)
 */

#include "dosgui_startmenu.h"
#include "wubu_theme.h"
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

/* Regression for the XP Luna start-menu chrome: the sidebar must draw a
 * vertical gradient (blue -> green) and the orb logo must render. */
static void test_xp_sidebar_gradient_and_orb(void) {
    TEST("XP start menu draws gradient sidebar + orb");
    dosgui_startmenu_init();
    wubu_theme_set(THEME_XP_LUNA_BLUE);
    vbe_init(640, 480);
    dosgui_startmenu_open();
    dosgui_startmenu_render(NULL, 640, 480);

    /* Scan the VBE buffer for signature XP colours (stored as-is, no swap). */
    int found_blue = 0, found_orb_red = 0;
    for (int y = 0; y < 480 && !(found_blue && found_orb_red); y++)
        for (int x = 0; x < 640; x++) {
            uint32_t p = vbe_get_pixel(x, y);
            if (p == 0x00539E) { found_blue = 1; break; }      /* Luna sidebar blue */
            if (p == 0xF24C3E) { found_orb_red = 1; break; }    /* orb red quadrant */
        }
    CHECK(found_blue, "XP sidebar gradient (Luna blue) drawn");
    CHECK(found_orb_red, "XP orb logo (red quadrant) drawn");

    dosgui_startmenu_close();
    vbe_shutdown();
    PASS();
}

/* XP parity: the nested-cascade registry resolves a registered id to
 * its submenu and rejects out-of-range ids with NULL. */
static void test_sm_registry(void) {
    TEST("startmenu cascade registry resolves ids");
    dosgui_startmenu_init();
    /* Programs (id 0) is registered by init. */
    SmSubmenu *p = dosgui_sm_get(0);
    CHECK(p != NULL, "registry resolves id 0 (Programs)");
    CHECK(dosgui_sm_get(-1) == NULL, "registry rejects id -1");
    CHECK(dosgui_sm_get(SM_MAX_SUBMENUS) == NULL, "registry rejects out-of-range id");
    PASS();
}

/* XP parity: Programs is a cascade — it holds SM_ITEM_SUBMENU category
 * items whose submenu_id resolves to a registered nested submenu. */
static void test_programs_cascade_items(void) {
    TEST("Programs flyout holds nested category cascades");
    dosgui_startmenu_init();
    SmSubmenu *prog = dosgui_sm_get(0);
    CHECK(prog != NULL, "Programs registered");
    if (!prog) { PASS(); return; }

    int submenu_items = 0, resolvable = 0;
    for (int i = 0; i < prog->item_count; i++) {
        SmMenuItem *it = &prog->items[i];
        if (it->type == SM_ITEM_SUBMENU) {
            submenu_items++;
            if (dosgui_sm_get(it->submenu_id) != NULL) resolvable++;
        }
    }
    CHECK(submenu_items >= 3, "Programs has >=3 category cascade items");
    CHECK(resolvable == submenu_items, "every category cascade resolves to a submenu");
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
    test_sm_registry();
    test_programs_cascade_items();
    test_open_close();
    test_render_no_crash();
    test_xp_sidebar_gradient_and_orb();
    test_multiple_toggles();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}