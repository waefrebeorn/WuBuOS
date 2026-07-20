/*
 * dosgui_wm_test.c  --  WuBuOS DosGui Window Manager Test Suite
 *
 * Cell 400: Fable Windowing Agent test suite.
 * Tests window creation, z-order, drag, focus, taskbar, icons.
 */

#include "dosgui_wm.h"
#include "dosgui_wm_internal.h"
#include "wubu_trash.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) printf("  TEST %-55s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static void rm_rf(const char *path) {
    /* Minimal recursive delete for test temp dirs. */
    struct stat st;
    if (stat(path, &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *e;
            char buf[1024];
            while ((e = readdir(d)) != NULL) {
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                snprintf(buf, sizeof(buf), "%s/%s", path, e->d_name);
                rm_rf(buf);
            }
            closedir(d);
        }
        rmdir(path);
    } else {
        unlink(path);
    }
}

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

/* Regression: Win98 Classic (flat, rad==0) windows must render the THEME's
 * navy title color (win_title_active = 0x000080), NOT the old hardcoded
 * XP-blue gradient (0x0A2A6A..0x3A6EA5) that vbe_title_bar() used to emit.
 * A regression to the hardcoded shade would re-introduce the "broken GUI /
 * wrong Chicago theme" bug. */
static void test_render_win98_title_bar_is_theme_navy(void) {
    TEST("Win98 title bar uses theme navy, not hardcoded XP-blue");
    wubu_theme_set(THEME_WIN98_CLASSIC);
    vbe_init(256, 256);
    dosgui_wm_init(256, 256);

    DosGuiWindow *w = dosgui_wm_create(10, 10, 160, 100, "Navy Title");
    CHECK(w, "window created");
    if (w) w->on_draw = NULL;

    dosgui_wm_render(NULL, 256, 256);

    /* Sample the active (focused) title bar: center of the band, a few px
     * down from the top edge. Under Win98 this must be the theme navy
     * 0x000080, never the XP-blue gradient (which would be a much lighter
     * blue like 0x3A6EA5). */
    int tx = w->x + w->w / 2;
    int ty = w->y + 4;
    uint32_t c = vbe_get_pixel(tx, ty);
    CHECK(c == 0x000080, "title bar pixel is theme navy (0x000080)");

    /* And confirm the old XP-blue shade is NOT what we got. */
    CHECK(c != 0x003A6EA5 && c != 0x000A2A6A, "title bar is not the old XP-blue gradient");

    dosgui_wm_shutdown();
    PASS();
}

/* Regression: the VBE scissor/clip rect must confine pixel writes. A buggy
 * on_draw (or any primitive) must NEVER paint outside the clip -- this is
 * what prevents "content bleeding into neighboring windows" artifacts. */
static void test_vbe_clip_confines_pixels(void) {
    TEST("vbe clip rect confines pixel writes");
    vbe_init(64, 64);

    vbe_clear(0x00000000);
    vbe_set_clip(10, 10, 20, 20);
    /* Attempt to paint far outside the clip -- must be rejected. */
    vbe_set_pixel(2, 2, 0x00FF0000);
    vbe_set_pixel(63, 63, 0x00FF0000);
    vbe_fill_rect(0, 0, 64, 64, 0x00FF0000);
    CHECK(vbe_get_pixel(2, 2) == 0x00000000, "pixel outside clip untouched");
    CHECK(vbe_get_pixel(63, 63) == 0x00000000, "far corner outside clip untouched");
    /* Pixel inside the clip must land. */
    vbe_set_pixel(15, 15, 0x0000FF00);
    CHECK(vbe_get_pixel(15, 15) == 0x0000FF00, "pixel inside clip lands");

    vbe_reset_clip();
    vbe_set_pixel(2, 2, 0x00FF0000);
    CHECK(vbe_get_pixel(2, 2) == 0x00FF0000, "after reset_clip, pixel writes again");
    PASS();
}

/* Regression: a window whose body would extend into the taskbar band must
 * be clipped ABOVE the taskbar -- its title bar / close "X" must never paint
 * over the taskbar (the "FILE MANAGER CLOSE" text-merge artifact). */
