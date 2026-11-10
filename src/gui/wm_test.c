/*
 * wm_test.c  --  WuBuOS DosGui Window Manager Test Suite
 *
 * Cell 400: Fable Windowing Agent test suite.
 * Tests window creation, z-order, drag, focus, taskbar, icons.
 */

#include "wm.h"
#include "wm_internal.h"
#include "wubu_trash.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"   /* MOD_SHIFT/CTRL/ALT/WIN */
#include "shell/wubu_osd.h"
#include "shell/startmenu.h"
#include "shell/wubu_screensaver.h"
#include "shell/wubu_lock_screen.h"
#include "../kernel/wubu_gesture.h"
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
    TEST("wm_init returns 0");
    int rc = wm_init(1024, 768);
    CHECK(rc == 0, "init should succeed");
    wm_shutdown();
    PASS();
}

static void test_init_shutdown_no_crash(void) {
    TEST("init + shutdown no crash");
    wm_init(800, 600);
    wm_shutdown();
    PASS();
}

/* -- Window Management Tests ----------------------------------- */

static void test_create_window(void) {
    TEST("wm_create returns non-NULL");
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(10, 20, 200, 150, "Test");
    CHECK(w != NULL, "window should be created");
    wm_shutdown();
    PASS();
}

static void test_create_multiple(void) {
    TEST("create 10 windows");
    wm_init(1024, 768);
    int ok = 1;
    for (int i = 0; i < 10; i++) {
        char title[32];
        snprintf(title, sizeof(title), "Win %d", i);
        DosGuiWindow *w = wm_create(10 + i * 30, 20 + i * 20, 200, 150, title);
        if (!w) { ok = 0; break; }
    }
    CHECK(ok, "all 10 windows created");
    CHECK(wm_window_count() == 10, "count == 10");
    wm_shutdown();
    PASS();
}

static void test_window_count(void) {
    TEST("window count tracks creates and destroys");
    wm_init(1024, 768);
    DosGuiWindow *w1 = wm_create(10, 20, 200, 150, "W1");
    DosGuiWindow *w2 = wm_create(50, 60, 200, 150, "W2");
    CHECK(wm_window_count() == 2, "count == 2 after creates");
    wm_destroy(w1);
    CHECK(wm_window_count() == 1, "count == 1 after destroy");
    wm_destroy(w2);
    CHECK(wm_window_count() == 0, "count == 0 after all destroyed");
    wm_shutdown();
    PASS();
}

static void test_focus(void) {
    TEST("focus tracks last created window");
    wm_init(1024, 768);
    DosGuiWindow *w1 = wm_create(10, 20, 200, 150, "W1");
    DosGuiWindow *w2 = wm_create(50, 60, 200, 150, "W2");
    DosGuiWindow *f = wm_get_focused();
    CHECK(f == w2, "focused should be w2 (last created)");
    wm_set_focus(w1);
    f = wm_get_focused();
    CHECK(f == w1, "after set_focus, focused should be w1");
    wm_shutdown();
    PASS();
}

static void test_find_by_id(void) {
    TEST("wm_find_by_id returns correct window");
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(10, 20, 200, 150, "Test");
    int id = w->id;
    DosGuiWindow *found = wm_find_by_id(id);
    CHECK(found == w, "find_by_id should return same pointer");
    DosGuiWindow *bad = wm_find_by_id(9999);
    CHECK(bad == NULL, "find_by_id with bad id returns NULL");
    wm_shutdown();
    PASS();
}

/* -- Input Tests ----------------------------------------------- */

static void test_mouse_click_no_crash(void) {
    TEST("mouse click on empty desktop no crash");
    wm_init(1024, 768);
    wm_handle_mouse(50, 50, 1, 1); /* down */
    wm_handle_mouse(50, 50, 0, 2); /* up */
    PASS();
    wm_shutdown();
}

static void test_mouse_drag_window(void) {
    TEST("mouse drag moves window");
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(100, 100, 200, 150, "DragTest");
    int orig_x = w->x, orig_y = w->y;
    /* Click on title bar */
    wm_handle_mouse(110, 105, 1, 1);
    /* Drag */
    wm_handle_mouse(150, 145, 1, 0);
    CHECK(w->x != orig_x || w->y != orig_y, "window moved");
    wm_handle_mouse(150, 145, 0, 2);
    PASS();
    wm_shutdown();
}

/* -- Taskbar Tests --------------------------------------------- */

static void test_taskbar_height(void) {
    TEST("taskbar height == 28");
    CHECK(taskbar_height() == 28, "should be 28");
    PASS();
}

static void test_render_no_crash(void) {
    TEST("render desktop with windows no crash");
    wm_init(1024, 768);
    wm_create(60, 60, 300, 200, "Welcome");
    wm_create(400, 100, 250, 180, "Notepad");
    uint32_t *fb = (uint32_t *)calloc(1024 * 768, 4);
    wm_render_desktop(fb, 1024, 768);
    free(fb);
    PASS();
    wm_shutdown();
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
    wm_init(256, 256);

    DosGuiWindow *w = wm_create(10, 10, 160, 100, "Navy Title");
    CHECK(w, "window created");
    if (w) w->on_draw = NULL;

    wm_render(NULL, 256, 256);

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

    wm_shutdown();
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
    wm_init(256, 256);

    int task_h = taskbar_height();   /* 28 for Win98 */
    int tb_top = 256 - task_h;

    /* Create a window whose bottom extends well past the taskbar.
     * x=90 keeps its columns right of the Start button (x 4..64), whose
     * 4-pane flag emblem legitimately contains a navy quadrant — the
     * navy detector must only see the WINDOW's pixels. */
    DosGuiWindow *w = wm_create(90, tb_top - 10, 120, 120, "Low Win");
    CHECK(w, "window created");
    if (w) w->on_draw = NULL;
    wm_render(NULL, 256, 256);

    /* No window-title (navy) pixel may appear inside the taskbar band.
     * Skip x < 68: the Start button (x 4..64) legitimately contains a navy
     * flag quadrant, so the navy detector must only probe the rest of the
     * band (where the test window at x=90..210 would bleed if unclipped). */
    int bleed = 0;
    for (int y = tb_top; y < 256; y++)
        for (int x = 68; x < 256; x++) {
            uint32_t c = vbe_get_pixel(x, y);
            int r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF;
            if (b > 80 && r < 60 && g < 60) bleed++;   /* navy title in band */
        }
    CHECK(bleed == 0, "no window-title pixel painted in the taskbar band");

    wm_shutdown();
    PASS();
}

