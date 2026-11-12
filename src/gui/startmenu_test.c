/*
 * startmenu_test.c  --  WuBuOS DosGui Start Menu Test Suite (Cell 402)
 */

#include "../egojar.h"
#include "shell/startmenu.h"
#include "shell/startmenu_internal.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include "../apps/apps.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-50s", name); g_total++;
#define PASS() do { printf("\u2705\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("\u274C %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static void test_init(void) {
    TEST("startmenu_init initializes state");
    startmenu_init();
    CHECK(startmenu_is_open() == 0, "should be closed");
    PASS();
}

static void test_open_close(void) {
    TEST("startmenu_open/close/toggle");
    startmenu_init();
    startmenu_open();
    CHECK(startmenu_is_open() == 1, "should be open");
    startmenu_close();
    CHECK(startmenu_is_open() == 0, "should be closed");
    startmenu_toggle();
    CHECK(startmenu_is_open() == 1, "toggle opens");
    startmenu_toggle();
    CHECK(startmenu_is_open() == 0, "toggle closes");
    PASS();
}

static void test_render_no_crash(void) {
    TEST("startmenu_render no crash (with VBE)");
    startmenu_init();
    vbe_init(320, 200);
    startmenu_open();
    startmenu_render(NULL, 320, 200);
    startmenu_close();
    startmenu_render(NULL, 320, 200);
    vbe_shutdown();
    PASS();
}

/* Regression for the XP Luna start-menu chrome: the sidebar must draw a
 * vertical gradient (blue -> green) and the orb logo must render. */
static void test_xp_sidebar_gradient_and_orb(void) {
    TEST("XP start menu draws gradient sidebar + orb");
    startmenu_init();
    wubu_theme_set(THEME_XP_LUNA_BLUE);
    vbe_init(640, 480);
    startmenu_open();
    startmenu_render(NULL, 640, 480);

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

    startmenu_close();
    vbe_shutdown();
    PASS();
}

static void test_multiple_toggles(void) {
    TEST("Multiple open/close/toggle cycles");
    startmenu_init();
    for (int i = 0; i < 10; i++) {
        startmenu_toggle();
        CHECK(startmenu_is_open() == 1, "toggle opens");
        startmenu_toggle();
        CHECK(startmenu_is_open() == 0, "toggle closes");
    }
    PASS();
}

/* -- Discovered .wet applets reach the start menu ------------------ */

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(content, f);
    fclose(f);
}

static void test_wet_applets_in_programs(void) {
    TEST("Discovered .wet applets appear in the Programs submenu");

    const char *dir = "/tmp/wubu_startmenu_applets";
    mkdir(dir, 0755);
    char p1[256], p2[256], p3[256];
    snprintf(p1, sizeof p1, "%s/notes.wet", dir);
    snprintf(p2, sizeof p2, "%s/paint.wet", dir);
    snprintf(p3, sizeof p3, "%s/readme.txt", dir);
    write_file(p1, "form notes {}\n");
    write_file(p2, "form paint {}\n");
    write_file(p3, "not an applet\n");

    CHECK(app_scan_wet_dir(dir) == 2, "exactly the two .wet files register");
    CHECK(app_wet_count() == 2, "the registry holds two applets");

    /* Rebuild the menu the way the real shell does. */
    startmenu_build_submenus();

    int found_notes = 0, found_paint = 0, found_readme = 0;
    for (int i = 0; i < g_programs.item_count; i++) {
        if (strcmp(g_programs.items[i].label, "notes") == 0) found_notes = 1;
        if (strcmp(g_programs.items[i].label, "paint") == 0)  found_paint = 1;
        if (strcmp(g_programs.items[i].label, "readme.txt") == 0) found_readme = 1;
    }
    CHECK(found_notes, "notes appears in Programs");
    CHECK(found_paint,  "paint appears in Programs");
    CHECK(!found_readme, "the .txt impostor does not");

    /* The launch path must resolve by the same name the menu shows. */
    const DosGuiAppDef *a = app_find_wet_by_name("notes");
    CHECK(a != NULL, "app_find_wet_by_name resolves the menu label");
    CHECK(a && a->path && strstr(a->path, "notes.wet") != NULL,
          "the entry carries the file path");

    /* A second scan must not duplicate. */
    CHECK(app_scan_wet_dir(dir) == 0, "a second scan adds nothing");
    CHECK(app_wet_count() == 2, "still two after the rescan");

    remove(p1); remove(p2); remove(p3); rmdir(dir);
    PASS();
}

int main(void) {
    printf("+========================================================+\n");
    printf("|  WuBuOS DosGui Start Menu Test Suite (Cell 402)        |\n");
    printf("+========================================================+\n\n");

    test_init();
    test_open_close();
    test_render_no_crash();
    test_xp_sidebar_gradient_and_orb();
    test_multiple_toggles();
    test_wet_applets_in_programs();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}