/*
 * dosgui_startmenu_test.c  --  WuBuOS DosGui Start Menu Test Suite (Cell 402)
 */

#include "dosgui_startmenu.h"
#include "dosgui_startmenu_internal.h"
#include "dosgui_wm.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

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

/* Diagnostic: clicking the Programs main item opens its flyout. */
static void test_programs_click_opens_flyout(void) {
    TEST("clicking Programs opens the flyout (submenu_open)");
    dosgui_startmenu_init();
    vbe_init(1024, 768);
    dosgui_startmenu_open();
    int task_h = dosgui_taskbar_height();
    int mh = 24;
    int menu_y = dosgui_wm_screen_h() - task_h - (g_main_count * mh + 4);
    int prog_x = 56 + 30;
    int prog_y = menu_y + 2 + mh / 2;
    dosgui_startmenu_handle_click(prog_x, prog_y);
    CHECK(g_submenu_open == 0, "Programs click sets submenu_open=0");
    dosgui_startmenu_close();
    vbe_shutdown();
    PASS();
}

/* XP parity end-to-end: opening Programs then clicking a category
 * renders the nested cascade submenu to the right of the main menu. */
static void test_cascade_renders_nested_submenu(void) {
    TEST("cascade: Programs -> category renders nested submenu");
    dosgui_startmenu_init();
    vbe_init(1024, 768);
    dosgui_startmenu_open();

    int task_h = dosgui_taskbar_height();
    int mh = 24;
    int menu_y = dosgui_wm_screen_h() - task_h - (g_main_count * mh + 4);

    /* Click Programs (main item 0) -> opens its flyout (submenu_open=0). */
    dosgui_startmenu_handle_click(56 + 30, menu_y + 2 + mh / 2);
    dosgui_startmenu_render(NULL, 1024, 768);
    CHECK(g_submenu_open == 0, "Programs click opens the flyout (submenu_open=0)");

    /* Click the first category row (Accessories, SM_ITEM_SUBMENU,
     * submenu_id=1) in the Programs flyout -> opens the nested cascade
     * (submenu_open becomes 1). Flyout at sx=4+200=204. */
    dosgui_startmenu_handle_click(204 + 40, menu_y + 2 + mh / 2);
    dosgui_startmenu_render(NULL, 1024, 768);
    CHECK(g_submenu_open == 1, "category click opens the nested cascade (submenu_open=1)");

    dosgui_startmenu_close();
    vbe_shutdown();
    PASS();
}

/* XP parity: the Start-menu search box captures typed keys and updates
 * filtered results (the WM now routes keys here when the menu is open,
 * and handle_key appends printable chars to the query). */
static void test_search_key_input(void) {
    TEST("start-menu search captures typed keys + updates results");
    dosgui_startmenu_init();
    dosgui_startmenu_open();

    CHECK(!dosgui_startmenu_search_active(), "search inactive before Tab");
    /* Tab enters search mode. */
    dosgui_startmenu_handle_key('\t', 0);
    CHECK(dosgui_startmenu_search_active(), "Tab activates search mode");

    /* Type characters; handle_key must append them to the query + refilter
     * (no crash, results stay queryable). */
    dosgui_startmenu_handle_key('c', 0);
    dosgui_startmenu_handle_key('a', 0);
    dosgui_startmenu_handle_key('l', 0);
    SmProgramEntry **res = NULL; int n = 0;
    dosgui_startmenu_search_get_results(&res, &n);
    CHECK(n >= 0, "results queryable after typing");

    /* Backspace removes a char (still active). */
    dosgui_startmenu_handle_key(8, 0);
    CHECK(dosgui_startmenu_search_active(), "search still active after backspace");

    /* Esc clears search. */
    dosgui_startmenu_handle_key(27, 0);
    CHECK(!dosgui_startmenu_search_active(), "Esc clears search mode");

    dosgui_startmenu_close();
    PASS();
}

/* XP parity: hover opens the cascade chain (All Programs -> category).
 * Previously only click opened a flyout; XP opens on hover. */