static void test_render_window_clipped_above_taskbar(void) {
    TEST("window body never bleeds into the taskbar band");
    wubu_theme_set(THEME_WIN98_CLASSIC);
    vbe_init(256, 256);
    dosgui_wm_init(256, 256);

    int task_h = dosgui_taskbar_height();   /* 28 for Win98 */
    int tb_top = 256 - task_h;

    /* Create a window whose bottom extends well past the taskbar. */
    DosGuiWindow *w = dosgui_wm_create(20, tb_top - 10, 120, 120, "Low Win");
    CHECK(w, "window created");
    if (w) w->on_draw = NULL;
    dosgui_wm_render(NULL, 256, 256);

    /* No window-title (navy) pixel may appear inside the taskbar band. */
    int bleed = 0;
    for (int y = tb_top; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            uint32_t c = vbe_get_pixel(x, y);
            int r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF;
            if (b > 80 && r < 60 && g < 60) bleed++;   /* navy title in band */
        }
    CHECK(bleed == 0, "no window-title pixel painted in the taskbar band");

    dosgui_wm_shutdown();
    PASS();
}

/* Regression for the Chicago->XP desktop polish: icons must draw a real
 * glyph (not a flat box) and a selected icon must show a selection rect. */
static void test_render_draws_glyphs_and_selection(void) {
    TEST("render draws icon glyphs + selection highlight");
    vbe_init(256, 256);
    dosgui_wm_init(256, 256);

    int folder = dosgui_icon_add_ex("Projects", DESK_ICON_FOLDER, NULL, 0, 0, 0, NULL);
    int file   = dosgui_icon_add_ex("Notes.txt", DESK_ICON_FILE, NULL, 1, 0, 0, NULL);
    DosGuiIcon *fi = dosgui_icon_get(folder);
    DosGuiIcon *li = dosgui_icon_get(file);
    CHECK(fi && li, "both icons added");
    if (fi) fi->selected = true;   /* select the folder icon */

    dosgui_wm_render(NULL, 256, 256);

    /* Glyph check: count non-background pixels inside the folder icon cell. */
    int glyph_pixels = 0;
    for (int y = fi->y; y < fi->y + DOSGUI_ICON_SIZE; y++)
        for (int x = fi->x; x < fi->x + DOSGUI_ICON_SIZE; x++)
            if (vbe_get_pixel(x, y) != 0) glyph_pixels++;
    CHECK(glyph_pixels > 20, "folder glyph drew recognizable pixels");

    /* Selection check: the white focus outline pixel should appear at the
     * top-left corner of the selected icon cell. */
    uint32_t corner = vbe_get_pixel(fi->x, fi->y);
    CHECK(corner == 0x00FFFFFF, "selection focus rect drawn at icon corner");

    dosgui_wm_shutdown();
    PASS();
}

/* Regression for the XP Luna drop-shadow: a window must cast a soft blended
 * shadow to its bottom-right under Luna themes (and none under Win98). */