/* Regression for the Chicago->XP desktop polish: icons must draw a real
 * glyph (not a flat box) and a selected icon must show a selection rect. */
static void test_render_draws_glyphs_and_selection(void) {
    TEST("render draws icon glyphs + selection highlight");
    vbe_init(256, 256);
    wm_init(256, 256);

    int folder = icon_add_ex("Projects", DESK_ICON_FOLDER, NULL, 0, 0, 0, NULL);
    int file   = icon_add_ex("Notes.txt", DESK_ICON_FILE, NULL, 1, 0, 0, NULL);
    DosGuiIcon *fi = icon_get(folder);
    DosGuiIcon *li = icon_get(file);
    CHECK(fi && li, "both icons added");
    if (fi) fi->selected = true;   /* select the folder icon */

    wm_render(NULL, 256, 256);

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

    wm_shutdown();
    PASS();
}

/* Regression for the XP Luna drop-shadow: a window must cast a soft blended
 * shadow to its bottom-right under Luna themes (and none under Win98). */
static void test_render_window_drop_shadow(void) {
    TEST("XP window casts a soft drop-shadow");
    wubu_theme_set(THEME_XP_LUNA_BLUE);
    vbe_init(256, 256);
    wm_init(256, 256);
    wm_create(10, 10, 120, 90, "Shadowed");
    wm_render(NULL, 256, 256);

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

    wm_shutdown();
    PASS();
}

/* -- Icon Tests ------------------------------------------------ */

static void test_add_icon(void) {
    TEST("icon_add returns valid index");
    wm_init(1024, 768);
    int idx = icon_add("My Computer", 0, 0, NULL);
    CHECK(idx == 0, "first icon index == 0");
    int idx2 = icon_add("Temple REPL", 0, 1, NULL);
    CHECK(idx2 == 1, "second icon index == 1");
    wm_shutdown();
    PASS();
}

static void test_systray_lifecycle(void) {
    TEST("sysTray: real add/color/update_color/remove round-trip");
    wm_init(1024, 768);

    /* Add an icon through the REAL implementation (wm_systray.c). */
    int id = systray_add("archd", 0x00AA00, NULL, NULL);
    CHECK(id == 0, "first systray icon id == 0");
    CHECK(systray_color("archd") == 0x00AA00, "color matches add");

    /* Update its color in place. */
    CHECK(systray_update_color("archd", 0x0000AA) == 0, "update succeeds");
    CHECK(systray_color("archd") == 0x0000AA, "color reflects update");

    /* Unknown name: color returns 0, update errors. */
    CHECK(systray_color("ghost") == 0, "unknown icon color == 0");
    CHECK(systray_update_color("ghost", 0x0000AA) == -1, "update unknown errors");

    /* Remove it. */
    systray_remove("archd");
    CHECK(systray_color("archd") == 0, "removed icon no longer queryable");

    wm_shutdown();
    PASS();
}

static void test_window_state_transitions(void) {
    TEST("window state: resize/move clamps + maximize/minimize/restore");
    int sw = 1024, sh = 768;
    int tb = taskbar_height();
    wm_init(sw, sh);

    DosGuiWindow *w = wm_create(10, 10, 200, 100, "State");
    CHECK(w != NULL, "window created");

    /* resize honors the request and clamps to screen minus taskbar. */
    wm_resize(w, 300, 200);
    CHECK(w->w == 300 && w->h == 200, "resize sets geometry");
    wm_resize(w, 99999, 99999);
    CHECK(w->w == sw, "resize clamps width to screen");
    CHECK(w->h == sh - tb, "resize clamps height to above-taskbar");

    /* move clamps negative offsets to 0 and stays on-screen. */
    wm_move(w, -50, -20);
    CHECK(w->x == 0 && w->y == 0, "move clamps negatives to 0");
    wm_move(w, 5000, 8000);
    CHECK(w->x == sw - w->w, "move keeps window on-screen (x)");
    CHECK(w->y == sh - tb - w->h, "move keeps window above taskbar (y)");

    /* maximize: full-screen above taskbar, remembers restore rect. */
    wm_resize(w, 400, 300);   /* small rect first so move isn't clamped */
    wm_move(w, 40, 40);
    wm_maximize(w);
    CHECK(wm_is_maximized(w), "window is maximized");
    CHECK(w->x == 0 && w->y == 0, "maximize moves to origin");
    CHECK(w->w == sw && w->h == sh - tb, "maximize fills above taskbar");

    /* restore returns to the remembered rect and clears maximized. */
    wm_restore(w);
    CHECK(!wm_is_maximized(w), "restore clears maximized");
    CHECK(w->x == 40 && w->y == 40, "restore restores x/y");
    CHECK(w->w == 400 && w->h == 300, "restore restores w/h");

    /* minimize/restore toggles the flag; render skips minimized windows. */
    wm_minimize(w);
    CHECK(wm_is_minimized(w), "window is minimized");
    wm_restore(w);
    CHECK(!wm_is_minimized(w), "restore clears minimized");

    wm_shutdown();
    PASS();
}