static void test_hover_opens_cascade(void) {
    TEST("hover opens Programs flyout then nested category cascade");
    dosgui_startmenu_init();
    dosgui_startmenu_open();

    int task_h = dosgui_taskbar_height();
    int mh = 24;
    int sw = 48;              /* sidebar_w */
    int menu_y = dosgui_wm_screen_h() - task_h - (g_main_count * mh + 4);
    int content_x = 4 + sw + 4;

    /* Hover the Programs main item (index 0) -> flyout (submenu 0) opens. */
    int prog_x = content_x + 30;
    int prog_y = menu_y + 2 + mh / 2;
    dosgui_startmenu_track_hover(prog_x, prog_y);
    CHECK(g_submenu_open == 0, "hovering Programs opens its flyout (submenu 0)");

    /* Hover a category item inside the Programs flyout. The flyout is at
     * sx = 4 + 200 = 204; first row near sx+6+20. Programs leads with
     * SM_ITEM_SUBMENU category items (Accessories=1, WuBuOS=2, System=3). */
    int cat_x = 204 + 40;
    int cat_y = menu_y + 2 + 0 * mh + mh / 2;
    dosgui_startmenu_track_hover(cat_x, cat_y);
    CHECK(g_submenu_open >= 1, "hovering a category opens its nested cascade");

    dosgui_startmenu_close();
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

extern char g_last_launch[64];

static int name_has_prefix(const char *name, const char *q)
{
    for (int i = 0; q[i]; i++) {
        if (!name[i]) return 0;
        if (tolower((unsigned char)name[i]) != tolower((unsigned char)q[i])) return 0;
    }
    return 1;
}

/* UX #495: prefix matches rank before substring matches, Enter launches the
 * top hit, and clear resets the search state. */
static void test_search_ordering_and_launch(void) {
    TEST("search: prefix-before-substring, Enter launch, clear resets");
    dosgui_startmenu_init();
    dosgui_startmenu_open();
    /* Seed a DB whose storage order would put a substring match first:
     * "Local Calendar" is added before "Calculator" / "Camera". */
    memset(&g_program_db, 0, sizeof(g_program_db));
    snprintf(g_program_db.entries[0].name, sizeof(g_program_db.entries[0].name), "%s", "Local Calendar");
    snprintf(g_program_db.entries[1].name, sizeof(g_program_db.entries[1].name), "%s", "Calculator");
    snprintf(g_program_db.entries[2].name, sizeof(g_program_db.entries[2].name), "%s", "Camera");
    g_program_db.count = 3;

    /* Type "ca" through the same entry point the key handler uses. */
    dosgui_startmenu_search_update("ca");

    SmProgramEntry **res = NULL; int n = 0;
    dosgui_startmenu_search_get_results(&res, &n);
    CHECK(n == 3, "query 'ca' matches all three seeded programs");

    int seen_sub = 0, order_ok = 1;
    for (int i = 0; i < n; i++) {
        int p = name_has_prefix(res[i]->name, "ca");
        if (!p) seen_sub = 1;
        else if (seen_sub) order_ok = 0;
    }
    CHECK(order_ok, "prefix matches precede substring matches");
    CHECK(strcmp(res[2]->name, "Local Calendar") == 0, "substring match ranks last");

    int n_before = n;
    dosgui_startmenu_handle_key(8, 0);              /* backspace: "ca" -> "c" */
    dosgui_startmenu_search_get_results(&res, &n);
    CHECK(n >= n_before, "backspace widens (or holds) the result set");

    dosgui_startmenu_search_update("ca");
    dosgui_startmenu_search_get_results(&res, &n);
    CHECK(n == 3, "query restored after retyping");

    char top[64];
    snprintf(top, sizeof(top), "%s", res[0]->name);
    g_last_launch[0] = '\0';
    dosgui_startmenu_handle_key(13, 0);             /* Enter */
    CHECK(strcmp(g_last_launch, top) == 0, "Enter launches the top hit");
    CHECK(dosgui_startmenu_is_open() == 0, "menu closes after launch");

    /* clear resets state */
    dosgui_startmenu_open();
    dosgui_startmenu_handle_key('\t', 0);
    dosgui_startmenu_handle_key('c', 0);
    dosgui_startmenu_search_clear();
    CHECK(!dosgui_startmenu_search_active(), "clear deactivates search");
    CHECK(g_search.query[0] == '\0' && g_search.result_count == 0,
          "clear resets query and results");
    dosgui_startmenu_close();
    PASS();
}

int main(void) {
    printf("+========================================================+\n");
    printf("|  WuBuOS DosGui Start Menu Test Suite (Cell 402)        |\n");
    printf("+========================================================+\n\n");

    test_init();
    test_sm_registry();
    test_programs_cascade_items();
    test_programs_click_opens_flyout();
    test_hover_opens_cascade();
    test_search_key_input();
    test_search_ordering_and_launch();
    test_cascade_renders_nested_submenu();
    test_open_close();
    test_render_no_crash();
    test_xp_sidebar_gradient_and_orb();
    test_multiple_toggles();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}