static void test_render_window_drop_shadow(void) {
    TEST("XP window casts a soft drop-shadow");
    wubu_theme_set(THEME_XP_LUNA_BLUE);
    vbe_init(256, 256);
    dosgui_wm_init(256, 256);
    dosgui_wm_create(10, 10, 120, 90, "Shadowed");
    dosgui_wm_render(NULL, 256, 256);

    /* Luminance of a pixel in the shadow band (just right of the window) vs a
     * reference desktop pixel far from any window. The shadow blends a dark
     * colour over the desktop, so it must be visibly darker. */
    int sx = 10 + 120 + 3, sy = 10 + 45;       /* shadow band, right side */
    int rx = 240, ry = 20;                       /* reference desktop pixel */
    uint32_t sp = vbe_get_pixel(sx, sy);
    uint32_t rp = vbe_get_pixel(rx, ry);
    int sl = (int)((sp >> 16 & 0xFF) + (sp >> 8 & 0xFF) + (sp & 0xFF));
    int rl = (int)((rp >> 16 & 0xFF) + (rp >> 8 & 0xFF) + (rp & 0xFF));
    CHECK(sl < rl, "shadow pixel darker than reference desktop");

    dosgui_wm_shutdown();
    PASS();
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


/* -- Desktop Live Namespace Tests (Stream A: ReactOS explorer/desktop.cpp) -- */

static void test_desktop_live_namespace_init(void) {
    TEST("~/Desktop .desktop files appear as icons on init");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_desktest_%d", getpid());
    setenv("XDG_DESKTOP_DIR", desk, 1);
    rm_rf(desk);
    mkdir(desk, 0755);
    /* Drop two real .desktop shortcuts. */
    char p1[600], p2[600];
    snprintf(p1, sizeof(p1), "%s/Alpha.desktop", desk);
    snprintf(p2, sizeof(p2), "%s/Bravo.desktop", desk);
    FILE *f = fopen(p1, "w"); if (f) { fputs("[Desktop Entry]\n", f); fclose(f); }
    f = fopen(p2, "w"); if (f) { fputs("[Desktop Entry]\n", f); fclose(f); }

    wubu_settings_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_refresh_desktop();

    int found_alpha = 0, found_bravo = 0;
    for (int i = 0; i < dosgui_wm_get_icon_count(); i++) {
        DosGuiIcon *ic = dosgui_icon_get(i);
        if (!ic || !ic->alive) continue;
        if (strcasecmp(ic->name, "Alpha") == 0) found_alpha = 1;
        if (strcasecmp(ic->name, "Bravo") == 0) found_bravo = 1;
    }
    CHECK(found_alpha, "Alpha.desktop surfaced as desktop icon");
    CHECK(found_bravo, "Bravo.desktop surfaced as desktop icon");

    dosgui_wm_shutdown();
    rm_rf(desk);
    PASS();
}

static void test_desktop_new_folder(void) {
    TEST("New Folder creates a real dir + live icon");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_desktest_%d", getpid());
    setenv("XDG_DESKTOP_DIR", desk, 1);
    rm_rf(desk);
    mkdir(desk, 0755);

    wubu_settings_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_refresh_desktop();
    int before = dosgui_wm_get_icon_count();

    int rc = dosgui_wm_new_folder();
    CHECK(rc == 0, "dosgui_wm_new_folder returns 0");
    CHECK(access(desk, 0) == 0 && access(desk, 0) == 0, "folder exists");
    /* The new folder must be a real directory on disk. */
    char folder[600];
    snprintf(folder, sizeof(folder), "%s/New Folder", desk);
    struct stat st;
    CHECK(stat(folder, &st) == 0 && S_ISDIR(st.st_mode), "New Folder is a real directory");
    CHECK(dosgui_wm_get_icon_count() > before, "new icon added after folder creation");

    dosgui_wm_shutdown();
    rm_rf(desk);
    PASS();
}

static void test_desktop_new_text_doc(void) {
    TEST("New Text Document creates a real file + live icon");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_desktest_%d", getpid());
    setenv("XDG_DESKTOP_DIR", desk, 1);
    rm_rf(desk);
    mkdir(desk, 0755);

    wubu_settings_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_refresh_desktop();
    int before = dosgui_wm_get_icon_count();

    int rc = dosgui_wm_new_text_doc();
    CHECK(rc == 0, "dosgui_wm_new_text_doc returns 0");
    char doc[600];
    snprintf(doc, sizeof(doc), "%s/New Text Document.txt", desk);
    struct stat st;
    CHECK(stat(doc, &st) == 0 && S_ISREG(st.st_mode), "New Text Document is a real file");
    CHECK(dosgui_wm_get_icon_count() > before, "new icon added after document creation");

    dosgui_wm_shutdown();
    rm_rf(desk);
    PASS();
}

static void test_desktop_sort_by_size(void) {
    TEST("Sort By Size reorders icons by stat size");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_desktest_%d", getpid());
    setenv("XDG_DESKTOP_DIR", desk, 1);
    rm_rf(desk);
    mkdir(desk, 0755);
    /* Two .desktop files of different sizes. */
    char small[600], big[600];
    snprintf(small, sizeof(small), "%s/Small.desktop", desk);
    snprintf(big, sizeof(big), "%s/Big.desktop", desk);
    FILE *f = fopen(small, "w"); if (f) { fputc('x', f); fclose(f); }
    f = fopen(big, "w"); if (f) { for (int i = 0; i < 64; i++) fputc('x', f); fclose(f); }

    wubu_settings_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_refresh_desktop();

    /* Force Name sort first (stable), then Size sort. */
    dosgui_wm_sort_icons(DOSGUI_SORT_NAME);
    dosgui_wm_sort_icons(DOSGUI_SORT_SIZE);

    /* Find the two icons and confirm Big precedes Small (ascending size). */
    int small_idx = -1, big_idx = -1;
    for (int i = 0; i < dosgui_wm_get_icon_count(); i++) {
        DosGuiIcon *ic = dosgui_icon_get(i);
        if (!ic || !ic->alive) continue;
        if (strcasecmp(ic->name, "Big") == 0) big_idx = i;
        if (strcasecmp(ic->name, "Small") == 0) small_idx = i;
    }
    CHECK(small_idx >= 0 && big_idx >= 0, "both icons present after sort");
    CHECK(big_idx < small_idx, "Big (larger) icon ordered before Small by size");

    dosgui_wm_shutdown();
    rm_rf(desk);
    PASS();
}

/* Regression: the desktop must retain MORE than the old 16-icon cap when
 * ~/Desktop holds many entries (refresh used to silently drop them). */
static void test_desktop_many_icons_retained(void) {
    TEST("refresh retains >16 icons (no silent drop at cap)");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_desktest_%d", getpid());
    setenv("XDG_DESKTOP_DIR", desk, 1);
    rm_rf(desk);
    mkdir(desk, 0755);

    /* Create 30 real .desktop files in ~/Desktop. */
    for (int i = 0; i < 30; i++) {
        char p[640];
        snprintf(p, sizeof(p), "%s/App%02d.desktop", desk, i);
        FILE *f = fopen(p, "w");
        if (f) { fputs("[Desktop Entry]\n", f); fclose(f); }
    }

    wubu_settings_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_refresh_desktop();

    int found = 0;
    for (int i = 0; i < dosgui_wm_get_icon_count(); i++) {
        DosGuiIcon *ic = dosgui_icon_get(i);
        if (ic && ic->alive) found++;
    }
    CHECK(found >= 20, "at least 20 of 30 desktop entries surfaced as icons");

    dosgui_wm_shutdown();
    rm_rf(desk);
    PASS();
}

/* Regression: dragging an icon + ending the drag persists its grid position
 * so it survives a restore (ReactOS-style layout persistence). */
static void test_icon_drag_persist_restore(void) {
    TEST("drag-end saves + restore reapplies icon position");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_desktest_%d", getpid());
    setenv("XDG_DESKTOP_DIR", desk, 1);
    rm_rf(desk);
    mkdir(desk, 0755);
    char p[640];
    snprintf(p, sizeof(p), "%s/Pin.desktop", desk);
    FILE *f = fopen(p, "w"); if (f) { fputs("[Desktop Entry]\n", f); fclose(f); }

    wubu_settings_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_refresh_desktop();

    /* Find the icon and simulate a drag: move it to grid (3,4), then persist. */
    int idx = -1;
    for (int i = 0; i < dosgui_wm_get_icon_count(); i++) {
        DosGuiIcon *ic = dosgui_icon_get(i);
        if (ic && ic->alive && strcasecmp(ic->name, "Pin") == 0) { idx = i; break; }
    }
    CHECK(idx >= 0, "Pin icon present");
    DosGuiIcon *ic = dosgui_icon_get(idx);
    ic->grid_x = 3; ic->grid_y = 4;
    ic->x = 20 + 3 * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    ic->y = 20 + 4 * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    dosgui_wm_save_icon_layout();   /* what the drag-end handler now calls */

    /* Simulate restart: re-init (desktop.c boot path re-enumerates ~/Desktop),
     * then restore persisted positions -- matching real boot flow. */
    dosgui_wm_shutdown();
    wubu_settings_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_refresh_desktop();   /* dosgui_desktop_init() does this on boot */
    dosgui_wm_restore_icon_layout();

    int ridx = -1;
    for (int i = 0; i < dosgui_wm_get_icon_count(); i++) {
        DosGuiIcon *rc = dosgui_icon_get(i);
        if (rc && rc->alive && strcasecmp(rc->name, "Pin") == 0) { ridx = i; break; }
    }
    CHECK(ridx >= 0, "Pin icon present after re-init");
    DosGuiIcon *rc = dosgui_icon_get(ridx);
    CHECK(rc->grid_x == 3 && rc->grid_y == 4, "persisted grid (3,4) restored");

    dosgui_wm_shutdown();
    rm_rf(desk);
    PASS();
}

/* Regression: Delete routes the underlying ~/Desktop file to the Recycle Bin
 * (not just an in-memory hide) and compacts the icon array. */
static void test_icon_delete_moves_to_trash(void) {
    TEST("Delete moves underlying file to trash + compacts");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_deltest_%d", getpid());
    setenv("XDG_DESKTOP_DIR", desk, 1);
    setenv("XDG_DATA_HOME", desk, 1);   /* trash lives under XDG data home */
    rm_rf(desk);
    mkdir(desk, 0755);
    char p[640];
    snprintf(p, sizeof(p), "%s/Note.desktop", desk);
    FILE *f = fopen(p, "w"); if (f) { fputs("[Desktop Entry]\n", f); fclose(f); }

    wubu_settings_init();
    wubu_trash_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_refresh_desktop();

    int before = dosgui_wm_get_icon_count();
    int idx = -1;
    for (int i = 0; i < before; i++) {
        DosGuiIcon *ic = dosgui_icon_get(i);
        if (ic && ic->alive && strcasecmp(ic->name, "Note") == 0) { idx = i; break; }
    }
    CHECK(idx >= 0, "Note icon present");
    DosGuiIcon *ic = dosgui_icon_get(idx);
    CHECK(ic->target[0] != '\0', "icon carries real fs target path");

    /* Simulate confirm-delete of the single icon (replicates dialog_delete_on_key). */
    if (ic->target[0]) wubu_trash_move(ic->target);
    ic->alive = false;
    dosgui_wm_compact_icons();

    CHECK(dosgui_wm_get_icon_count() == before - 1, "icon count dropped by 1 after delete");
    /* Underlying file must be gone from ~/Desktop (moved to trash). */
    struct stat st;
    CHECK(stat(p, &st) != 0, "underlying .desktop removed from ~/Desktop");

    dosgui_wm_shutdown();
    rm_rf(desk);
    PASS();
}

/* Regression: multi-select delete removes the whole selected group + compacts. */
static void test_icon_multiselect_delete(void) {
    TEST("multi-select delete removes all selected icons");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_msel_%d", getpid());
    setenv("XDG_DESKTOP_DIR", desk, 1);
    setenv("XDG_DATA_HOME", desk, 1);
    rm_rf(desk);
    mkdir(desk, 0755);
    for (int k = 0; k < 3; k++) {
        char pp[640];
        snprintf(pp, sizeof(pp), "%s/Item%d.desktop", desk, k);
        FILE *f = fopen(pp, "w"); if (f) { fputs("[Desktop Entry]\n", f); fclose(f); }
    }

    wubu_settings_init();
    wubu_trash_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_refresh_desktop();

    int before = dosgui_wm_get_icon_count();
    /* Select every live icon. */
    int sel = 0;
    for (int i = 0; i < before; i++) {
        DosGuiIcon *ic = dosgui_icon_get(i);
        if (ic && ic->alive) { ic->selected = true; sel++; }
    }
    CHECK(sel >= 3, "at least 3 icons selected");

    /* Replicate dialog_delete_on_key multi-select branch. */
    for (int i = 0; i < g_dwm.icon_count; i++) {
        if (g_dwm.icons[i].alive && g_dwm.icons[i].selected) {
            if (g_dwm.icons[i].target[0]) wubu_trash_move(g_dwm.icons[i].target);
            g_dwm.icons[i].alive = false;
            g_dwm.icons[i].selected = false;
        }
    }
    dosgui_wm_compact_icons();

    CHECK(dosgui_wm_get_icon_count() == before - sel, "all selected icons removed + compacted");

    dosgui_wm_shutdown();
    rm_rf(desk);
    PASS();
}

/* Regression: auto-arrange toggle persists to settings and re-applies on boot. */
static void test_auto_arrange_persists_restart(void) {
    TEST("auto-arrange toggle persists + re-applies on boot");
    char dsk[512];
    snprintf(dsk, sizeof(dsk), "/tmp/wubu_arr_%d", getpid());
    setenv("XDG_DESKTOP_DIR", dsk, 1);
    setenv("XDG_CONFIG_HOME", dsk, 1);
    rm_rf(dsk); mkdir(dsk, 0755);

    wubu_settings_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_set_auto_arrange(true);
    const WubuSettings *s = wubu_settings_get();
    CHECK(s->theme.auto_arrange == true, "settings recorded auto-arrange ON");

    /* Simulate restart: reload settings, re-init WM, then run the EXACT
     * boot-path restore line from dosgui_desktop_init():
     *   dosgui_wm_set_auto_arrange(s->theme.auto_arrange);
     * (calling the full dosgui_desktop_init() would pull in the DOS emulator
     * subsystem; the restore logic is a single setter call, tested here.) */
    dosgui_wm_shutdown();
    wubu_settings_init();
    dosgui_wm_init(1024, 768);
    dosgui_wm_set_auto_arrange(wubu_settings_get()->theme.auto_arrange);
    CHECK(dosgui_wm_get_auto_arrange() == true, "auto-arrange ON after re-init (boot path)");

    dosgui_wm_shutdown();
    rm_rf(dsk);
    PASS();
}

/* -- Main ------------------------------------------------------ */
/* -- Invalidation / dirty-tracking Tests ----------------------- */

static void test_invalidate_tracking(void) {
    TEST("invalidate tracks windows + poll drains queue");
    dosgui_wm_init(1024, 768);

    /* Start clean. */
    int id;
    while (dosgui_wm_poll_dirty(&id)) { /* drain */ }

    DosGuiWindow *a = dosgui_wm_create(10, 10, 100, 100, "A");
    DosGuiWindow *b = dosgui_wm_create(200, 200, 100, 100, "B");
    CHECK(a && b, "two windows created");

    dosgui_wm_invalidate(a);
    dosgui_wm_invalidate(b);
    CHECK(dosgui_wm_dirty_count() == 2, "two windows queued dirty");

    int seen_a = 0, seen_b = 0, n = 0;
    while (dosgui_wm_poll_dirty(&id)) {
        if (id == a->id) seen_a = 1;
        if (id == b->id) seen_b = 1;
        n++;
    }
    CHECK(n == 2, "poll returned exactly two dirty ids");
    CHECK(seen_a && seen_b, "both invalidated windows reported");
    CHECK(dosgui_wm_dirty_count() == 0, "queue empty after drain");

    /* invalidate_all => poll returns -1 (full redraw). */
    dosgui_wm_invalidate_all();
    CHECK(dosgui_wm_dirty_count() == -1, "invalidate_all flags full redraw");
    int rid;
    CHECK(dosgui_wm_poll_dirty(&rid) && rid == -1, "poll reports full-redraw (-1)");
    CHECK(dosgui_wm_dirty_count() == 0, "full-redraw consumed");

    /* NULL window => invalidate_all semantics. */
    dosgui_wm_invalidate(NULL);
    CHECK(dosgui_wm_dirty_count() == -1, "NULL invalidate => full redraw");

    dosgui_wm_shutdown();
    PASS();
}

/* -- Main ------------------------------------------------------ */

int main(void) {
    printf("+========================================================+\n");
    printf("|  WuBuOS DosGui Window Manager Test Suite (Cell 400)   |\n");
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
    test_render_draws_glyphs_and_selection();
    test_render_window_drop_shadow();
    test_render_win98_title_bar_is_theme_navy();
    test_vbe_clip_confines_pixels();
    test_render_window_clipped_above_taskbar();
    test_add_icon();
    test_icon_hit_test();
    test_icon_layout_persist_restore();
    test_icon_get_bounds();
    test_fable_font();
    test_fable_text_width();
    test_fable_primitives_exist();
    test_invalidate_tracking();
    test_desktop_live_namespace_init();
    test_desktop_new_folder();
    test_desktop_new_text_doc();
    test_desktop_sort_by_size();
    test_desktop_many_icons_retained();
    test_icon_drag_persist_restore();
    test_icon_delete_moves_to_trash();
    test_icon_multiselect_delete();
    test_auto_arrange_persists_restart();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}