static void test_icon_hit_test(void) {
    TEST("icon hit test finds correct icon");
    wm_init(1024, 768);
    icon_add("Icon0", 0, 0, NULL);
    icon_add("Icon1", 1, 0, NULL);
    /* Icon 0 at (20, 20), size 32 */
    int hit = icon_hit_test(30, 30);
    CHECK(hit == 0, "should hit icon 0");
    /* Icon 1 at (20 + 1*(32+8), 20) = (60, 20) */
    hit = icon_hit_test(70, 30);
    CHECK(hit == 1, "should hit icon 1");
    /* Miss */
    hit = icon_hit_test(500, 500);
    CHECK(hit == -1, "should miss");
    wm_shutdown();
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
    wm_init(1024, 768);

    int a = icon_add("PersistA", 0, 0, NULL);
    int b = icon_add("PersistB", 1, 0, NULL);
    CHECK(a >= 0 && b >= 0, "two icons added");

    /* Move B to a different grid cell (simulating a drag). */
    DosGuiIcon *ic = icon_get(b);
    CHECK(ic != NULL, "icon_get returns icon B");
    ic->grid_x = 3; ic->grid_y = 5;
    ic->x = 20 + 3 * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    ic->y = 20 + 5 * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);

    wm_save_icon_layout();          /* persist */
    const WubuSettings *s = wubu_settings_get();
    CHECK(s->theme.icon_layout_count >= 2, "settings recorded >= 2 layout entries");

    /* Simulate a fresh boot: new WM, re-add icons at default positions. */
    wm_shutdown();
    wubu_settings_init();                  /* reload from disk */
    wm_init(1024, 768);
    icon_add("PersistA", 0, 0, NULL);
    icon_add("PersistB", 1, 0, NULL);
    wm_restore_icon_layout();       /* restore persisted positions */

    DosGuiIcon *rb = icon_get(1);
    CHECK(rb != NULL, "restored icon B present");
    CHECK(rb->grid_x == 3 && rb->grid_y == 5, "icon B restored to persisted grid (3,5)");
    CHECK(rb->x == 20 + 3 * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP), "icon B x recomputed from grid");

    wm_shutdown();
    PASS();
}

static void test_icon_get_bounds(void) {
    TEST("icon_get returns NULL out of bounds");
    wm_init(800, 600);
    icon_add("X", 0, 0, NULL);
    CHECK(icon_get(0) != NULL, "icon_get(0) valid");
    CHECK(icon_get(99) == NULL, "icon_get(99) NULL");
    CHECK(icon_get(-1) == NULL, "icon_get(-1) NULL");
    wm_shutdown();
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
    wm_init(1024, 768);
    wm_refresh_desktop();

    int found_alpha = 0, found_bravo = 0;
    for (int i = 0; i < wm_get_icon_count(); i++) {
        DosGuiIcon *ic = icon_get(i);
        if (!ic || !ic->alive) continue;
        if (strcasecmp(ic->name, "Alpha") == 0) found_alpha = 1;
        if (strcasecmp(ic->name, "Bravo") == 0) found_bravo = 1;
    }
    CHECK(found_alpha, "Alpha.desktop surfaced as desktop icon");
    CHECK(found_bravo, "Bravo.desktop surfaced as desktop icon");

    wm_shutdown();
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
    wm_init(1024, 768);
    wm_refresh_desktop();
    int before = wm_get_icon_count();

    int rc = wm_new_folder();
    CHECK(rc == 0, "wm_new_folder returns 0");
    CHECK(access(desk, 0) == 0 && access(desk, 0) == 0, "folder exists");
    /* The new folder must be a real directory on disk. */
    char folder[600];
    snprintf(folder, sizeof(folder), "%s/New Folder", desk);
    struct stat st;
    CHECK(stat(folder, &st) == 0 && S_ISDIR(st.st_mode), "New Folder is a real directory");
    CHECK(wm_get_icon_count() > before, "new icon added after folder creation");

    wm_shutdown();
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
    wm_init(1024, 768);
    wm_refresh_desktop();
    int before = wm_get_icon_count();

    int rc = wm_new_text_doc();
    CHECK(rc == 0, "wm_new_text_doc returns 0");
    char doc[600];
    snprintf(doc, sizeof(doc), "%s/New Text Document.txt", desk);
    struct stat st;
    CHECK(stat(doc, &st) == 0 && S_ISREG(st.st_mode), "New Text Document is a real file");
    CHECK(wm_get_icon_count() > before, "new icon added after document creation");

    wm_shutdown();
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
    wm_init(1024, 768);
    wm_refresh_desktop();

    /* Force Name sort first (stable), then Size sort. */
    wm_sort_icons(DOSGUI_SORT_NAME);
    wm_sort_icons(DOSGUI_SORT_SIZE);

    /* Find the two icons and confirm Big precedes Small (ascending size). */
    int small_idx = -1, big_idx = -1;
    for (int i = 0; i < wm_get_icon_count(); i++) {
        DosGuiIcon *ic = icon_get(i);
        if (!ic || !ic->alive) continue;
        if (strcasecmp(ic->name, "Big") == 0) big_idx = i;
        if (strcasecmp(ic->name, "Small") == 0) small_idx = i;
    }
    CHECK(small_idx >= 0 && big_idx >= 0, "both icons present after sort");
    CHECK(big_idx < small_idx, "Big (larger) icon ordered before Small by size");

    wm_shutdown();
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
    wm_init(1024, 768);
    wm_refresh_desktop();

    int found = 0;
    for (int i = 0; i < wm_get_icon_count(); i++) {
        DosGuiIcon *ic = icon_get(i);
        if (ic && ic->alive) found++;
    }
    CHECK(found >= 20, "at least 20 of 30 desktop entries surfaced as icons");

    wm_shutdown();
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
    wm_init(1024, 768);
    wm_refresh_desktop();

    /* Find the icon and simulate a drag: move it to grid (3,4), then persist. */
    int idx = -1;
    for (int i = 0; i < wm_get_icon_count(); i++) {
        DosGuiIcon *ic = icon_get(i);
        if (ic && ic->alive && strcasecmp(ic->name, "Pin") == 0) { idx = i; break; }
    }
    CHECK(idx >= 0, "Pin icon present");
    DosGuiIcon *ic = icon_get(idx);
    ic->grid_x = 3; ic->grid_y = 4;
    ic->x = 20 + 3 * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    ic->y = 20 + 4 * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    wm_save_icon_layout();   /* what the drag-end handler now calls */

    /* Simulate restart: re-init (desktop.c boot path re-enumerates ~/Desktop),
     * then restore persisted positions -- matching real boot flow. */
    wm_shutdown();
    wubu_settings_init();
    wm_init(1024, 768);
    wm_refresh_desktop();   /* desktop_init() does this on boot */
    wm_restore_icon_layout();

    int ridx = -1;
    for (int i = 0; i < wm_get_icon_count(); i++) {
        DosGuiIcon *rc = icon_get(i);
        if (rc && rc->alive && strcasecmp(rc->name, "Pin") == 0) { ridx = i; break; }
    }
    CHECK(ridx >= 0, "Pin icon present after re-init");
    DosGuiIcon *rc = icon_get(ridx);
    CHECK(rc->grid_x == 3 && rc->grid_y == 4, "persisted grid (3,4) restored");

    wm_shutdown();
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
    wm_init(1024, 768);
    wm_refresh_desktop();

    int before = wm_get_icon_count();
    int idx = -1;
    for (int i = 0; i < before; i++) {
        DosGuiIcon *ic = icon_get(i);
        if (ic && ic->alive && strcasecmp(ic->name, "Note") == 0) { idx = i; break; }
    }
    CHECK(idx >= 0, "Note icon present");
    DosGuiIcon *ic = icon_get(idx);
    CHECK(ic->target[0] != '\0', "icon carries real fs target path");

    /* Simulate confirm-delete of the single icon (replicates dialog_delete_on_key). */
    if (ic->target[0]) wubu_trash_move(ic->target);
    ic->alive = false;
    wm_compact_icons();

    CHECK(wm_get_icon_count() == before - 1, "icon count dropped by 1 after delete");
    /* Underlying file must be gone from ~/Desktop (moved to trash). */
    struct stat st;
    CHECK(stat(p, &st) != 0, "underlying .desktop removed from ~/Desktop");

    wm_shutdown();
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
    wm_init(1024, 768);
    wm_refresh_desktop();

    int before = wm_get_icon_count();
    /* Select every live icon. */
    int sel = 0;
    for (int i = 0; i < before; i++) {
        DosGuiIcon *ic = icon_get(i);
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
    wm_compact_icons();

    CHECK(wm_get_icon_count() == before - sel, "all selected icons removed + compacted");

    wm_shutdown();
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
    wm_init(1024, 768);
    wm_set_auto_arrange(true);
    const WubuSettings *s = wubu_settings_get();
    CHECK(s->theme.auto_arrange == true, "settings recorded auto-arrange ON");

    /* Simulate restart: reload settings, re-init WM, then run the EXACT
     * boot-path restore line from desktop_init():
     *   wm_set_auto_arrange(s->theme.auto_arrange);
     * (calling the full desktop_init() would pull in the DOS emulator
     * subsystem; the restore logic is a single setter call, tested here.) */
    wm_shutdown();
    wubu_settings_init();
    wm_init(1024, 768);
    wm_set_auto_arrange(wubu_settings_get()->theme.auto_arrange);
    CHECK(wm_get_auto_arrange() == true, "auto-arrange ON after re-init (boot path)");

    wm_shutdown();
    rm_rf(dsk);
    PASS();
}

/* -- Main ------------------------------------------------------ */
/* -- Invalidation / dirty-tracking Tests ----------------------- */

static void test_invalidate_tracking(void) {
    TEST("invalidate tracks windows + poll drains queue");
    wm_init(1024, 768);

    /* Start clean. */
    int id;
    while (wm_poll_dirty(&id)) { /* drain */ }

    DosGuiWindow *a = wm_create(10, 10, 100, 100, "A");
    DosGuiWindow *b = wm_create(200, 200, 100, 100, "B");
    CHECK(a && b, "two windows created");

    wm_invalidate(a);
    wm_invalidate(b);
    CHECK(wm_dirty_count() == 2, "two windows queued dirty");

    int seen_a = 0, seen_b = 0, n = 0;
    while (wm_poll_dirty(&id)) {
        if (id == a->id) seen_a = 1;
        if (id == b->id) seen_b = 1;
        n++;
    }
    CHECK(n == 2, "poll returned exactly two dirty ids");
    CHECK(seen_a && seen_b, "both invalidated windows reported");
    CHECK(wm_dirty_count() == 0, "queue empty after drain");

    /* invalidate_all => poll returns -1 (full redraw). */
    wm_invalidate_all();
    CHECK(wm_dirty_count() == -1, "invalidate_all flags full redraw");
    int rid;
    CHECK(wm_poll_dirty(&rid) && rid == -1, "poll reports full-redraw (-1)");
    CHECK(wm_dirty_count() == 0, "full-redraw consumed");

    /* NULL window => invalidate_all semantics. */
    wm_invalidate(NULL);
    CHECK(wm_dirty_count() == -1, "NULL invalidate => full redraw");

    wm_shutdown();
    PASS();
}

/* -- Main ------------------------------------------------------ */


/* -- Snap Layouts (keyboard) ------------------------------------ */

static void test_snap_zones(void) {
    TEST("snap_window_to_zone places a window in the named zone");
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(10, 20, 200, 150, "Snap");
    CHECK(w != NULL, "window created");
    if (!w) { wm_shutdown(); return; }

    int sh = 768 - taskbar_height_dynamic();

    snap_window_to_zone(w, 0);
    CHECK(w->x == 0 && w->w == 512 && w->h == sh, "zone 0 = left half");

    snap_window_to_zone(w, 1);
    CHECK(w->x == 512 && w->w == 512 && w->h == sh, "zone 1 = right half");

    snap_window_to_zone(w, 2);
    CHECK(w->x == 0 && w->y == 0 && w->w == 512 && w->h == sh / 2,
          "zone 2 = top-left quarter");

    snap_window_to_zone(w, 5);
    CHECK(w->x == 512 && w->y == sh / 2, "zone 5 = bottom-right quarter");

    wm_shutdown();
    PASS();
}

static void test_win_arrow_hotkey(void) {
    TEST("Win+Arrow snaps the focused window (Snap Layouts hotkey)");
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(10, 20, 200, 150, "Hotkey");
    CHECK(w != NULL, "window created");
    if (!w) { wm_shutdown(); return; }
    wm_set_focus(w);

    wm_handle_key(0xE04B, MOD_WIN);
    CHECK(w->x == 0 && w->w == 512, "Win+Left snapped to the left half");

    wm_handle_key(0xE04D, MOD_WIN);
    CHECK(w->x == 512, "Win+Right snapped to the right half");

    wm_handle_key(0xE048, MOD_WIN);
    CHECK((w->flags & DOSGUI_WIN_MAXIMIZED) != 0, "Win+Up maximized");

    wm_shutdown();
    PASS();
}

static void test_modifier_bits_match_input_h(void) {
    TEST("the WM drives shortcuts with input.h modifier bits (Alt+Tab reachable)");
    CHECK(MOD_SHIFT == 0x01 && MOD_CTRL == 0x02 && MOD_ALT == 0x04 && MOD_WIN == 0x08,
          "input.h modifier bits are what the WM must use");

    wm_init(1024, 768);
    DosGuiWindow *a = wm_create(10, 20, 200, 150, "A");
    DosGuiWindow *b = wm_create(400, 20, 200, 150, "B");
    CHECK(a && b, "two windows");
    if (a && b) {
        wm_set_focus(a);
        wm_handle_key(0x09, MOD_ALT);       /* Alt+Tab */
        CHECK(wm_get_focused() == b, "Alt+Tab moved focus to the other window");
    }
    wm_shutdown();
    PASS();
}


static void test_show_desktop_hotkey(void) {
    TEST("Win+D minimizes every window and restores them");
    wm_init(1024, 768);
    DosGuiWindow *a = wm_create(10, 20, 200, 150, "A");
    DosGuiWindow *b = wm_create(400, 20, 200, 150, "B");
    CHECK(a && b, "two windows");
    if (!a || !b) { wm_shutdown(); return; }

    wm_handle_key(0x20, MOD_WIN);       /* Win+D */
    CHECK((a->flags & DOSGUI_WIN_MINIMIZED) && (b->flags & DOSGUI_WIN_MINIMIZED),
          "both windows minimized");

    wm_handle_key(0x20, MOD_WIN);       /* Win+D again */
    CHECK(!(a->flags & DOSGUI_WIN_MINIMIZED) && !(b->flags & DOSGUI_WIN_MINIMIZED),
          "both windows restored");

    wm_shutdown();
    PASS();
}


static void test_add_desktop_and_overview(void) {
    TEST("Win+Ctrl+D creates a desktop; Win+Tab opens the overview grid");
    wm_init(1024, 768);
    CHECK(wm_get_current_desktop() == 0, "starts on desktop 0");

    wm_handle_key(0x20, MOD_WIN | MOD_CTRL);   /* Win+Ctrl+D */
    wm_set_current_desktop(1);
    CHECK(wm_get_current_desktop() == 1, "the new desktop is selectable");

    wm_handle_key(0x09, MOD_WIN);              /* Win+Tab */
    CHECK(wm_overview_is_open(), "overview opened");

    int x, y, w, h;
    wm_overview_tile_rect(1024, 768, 0, &x, &y, &w, &h);
    CHECK(wm_overview_handle_click(x + w / 2, y + h / 2, 1024, 768),
          "a tile click is consumed");
    CHECK(!wm_overview_is_open(), "overview closed after picking a tile");
    CHECK(wm_get_current_desktop() == 0, "switched to the clicked desktop");

    wm_shutdown();
    PASS();
}


static void test_osd_levels_and_hotkey(void) {
    TEST("volume/brightness clamp, the OSD shows, Ctrl+Alt+Up changes volume");
    wubu_osd_set_volume(1.5f);
    CHECK(wubu_osd_volume() == 1.0f, "volume clamps at 1.0");
    wubu_osd_set_volume(-0.5f);
    CHECK(wubu_osd_volume() == 0.0f, "volume clamps at 0.0");
    wubu_osd_set_volume(0.5f);
    CHECK(wubu_osd_volume() == 0.5f, "volume stores 0.5");

    wubu_osd_set_muted(true);
    CHECK(wubu_osd_muted(), "mute flag is stored");
    wubu_osd_set_volume(0.7f);
    CHECK(!wubu_osd_muted(), "raising the volume clears mute");

    wubu_osd_set_brightness(0.05f);
    CHECK(wubu_osd_brightness() == 0.2f, "brightness clamps at 0.2");
    wubu_osd_set_brightness(0.6f);
    CHECK(wubu_osd_brightness() == 0.6f, "brightness stores 0.6");

    wubu_osd_notify();
    CHECK(wubu_osd_is_visible(), "the OSD is visible after a change");
    for (int i = 0; i < 100; i++) wubu_osd_tick();
    CHECK(!wubu_osd_is_visible(), "the OSD fades out");

    /* The hotkey path drives the same state. */
    wm_init(1024, 768);
    wubu_osd_set_volume(0.50f);
    wm_handle_key(0xE048, MOD_CTRL | MOD_ALT);   /* Ctrl+Alt+Up */
    CHECK(wubu_osd_volume() > 0.50f, "Ctrl+Alt+Up raised the volume");
    CHECK(wubu_osd_is_visible(), "the hotkey showed the OSD");
    wm_handle_key(0xE050, MOD_CTRL | MOD_ALT);   /* Ctrl+Alt+Down */
    wm_handle_key(0xE050, MOD_CTRL | MOD_ALT);
    CHECK(wubu_osd_volume() < 0.60f, "Ctrl+Alt+Down lowered the volume");
    wm_shutdown();
    PASS();
}


static void test_quick_settings_panel(void) {
    TEST("Win+A opens quick settings; clicking the volume row steps it up");
    wm_init(1024, 768);
    CHECK(!quick_settings_is_open(), "panel starts closed");

    wm_handle_key(0x1E, MOD_WIN);      /* Win+A */
    CHECK(quick_settings_is_open(), "Win+A opened the panel");

    int x, y, w, h;
    quick_settings_rect(1024, 768, &x, &y, &w, &h);
    CHECK(x >= 0 && y >= 0 && w > 0 && h > 0, "the panel has a real rect");

    /* Rows are 30px with a 12px pad; volume is row 3. */
    wubu_osd_set_volume(0.50f);
    int vy = y + 12 + 3 * 30 + 8;
    quick_settings_handle_click(x + 4, vy, 1024, 768);
    CHECK(wubu_osd_volume() > 0.50f, "clicking the volume row raised it");
    CHECK(wubu_osd_is_visible(), "the panel's change showed the OSD");

    /* Clicking outside dismisses. */
    quick_settings_handle_click(2, 2, 1024, 768);
    CHECK(!quick_settings_is_open(), "an outside click closes the panel");

    wm_shutdown();
    PASS();
}


static void test_screensaver(void) {
    TEST("the screen saver engages after the idle timeout and wakes on input");
    wubu_screensaver_set_timeout_ms(100);
    wubu_screensaver_activity();
    CHECK(!wubu_screensaver_active(), "not active right after activity");

    wubu_screensaver_tick(60);
    CHECK(!wubu_screensaver_active(), "still not active under the timeout");
    wubu_screensaver_tick(60);
    CHECK(wubu_screensaver_active(), "engaged once the timeout elapsed");

    wm_init(1024, 768);
    wm_handle_key(0x1E, 0);                     /* any key */
    CHECK(!wubu_screensaver_active(), "a keypress woke the saver");

    wubu_screensaver_tick(200);
    CHECK(wubu_screensaver_active(), "re-engages after another idle period");

    wm_handle_mouse(10, 10, 1, 1);
    CHECK(!wubu_screensaver_active(), "a mouse move woke the saver");

    wm_shutdown();
    PASS();
}


static void test_lock_screen(void) {
    TEST("Win+L locks the session; Enter unlocks; input is swallowed while locked");
    wm_init(1024, 768);
    CHECK(!wubu_lock_screen_is_locked(), "starts unlocked");

    wm_handle_key(0x26, MOD_WIN);              /* Win+L */
    CHECK(wubu_lock_screen_is_locked(), "Win+L locked the session");

    /* A normal key must not reach any window or unlock. */
    wm_handle_key('X', 0);
    CHECK(wubu_lock_screen_is_locked(), "a normal key neither unlocks nor reaches an app");

    /* The pointer is swallowed too. */
    wm_handle_mouse(512, 384, 1, 1);
    CHECK(wubu_lock_screen_is_locked(), "a click while locked does not unlock");

    wm_handle_key('\r', 0);                   /* Enter */
    CHECK(!wubu_lock_screen_is_locked(), "Enter unlocked the session");

    /* Win+L again locks, and Win+L unlocks. */
    wm_handle_key(0x26, MOD_WIN);
    CHECK(wubu_lock_screen_is_locked(), "Win+L locked again");
    wm_handle_key(0x26, MOD_WIN);
    CHECK(!wubu_lock_screen_is_locked(), "Win+L toggled back to unlocked");

    wm_shutdown();
    PASS();
}


static uint32_t g_gesture_t = 0;

/* Feed a three-finger swipe by (dx, dy) through the gesture engine. */
static void feed_swipe(int dx, int dy) {
    g_gesture_t += 1000;
    uint32_t t0 = g_gesture_t;
    for (int i = 0; i < 3; i++)
        wubu_gesture_contact(i, 400, 700 + i * 30, true, t0);
    g_gesture_t += 50;
    for (int i = 0; i < 3; i++)
        wubu_gesture_contact(i, 400 + dx, 700 + i * 30 + dy, true, g_gesture_t);
    g_gesture_t += 50;
    for (int i = 0; i < 3; i++)
        wubu_gesture_contact(i, 400 + dx, 700 + i * 30 + dy, false, g_gesture_t);
}

static void test_gesture_actions(void) {
    TEST("touchpad gestures drive the same actions as the hotkeys");
    wm_init(1024, 768);
    wubu_gesture_init();
    g_gesture_t = 0;

    CHECK(!wm_overview_is_open(), "overview starts closed");

    feed_swipe(0, -100);                 /* three-finger swipe up */
    wm_gesture_dispatch();
    CHECK(wm_overview_is_open(), "swipe up opened the overview");

    feed_swipe(0, -100);                 /* again: toggles closed */
    wm_gesture_dispatch();
    CHECK(!wm_overview_is_open(), "a second swipe up closed it");

    feed_swipe(100, 0);                  /* swipe right: next desktop */
    wm_gesture_dispatch();
    CHECK(wm_get_current_desktop() == 1, "swipe right advanced the desktop");

    feed_swipe(-100, 0);                 /* swipe left: previous desktop */
    wm_gesture_dispatch();
    CHECK(wm_get_current_desktop() == 0, "swipe left went back");

    wm_shutdown();
    PASS();
}



static void test_desktop_new_shortcut(void) {
    TEST("New Shortcut writes a real .desktop and surfaces a live icon");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_desktest_%d", getpid());
    setenv("XDG_DESKTOP_DIR", desk, 1);
    rm_rf(desk);
    mkdir(desk, 0755);

    wubu_settings_init();
    wm_init(1024, 768);
    wm_refresh_desktop();
    int before = wm_get_icon_count();

    /* The named mechanism: craft a shortcut for an app. */
    int rc = wm_write_desktop_shortcut("Calc", "wubu-app calculator");
    CHECK(rc == 0, "wm_write_desktop_shortcut returns 0");

    /* It must be a real .desktop on disk with the Exec line we asked for. */
    char fname[600];
    snprintf(fname, sizeof(fname), "%s/Calc.desktop", desk);
    struct stat st;
    CHECK(stat(fname, &st) == 0 && S_ISREG(st.st_mode), "shortcut .desktop file exists");
    char fdata[200];
    FILE *f = fopen(fname, "r");
    CHECK(f != NULL, "shortcut .desktop opens for read");
    int has_exec = 0, has_name = 0;
    if (f) {
        while (fgets(fdata, sizeof(fdata), f)) {
            if (strncmp(fdata, "Exec=wubu-app calculator", 24) == 0) has_exec = 1;
            if (strncmp(fdata, "Name=Calc", 9) == 0) has_name = 1;
        }
        fclose(f);
    }
    CHECK(has_exec, ".desktop carries Exec=wubu-app calculator");
    CHECK(has_name, ".desktop carries Name=Calc");

    /* No-exec name defaults to 'wubu-app <name>'. */
    rc = wm_write_desktop_shortcut("Notes", NULL);
    CHECK(rc == 0, "default-exec shortcut returns 0");
    char nf[600];
    snprintf(nf, sizeof(nf), "%s/Notes.desktop", desk);
    f = fopen(nf, "r");
    int has_def_exec = 0;
    if (f) {
        while (fgets(fdata, sizeof(fdata), f))
            if (strncmp(fdata, "Exec=wubu-app Notes", 19) == 0) has_def_exec = 1;
        fclose(f);
    }
    CHECK(has_def_exec, "default Exec=wubu-app Notes");

    wm_refresh_desktop();
    int found = 0;
    for (int i = 0; i < wm_get_icon_count(); i++) {
        DosGuiIcon *ic = icon_get(i);
        if (!ic || !ic->alive) continue;
        if (strcasecmp(ic->name, "Calc") == 0) found = 1;
    }
    CHECK(found, "shortcut surfaced as a live desktop icon");
    CHECK(wm_get_icon_count() >= before + 1, "icon count grew");
    CHECK(wm_write_desktop_shortcut("", NULL) == -1, "empty name rejected");

    wm_shutdown();
    rm_rf(desk);
    PASS();
}
/* -- Window-rim islands ----------------------------------------- */

static void test_rim_islands(void) {
    TEST("rim islands: add, settle, render, drag, clear");
    wm_init(1024, 768);
    wm_rim_clear();
    CHECK(wm_rim_count() == 0, "rim starts empty");

    CHECK(wm_rim_add(0.0f, 14.0f, 0x0080FF, "editor") == 0, "island added");
    CHECK(wm_rim_count() == 1, "count is one");

    /* The rim wraps the FOCUSED window's frame. */
    DosGuiWindow *w = wm_create(200, 150, 400, 250, "Rim");
    CHECK(w != NULL, "window created");
    wm_set_focus(w);
    for (int k = 0; k < 120; k++) wm_rim_tick();

    int x, y, r;
    CHECK(wm_rim_island_pos(0, &x, &y, &r), "island has a position");
    CHECK(x >= 198 && x <= 202 && y >= 148 && y <= 152,
          "island settles on the rim's top-left corner");

    /* Rendering with an island live must draw it, not just not crash. */
    vbe_init(1024, 768);
    wm_render(NULL, 1024, 768);
    int painted = 0;
    for (int yy = y - 14; yy <= y + 14; yy++)
        for (int xx = x - 14; xx <= x + 14; xx++)
            if (vbe_get_pixel(xx, yy) == 0x0080FF) painted++;
    CHECK(painted > 0, "the island is actually drawn");

    /* Press grabs it, a move drags it, the release drops it. */
    CHECK(wm_rim_handle_pointer(x, y, 1, 1), "press grabs the island");
    CHECK(wm_rim_handle_pointer(600, 275, 1, 0), "move drags the island");
    CHECK(wm_rim_handle_pointer(600, 275, 1, 2), "release drops the island");
    for (int k = 0; k < 120; k++) wm_rim_tick();

    int x2, y2, r2;
    CHECK(wm_rim_island_pos(0, &x2, &y2, &r2), "island still positioned");
    CHECK(x2 > 500 && y2 > 200, "island travelled to the dragged point");

    /* A press-release that does not travel is a click, and is consumed. */
    CHECK(wm_rim_handle_pointer(x2, y2, 1, 1), "press on the moved island");
    CHECK(wm_rim_handle_pointer(x2, y2, 1, 2), "release is a click");

    wm_rim_clear();
    CHECK(wm_rim_count() == 0, "clear empties the rim");
    wm_shutdown();
    PASS();
}

/* -- Screen-edge slime seam ------------------------------------- */

static void test_slime_seam(void) {
    TEST("slime seam: add, settle, store, release, drag, clear");
    wm_init(1024, 768);
    wm_slime_clear();
    CHECK(wm_slime_count() == 0, "seam starts empty");

    CHECK(wm_slime_add(0 /* top */, 0.25f, 14.0f, 0x00C05050, "editor") == 0,
          "blob added");
    CHECK(wm_slime_count() == 1, "count is one");

    for (int k = 0; k < 200; k++) wm_slime_tick();

    int x, y, r;
    CHECK(wm_slime_island_pos(0, &x, &y, &r), "blob has a position");
    CHECK(x > 240 && x < 272, "blob rests a quarter along the top edge");
    CHECK(y >= 17 && y <= 23, "released blob sits 20px from the edge");

    /* A click stores it: it shrinks and sinks into the seam. */
    CHECK(wm_slime_handle_pointer(x, y, 1, 1), "press grabs the blob");
    CHECK(wm_slime_handle_pointer(x, y, 1, 2), "release is a click");
    for (int k = 0; k < 200; k++) wm_slime_tick();

    int x2, y2, r2;
    CHECK(wm_slime_island_pos(0, &x2, &y2, &r2), "blob still positioned");
    CHECK(y2 < 12, "a stored blob sinks deeper into the seam");
    CHECK(r2 < r, "a stored blob shrinks");

    /* Clicking a stored blob pulls it back out. */
    CHECK(wm_slime_handle_pointer(x2, y2, 1, 1), "press the stored blob");
    CHECK(wm_slime_handle_pointer(x2, y2, 1, 2), "release pops it out");
    for (int k = 0; k < 200; k++) wm_slime_tick();
    CHECK(wm_slime_island_pos(0, &x2, &y2, &r2), "blob still positioned");
    CHECK(y2 >= 17, "the blob came back out to its released depth");

    /* Dragging moves it along the edge. */
    wm_slime_handle_pointer(x2, y2, 1, 1);
    wm_slime_handle_pointer(700, 20, 1, 0);
    wm_slime_handle_pointer(700, 20, 1, 2);
    for (int k = 0; k < 200; k++) wm_slime_tick();
    int x3, y3, r3;
    wm_slime_island_pos(0, &x3, &y3, &r3);
    CHECK(x3 > 600, "blob travelled along the top edge");

    wm_slime_clear();
    CHECK(wm_slime_count() == 0, "clear empties the seam");
    wm_shutdown();
    PASS();
}

/* -- Auto-hide taskbar (modern theme) --------------------------- */

static void test_taskbar_autohide(void) {
    TEST("taskbar auto-hide: hides, reveals, and clicks follow the bar");
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    wm_create(200, 150, 300, 200, "Bar");
    int th = taskbar_height_dynamic();
    int sh = g_dwm.screen_h;

    /* Reveal first, so the layout measured here is the on-screen one. */
    wm_handle_mouse(512, sh - 4, 1, 1);
    for (int k = 0; k < 40; k++) wm_render(NULL, 1024, 768);
    CHECK(g_dwm.taskbar_offset == 0, "the bar starts revealed");

    WmTaskLayout L;
    taskbar_modern_layout(1024, 768, &L);
    int sx = L.start_x + L.start_w / 2;
    int sy = L.start_y + L.start_h / 2;

    /* Pointer well above the bottom band: the bar slides away. */
    wm_handle_mouse(512, 300, 0, 0);
    for (int k = 0; k < 40; k++) wm_render(NULL, 1024, 768);
    CHECK(g_dwm.taskbar_offset >= th - 4, "the bar hides when the pointer leaves");

    /* A press where the bar used to be must not reach Start. */
    wm_handle_mouse(sx, sy, 1, 1);
    CHECK(!startmenu_is_open(), "a click in the hidden band does not open Start");
    wm_handle_mouse(sx, sy, 1, 2);

    /* Pointer back at the very bottom: the bar rises. */
    wm_handle_mouse(sx, sh - 4, 1, 1);
    for (int k = 0; k < 40; k++) wm_render(NULL, 1024, 768);
    CHECK(g_dwm.taskbar_offset == 0, "the bar rises when the pointer returns");

    /* Now the same press lands on Start. */
    wm_handle_mouse(sx, sy, 1, 1);
    CHECK(startmenu_is_open(), "Start opens once the bar is revealed");
    wm_handle_mouse(512, 300, 1, 1);   /* a press outside closes the menu */
    wm_handle_mouse(512, 300, 1, 2);

    /* A classic theme never auto-hides. */
    wubu_theme_set(THEME_WIN98_CLASSIC);
    wm_handle_mouse(512, 300, 0, 0);
    for (int k = 0; k < 40; k++) wm_render(NULL, 1024, 768);
    CHECK(g_dwm.taskbar_offset == 0, "a classic theme keeps the bar pinned");

    wm_shutdown();
    PASS();
}

/* -- Modern (Win11-class) floating bar --------------------------- */

static void test_modern_taskbar(void) {
    TEST("modern bar: floating, centred, and its buttons are hit-testable");
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(200, 150, 300, 200, "Modern");
    wm_set_focus(w);
    int th = taskbar_height_dynamic();
    int sh = g_dwm.screen_h;

    wm_handle_mouse(512, sh - 8, 0, 0);
    for (int k = 0; k < 40; k++) wm_render(NULL, 1024, 768);

    WmTaskLayout L;
    taskbar_modern_layout(1024, 768, &L);
    CHECK(L.bar_x > 0 && L.bar_x + L.bar_w < 1024, "the bar floats, centred");
    CHECK(L.bar_y < sh - th, "the bar is raised off the bottom edge");
    CHECK(L.n_slots >= 1, "the window has a task button");
    CHECK(L.slot[0].x > L.start_x + L.start_w, "the button sits right of Start");

    /* Clicking the focused task's button minimises it; again restores it. */
    int bx = L.slot[0].x + L.slot[0].w / 2;
    int by = L.slot[0].y + L.slot[0].h / 2;
    wm_handle_mouse(bx, by, 1, 1);
    CHECK(w->flags & DOSGUI_WIN_MINIMIZED, "clicking the focused task minimises it");
    wm_handle_mouse(bx, by, 1, 2);

    wm_handle_mouse(bx, by, 1, 1);
    CHECK(!(w->flags & DOSGUI_WIN_MINIMIZED), "clicking a minimised task restores it");
    wm_handle_mouse(bx, by, 1, 2);

    /* Empty bar space is swallowed by the bar, not passed to the desktop. */
    wm_handle_mouse(L.bar_x + 2, L.bar_y + L.bar_h / 2, 1, 1);
    CHECK(!g_dwm.lasso_active, "a click on empty bar space does not start a lasso");
    wm_handle_mouse(L.bar_x + 2, L.bar_y + L.bar_h / 2, 1, 2);

    wubu_theme_set(THEME_WIN98_CLASSIC);
    wm_shutdown();
    PASS();
}

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
    test_systray_lifecycle();
    test_window_state_transitions();
    test_snap_zones();
    test_win_arrow_hotkey();
    test_modifier_bits_match_input_h();
    test_show_desktop_hotkey();
    test_add_desktop_and_overview();
    test_osd_levels_and_hotkey();
    test_quick_settings_panel();
    test_screensaver();
    test_lock_screen();
    test_gesture_actions();
    test_icon_hit_test();
    test_icon_layout_persist_restore();
    test_icon_get_bounds();
    test_fable_font();
    test_fable_text_width();
    test_fable_primitives_exist();
    test_invalidate_tracking();



    test_desktop_live_namespace_init();
    test_desktop_new_folder();
    test_desktop_new_shortcut();
    test_desktop_new_text_doc();
    test_desktop_sort_by_size();
    test_desktop_many_icons_retained();
    test_icon_drag_persist_restore();
    test_icon_delete_moves_to_trash();
    test_icon_multiselect_delete();
    test_auto_arrange_persists_restart();
    test_rim_islands();
    test_slime_seam();
    test_taskbar_autohide();
    test_modern_taskbar();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}
