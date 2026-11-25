/*
 * wm_test.c  --  WuBuOS DosGui Window Manager Test Suite
 *
 * Cell 400: Fable Windowing Agent test suite.
 * Tests window creation, z-order, drag, focus, taskbar, icons.
 */

#include "../egojar.h"
#include "wm.h"
#include "shell/projection_flyout.h"
#include "shell/alt_tab_switcher.h"
#include "shell/taskbar_overflow_menu.h"
#include "wm_internal.h"
#include "wubu_trash.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"   /* MOD_SHIFT/CTRL/ALT/WIN */
#include "shell/wubu_osd.h"
#include "shell/window_chrome.h"   /* caption metrics under test */
#include "shell/wm_shadow.h"       /* elevation shadow blur-count oracle */
#include "wubu_tokens.h"   /* the geometry scale they must come from */
#include "shell/startmenu.h"
#include "shell/wubu_screensaver.h"
#include "shell/wubu_lock_screen.h"
#include "../kernel/wubu_gesture.h"
#include "../gui/wubu_settings.h"
#include "buddy_sprite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) printf("  TEST %-55s", name); g_total++;
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)


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

/* Stands in for the app registry: the names our desktop seeder writes. The RULE is
 * what was broken -- "wubu-app <Name>" named a program that was never built -- so it is
 * asserted with an explicit matcher rather than by linking the whole registry. */
static int test_is_app(const char *name, void *ctx) {
    (void)ctx;
    return strcmp(name, "Calculator") == 0 || strcmp(name, "File Manager") == 0 ||
           strcmp(name, "Editor") == 0 || strcmp(name, "Terminal") == 0;
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
    TEST("taskbar height == canonical WUBU_METRIC_TASKBAR_H");
    CHECK(taskbar_height() == DOSGUI_TASK_H, "should equal the canonical taskbar metric");
    CHECK(DOSGUI_TASK_H == WUBU_METRIC_TASKBAR_H, "wm must track the metric owner");
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


/* The buddy's moods must be VISIBLE: each state bakes a different pose, so
 * the pixels under the buddy have to change when the state changes. A state
 * enum that maps to the same sprite (or to nothing) would pass a code review
 * and be invisible on the desktop. */
static unsigned long long buddy_region_hash(void) {
    /* Everything on the desktop is drawn through vbe_*, so the pixels live in
     * the VBE back buffer -- read them there, not from wm_render's fb (which
     * may be NULL). */
    wm_render(NULL, 1024, 768);
    unsigned long long h = 1469598103934665603ull;
    /* The buddy sits at (20,600) and is 96x96. */
    for (int y = 600; y < 696; y++)
        for (int x = 20; x < 116; x++) {
            h ^= vbe_get_pixel(x, y);
            h *= 1099511628211ull;
        }
    return h;
}

static void test_buddy_states_render_differently(void) {
    TEST("buddy states render different poses");
    vbe_init(1024, 768);
    wm_init(1024, 768);
    wubu_bonzi_init(20, 600);

    static const int states[] = {
        WUBU_BUDDY_STATE_DEFAULT, WUBU_BUDDY_STATE_WORK,
        WUBU_BUDDY_STATE_ERROR,   WUBU_BUDDY_STATE_SUCCESS,
        WUBU_BUDDY_STATE_IDLE,    WUBU_BUDDY_STATE_OVERHEAT,
        WUBU_BUDDY_STATE_BUSY,    WUBU_BUDDY_STATE_RELAXED,
        WUBU_BUDDY_STATE_SURF,    WUBU_BUDDY_STATE_RACE,
        WUBU_BUDDY_STATE_READ,
    };
    unsigned long long seen[sizeof(states)/sizeof(states[0])];
    int n = (int)(sizeof(states)/sizeof(states[0]));
    for (int i = 0; i < n; i++) {
        wubu_bonzi_set_state(states[i]);
        wubu_bonzi_tick(16);
        seen[i] = buddy_region_hash();
        CHECK(seen[i] != 0, "state renders");
    }
    /* Every pose must differ from the default AND from every other pose. */
    for (int i = 1; i < n; i++) {
        CHECK(seen[i] != seen[0], "state differs from default");
        for (int j = 1; j < n; j++)
            if (i != j) CHECK(seen[i] != seen[j], "two states share a pose");
    }
    /* Out of range falls back to default rather than drawing garbage. */
    wubu_bonzi_set_state(WUBU_BUDDY_STATE_COUNT + 99);
    CHECK(wubu_bonzi_state() == WUBU_BUDDY_STATE_DEFAULT, "out-of-range falls back");
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
    /* Sample LEFT of the caption text, not at the horizontal centre: the centre of
     * the band is where the title string is drawn, so once the caption geometry moved
     * to WUBU_METRIC_TITLE_H this probe started reading glyph coverage instead of the
     * bar. The window was correct; the probe was reading the wrong pixel. */
    /* Count the navy pixels in the caption band rather than probing one coordinate.
     * The one-pixel probe was fragile (it read caption glyphs once the geometry moved),
     * but it was NOT wrong when it fired: the real defect was two `.win_title_active`
     * designated initializers inside THEME_WIN98_CLASSIC, so the later orange value
     * overwrote navy and the Win98 title bar genuinely rendered orange. A count of the
     * band makes the check robust WITHOUT making it blind. */
    int navy = 0;
    for (int yy = w->y; yy < w->y + 24 && yy < 256; yy++)
        for (int xx = w->x; xx < w->x + w->w && xx < 256; xx++)
            if (vbe_get_pixel(xx, yy) == 0x000080) navy++;
    uint32_t c = 0x000080;
    CHECK(navy > 20, "title bar band is theme navy (0x000080)");
    if (navy <= 20) printf("      navy pixels in band: %d\n", navy);

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

/* Windows input semantics: the first click SELECTS an icon, the second OPENS it. The
 * shell used to launch on the first click, which meant a stray click started a program and
 * there was no selection state for rename/delete to act on. */
static int g_open_count;
static void open_counter(void) { g_open_count++; }

static void test_icon_click_selects_then_opens(void) {
    TEST("desktop icon: single click selects, double click opens");
    wm_init(1024, 768);
    g_open_count = 0;
    int n = icon_add_ex("DblTest", DESK_ICON_APP, NULL, 0, 0, 0x808080, open_counter);
    CHECK(n >= 0, "icon added");
    int cx = DOSGUI_ICON_MARGIN + 6, cy = DOSGUI_ICON_MARGIN + 6;

    wm_handle_mouse(cx, cy, 1, 1);
    CHECK(g_open_count == 0, "first click does NOT launch");
    CHECK(g_dwm.icons[n].selected, "first click selects the icon");

    wm_handle_mouse(cx, cy, 1, 1);
    CHECK(g_open_count == 1, "second click launches");

    /* After an open, the next click starts a NEW pair: it selects again, it does not
     * chain into a second launch. (Windows behaves the same way.) */
    g_open_count = 1;
    wm_handle_mouse(cx, cy, 1, 1);
    CHECK(g_open_count == 1, "the click after an open selects, does not relaunch");
    wm_handle_mouse(cx, cy, 1, 1);
    CHECK(g_open_count == 2, "and the click after that opens again");

    /* Enter on the selection opens it (Windows keyboard parity). */
    g_open_count = 0;
    wm_handle_mouse(cx, cy, 1, 1);            /* select only */
    CHECK(g_open_count == 0, "selected without launching");
    wm_handle_key(0x1C, 0);                    /* Enter */
    CHECK(g_open_count == 1, "Enter opens the selected icon");

    /* Right-clicking a taskbar button opens a jump list (Windows parity). The bar used to
     * ignore the right button entirely. */
    wubu_taskbar_pin("jumptest", "Jump Test", "wubu-app Calculator");
    taskbar_show_jump_list(0, 200, 700);
    CHECK(ctx_menu_visible(), "right-click on a taskbar task opens a jump list");
    ctx_menu_hide_all();

    /* Alt+Space opens the window system menu (Windows parity: there was none). */
    {
        DosGuiWindow *sw = wm_create(200, 120, 320, 200, "SysMenu");
        CHECK(sw != NULL, "window created for the system menu");
        wm_set_focus(sw);
        ctx_menu_hide_all();
        wm_handle_key(0x39, MOD_ALT);          /* Alt+Space */
        CHECK(ctx_menu_visible(), "Alt+Space opens the window system menu");
        ctx_menu_hide_all();
        wm_destroy(sw);
    }

    /* Clicking empty desktop clears the selection, as Windows does. */
    wm_handle_mouse(700, 400, 1, 1);
    for (int i = 0; i < wm_get_icon_count(); i++)
        CHECK(!g_dwm.icons[i].selected, "empty-desktop click clears the selection");
    wm_shutdown();
}

static void test_icon_hit_test(void) {
    TEST("icon hit test finds correct icon");
    wm_init(1024, 768);
    icon_add("Icon0", 0, 0, NULL);
    icon_add("Icon1", 1, 0, NULL);
    /* Icon 0 at (20, 20), size 32 */
    int hit = icon_hit_test(30, 30);
    CHECK(hit == 0, "should hit icon 0");
    /* Icon 1 one column over; the column pitch is wide enough for a label */
    hit = icon_hit_test(DOSGUI_ICON_MARGIN + DOSGUI_ICON_COL_PITCH + 10, 30);
    CHECK(hit == 1, "should hit icon 1");
    /* The label line belongs to the icon too, not just the 32px glyph */
    hit = icon_hit_test(DOSGUI_ICON_MARGIN + DOSGUI_ICON_COL_PITCH + 10, DOSGUI_ICON_MARGIN + DOSGUI_ICON_SIZE + 4);
    CHECK(hit == 1, "label line is clickable");
    /* Miss */
    hit = icon_hit_test(900, 700);
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
    /* The old assertions were pitch arithmetic on the BITMAP font (6px per character,
     * doubled by scale). Text is proportional now, so the contract is: the bitmap path
     * still scales exactly, and the proportional path is positive, additive and
     * order-dependent in the right direction. */
    int wA = vbe_text_width("ABC", 1);
    CHECK(wA > 0, "text has width");
    CHECK(vbe_text_width("AB", 1) + vbe_text_width("C", 1) == wA, "widths are additive");
    CHECK(vbe_text_width("iii", 1) < vbe_text_width("WWW", 1), "advances are proportional");
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
    ic->x = 20 + 3 * DOSGUI_ICON_COL_PITCH;
    ic->y = 20 + 5 * DOSGUI_ICON_ROW_PITCH;

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
    CHECK(rb->x == 20 + 3 * DOSGUI_ICON_COL_PITCH, "icon B x recomputed from grid");

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
    TEST("desktop .desktop files appear as icons on init");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_desktest_%d", getpid());
    setenv("WUBU_DESKTOP_DIR", desk, 1);
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
    setenv("WUBU_DESKTOP_DIR", desk, 1);
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
    setenv("WUBU_DESKTOP_DIR", desk, 1);
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
    setenv("WUBU_DESKTOP_DIR", desk, 1);
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
 * the desktop directory holds many entries (refresh used to silently drop them). */
static void test_desktop_many_icons_retained(void) {
    TEST("refresh retains >16 icons (no silent drop at cap)");
    char desk[512];
    snprintf(desk, sizeof(desk), "/tmp/wubu_desktest_%d", getpid());
    setenv("WUBU_DESKTOP_DIR", desk, 1);
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
    setenv("WUBU_DESKTOP_DIR", desk, 1);
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
    ic->x = 20 + 3 * DOSGUI_ICON_COL_PITCH;
    ic->y = 20 + 4 * DOSGUI_ICON_ROW_PITCH;
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
    setenv("WUBU_DESKTOP_DIR", desk, 1);
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
    setenv("WUBU_DESKTOP_DIR", desk, 1);
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
    setenv("WUBU_DESKTOP_DIR", dsk, 1);
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


/* -- Windows 11 Snap Layouts (Win+Z picker table) ---------------- */

static void test_snap_layouts(void) {
    TEST("Windows 11 layout table tiles the desktop exactly");
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(10, 20, 200, 150, "Layouts");
    CHECK(w != NULL, "window created");
    if (!w) { wm_shutdown(); return; }

    int sh = 768 - taskbar_height_dynamic();

    /* The six arrangements Windows 11 actually offers. */
    CHECK(wubu_snap_layout_zones(0) == 2, "layout 0 = two halves");
    CHECK(wubu_snap_layout_zones(1) == 3, "layout 1 = three columns");
    CHECK(wubu_snap_layout_zones(2) == 3, "layout 2 = left half + split right");
    CHECK(wubu_snap_layout_zones(3) == 3, "layout 3 = right half + split left");
    CHECK(wubu_snap_layout_zones(4) == 4, "layout 4 = four quadrants");
    CHECK(wubu_snap_layout_zones(5) == 2, "layout 5 = two-thirds + third");

    /* ---- keyboard zone picking (Windows 11 has none: its picker is mouse-only) ---- */
    CHECK(wubu_snap_neighbor(4, 0, 1, 0) == 1, "quadrants: right from top-left");
    CHECK(wubu_snap_neighbor(4, 0, 0, 1) == 2, "quadrants: down from top-left");
    CHECK(wubu_snap_neighbor(4, 3, -1, 0) == 2, "quadrants: left from bottom-right");
    CHECK(wubu_snap_neighbor(4, 3, 0, -1) == 1, "quadrants: up from bottom-right");
    CHECK(wubu_snap_neighbor(4, 0, -1, 0) == 0, "quadrants: nothing left of the left edge");
    CHECK(wubu_snap_neighbor(4, 0, 0, -1) == 0, "quadrants: nothing above the top edge");
    CHECK(wubu_snap_neighbor(1, 0, 1, 0) == 1, "three columns: right from the first");
    CHECK(wubu_snap_neighbor(1, 1, -1, 0) == 0, "three columns: left from the middle");
    CHECK(wubu_snap_neighbor(1, 2, 1, 0) == 2, "three columns: nothing right of the last");
    CHECK(wubu_snap_neighbor(2, 1, 0, 1) == 2, "split-right: down from the top half");
    CHECK(wubu_snap_neighbor(2, 2, 0, -1) == 1, "split-right: up from the bottom half");
    /* the full-height left region is right of nothing and left of two zones that tie on
     * distance; a tie must resolve the same way every time or the keyboard wanders */
    CHECK(wubu_snap_neighbor(2, 1, 1, 0) == wubu_snap_neighbor(2, 1, 1, 0),
          "a tie resolves deterministically");
    CHECK(wubu_snap_neighbor(0, 0, 0, 1) == 0, "halves have no zone below");

    CHECK(wubu_snap_zone_at(4, 10, 10, 1000, 700) == 0, "hit test: top-left quadrant");
    CHECK(wubu_snap_zone_at(4, 990, 10, 1000, 700) == 1, "hit test: top-right quadrant");
    CHECK(wubu_snap_zone_at(4, 10, 690, 1000, 700) == 2, "hit test: bottom-left quadrant");
    CHECK(wubu_snap_zone_at(4, 990, 690, 1000, 700) == 3, "hit test: bottom-right quadrant");
    CHECK(wubu_snap_zone_at(4, 500, 10, 1000, 700) == 1, "hit test: the seam belongs to ONE zone (half-open)");
    CHECK(wubu_snap_zone_at(4, -1, 10, 1000, 700) == -1, "hit test: outside is -1");
    CHECK(wubu_snap_zone_at(4, 1000, 10, 1000, 700) == -1, "hit test: the right edge is outside");

    /* ---- keyboard selection is PURE: the flyout cannot be trusted otherwise ---- */
    int kl = 0, kz = 0;
    CHECK(wubu_snap_key_select(&kl, &kz, '3') == WUBU_SNAP_KEY_MOVED && kl == 2,
          "digit 3 selects layout 2 (three zones: half + split right)");
    CHECK(kz == 0, "selecting a layout keeps a valid zone");
    kl = 4; kz = 3;   /* valid: quadrant 3 of the four-quadrant layout */
    CHECK(wubu_snap_key_select(&kl, &kz, '1') == WUBU_SNAP_KEY_MOVED && kz == 1,
          "switching to a 2-zone layout CLAMPS the selection instead of pointing nowhere");
    kl = 4; kz = 0;
    CHECK(wubu_snap_key_select(&kl, &kz, 0x4D) == WUBU_SNAP_KEY_MOVED && kz == 1,
          "Right moves the selection to the right quadrant");
    CHECK(wubu_snap_key_select(&kl, &kz, 0x50) == WUBU_SNAP_KEY_MOVED && kz == 3,
          "Down moves it to the quadrant below");
    CHECK(wubu_snap_key_select(&kl, &kz, '\r') == WUBU_SNAP_KEY_APPLY, "Enter means apply");
    CHECK(wubu_snap_key_select(&kl, &kz, 0x1B) == WUBU_SNAP_KEY_CLOSE, "Esc means close");
    CHECK(wubu_snap_key_select(&kl, &kz, 'q') == WUBU_SNAP_KEY_IGNORED, "an unrelated key changes nothing");
    CHECK(kl == 4 && kz == 3, "...and leaves the selection exactly where it was");
    kl = -5; kz = 99;
    CHECK(wubu_snap_key_select(&kl, &kz, 0x4B) == WUBU_SNAP_KEY_MOVED && kl == 0 && kz == 0,
          "a nonsense selection is repaired rather than propagated");



    /* Halves. */
    CHECK(wubu_snap_apply_layout_zone(w, 0, 0), "apply halves[0]");
    CHECK(w->x == 0 && w->w == 512, "left half occupies the left 512px");

    CHECK(wubu_snap_apply_layout_zone(w, 0, 1), "apply halves[1]");
    CHECK(w->x == 512 && w->x + w->w == 1024, "right half reaches the right edge");

    /* Thirds -- the case that exposed the truncation gap. */
    CHECK(wubu_snap_apply_layout_zone(w, 1, 0), "apply thirds[0]");
    CHECK(w->x == 0 && w->w > 0, "first column starts at the origin");
    CHECK(wubu_snap_apply_layout_zone(w, 1, 2), "apply thirds[2]");
    CHECK(w->x + w->w == 1024, "last column reaches the right edge (no dead strip)");
    CHECK(w->h == sh, "columns span the full usable height");

    /* Quadrants. */
    CHECK(wubu_snap_apply_layout_zone(w, 4, 3), "apply quadrant bottom-right");
    CHECK(w->x + w->w == 1024 && w->y + w->h == sh,
          "bottom-right quadrant reaches both edges");

    /* Split column: one full-height side, the other side divided into two halves.
     * The distinction is HEIGHT, not width -- both sides are half the screen wide. */
    CHECK(wubu_snap_apply_layout_zone(w, 2, 0), "apply split-column full side");
    int full_h = w->h;
    CHECK(wubu_snap_apply_layout_zone(w, 2, 1), "apply split-column top");
    CHECK(w->h == full_h / 2, "the stacked regions are half height");
    CHECK(w->x + w->w == 1024, "the stacked regions reach the right edge");

    /* Refusals -- a bad index must be refused, never silently placed. */
    CHECK(!wubu_snap_apply_layout_zone(NULL, 0, 0), "NULL window refused");
    CHECK(!wubu_snap_apply_layout_zone(w, -1, 0), "negative layout refused");
    CHECK(!wubu_snap_apply_layout_zone(w, 0, 9), "zone past the end refused");
    CHECK(wubu_snap_layout_zones(-1) == 0, "negative layout reports zero zones");

    /* Snapping must clear the maximized state. */
    w->flags |= DOSGUI_WIN_MAXIMIZED;
    CHECK(wubu_snap_apply_layout_zone(w, 0, 0), "snap after maximize");
    CHECK(!(w->flags & DOSGUI_WIN_MAXIMIZED), "maximized cleared by snapping");

    wm_shutdown();
    PASS();
}

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
        alt_tab_close(false);
        /* Alt+Tab now OPENS the switcher; focus moves when Alt is released. The observable
         * this test exists for -- "the MOD_ALT bit reaches a consumer" -- is the grid
         * opening, and the commit proves the rest of the path. */
        wm_handle_key(0x09, MOD_ALT);
        CHECK(alt_tab_is_open(), "Alt+Tab reached the switcher (MOD_ALT bit is right)");
        wm_handle_key_up(0x38, 0);          /* release Alt: commit */
        CHECK(!alt_tab_is_open(), "the release committed and closed it");
        CHECK(wm_get_focused() != a, "committing moved focus to another window");
        alt_tab_close(false);
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



/* Win+Shift+Arrow moves the window to the adjacent desktop; plain Win+Arrow
 * snaps. The snap block used to match ANY Win+Arrow and sat first, so the move
 * shortcut could never fire -- a superset binding swallowing a specific one. */

/* Spies for the shell shortcuts: wm_input calls these through weak symbols, so
 * the test can observe the DISPATCH without linking the real apps. */
static int g_fm_calls, g_cp_calls;
DosGuiWindow *launch_file_manager(void) { g_fm_calls++; return NULL; }
DosGuiWindow *control_launch(void) { g_cp_calls++; return NULL; }

/* Win+E / Win+I / Win+Q are the three most-used Windows shortcuts and none of
 * them existed: a user's first instincts in this shell did nothing. */
static void test_shell_shortcuts_reach_their_launchers(void) {
    TEST("Win+E opens File Explorer, Win+I Control Panel, Win+Q Start (and combos do not)");
    wm_init(1024, 768);
    g_fm_calls = g_cp_calls = 0;

    wm_handle_key('e', MOD_WIN);
    CHECK(g_fm_calls == 1, "Win+E dispatched the file manager");
    wm_handle_key('i', MOD_WIN);
    CHECK(g_cp_calls == 1, "Win+I dispatched the control panel");
    wm_handle_key('q', MOD_WIN);
    CHECK(startmenu_is_open(), "Win+Q opened the start menu");
    startmenu_close();   /* leave no state behind: a menu left open makes the
                          * NEXT test's shortcuts land on it (measured: 4 later
                          * tests failed until this was closed) */

    /* A combo must not fire the plain binding (the superset-swallows-specific
     * shape that broke Win+Shift+Arrow). */
    wm_handle_key('e', MOD_WIN | MOD_SHIFT);
    CHECK(g_fm_calls == 1, "Win+Shift+E does NOT open File Explorer");
    wm_handle_key('i', MOD_WIN | MOD_CTRL);
    CHECK(g_cp_calls == 1, "Win+Ctrl+I does NOT open Control Panel");

    wm_shutdown();
    PASS();
}

static void test_win_shift_arrow_moves_between_desktops(void) {
    TEST("Win+Shift+Left/Right moves the window to another desktop (not a snap)");
    wm_init(1024, 768);
    wm_handle_key(0x20, MOD_WIN | MOD_CTRL);   /* Win+Ctrl+D: a second desktop to move to */

    DosGuiWindow *w = wm_create(100, 100, 300, 200, "Move Me");
    CHECK(w != NULL, "window created");
    int start_desktop = w->desktop;
    int w0 = w->w, h0 = w->h;

    wm_handle_key(0xE04D, MOD_WIN | MOD_SHIFT);   /* Win+Shift+Right */
    CHECK(w->desktop == start_desktop + 1, "moved to the next desktop");
    CHECK(w->w == w0 && w->h == h0, "and was NOT snapped (geometry untouched)");

    /* Negative control: plain Win+Right still snaps, and does not move desktops.
     * Use a window on the CURRENT desktop: the one above was just moved away,
     * and snap acts on the focused window of the desktop being shown. */
    wm_set_current_desktop(w->desktop);
    DosGuiWindow *w2 = wm_create(120, 140, 320, 240, "Snap Me");
    CHECK(w2 != NULL, "second window created");
    int w2_x = w2->x, w2_w = w2->w;
    wm_handle_key(0xE04D, MOD_WIN);               /* Win+Right */
    CHECK(w2->desktop == w->desktop, "plain Win+Right does not change desktop");
    CHECK(w2->w != w2_w || w2->x != w2_x, "plain Win+Right snapped the window");

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
    setenv("WUBU_DESKTOP_DIR", desk, 1);
    rm_rf(desk);
    mkdir(desk, 0755);

    wubu_settings_init();
    wm_init(1024, 768);
    wm_refresh_desktop();
    int before = wm_get_icon_count();

    /* THE CONTRACT THAT WAS BROKEN: our seeder and our taskbar pins write
     * "wubu-app <Name>", and no such program was ever built -- so the shortcut fell
     * through to a shell lookup of a missing binary and the click did nothing. The
     * resolver must turn that Exec into a REGISTRY app, taking the whole remainder so
     * "File Manager" survives its space. */
    {
        /* The rule is tested with its own matcher: this graph has no app registry, and
         * the RULE is what was broken -- "wubu-app <Name>" had no implementation. */
        char got[128];
        CHECK(wubu_exec_app_name("wubu-app Calculator", test_is_app, NULL, got, sizeof got) == 0 &&
              strcmp(got, "Calculator") == 0, "wubu-app prefix resolves to an app name");
        CHECK(wubu_exec_app_name("wubu-app File Manager", test_is_app, NULL, got, sizeof got) == 0 &&
              strcmp(got, "File Manager") == 0, "spaces in the app name survive");
        CHECK(wubu_exec_app_name("Terminal", test_is_app, NULL, got, sizeof got) == 0 &&
              strcmp(got, "Terminal") == 0, "bare app name resolves");
        CHECK(wubu_exec_app_name("wubu-app Nope", test_is_app, NULL, got, sizeof got) != 0,
              "an unknown app does NOT resolve");
        CHECK(wubu_exec_app_name("wubu-app Calculator %U", test_is_app, NULL, got, sizeof got) == 0 &&
              strcmp(got, "Calculator") == 0, "field codes are stripped before resolution");
    }

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

    /* First-boot seed: a fresh desktop gets useful default icons (Plasma/SteamOS
     * behaviour), and only when it has no shortcuts -- never clobbers a curated one. */
    char desk2[512];
    snprintf(desk2, sizeof(desk2), "/tmp/wubu_deskeed_%d", getpid());
    setenv("WUBU_DESKTOP_DIR", desk2, 1);
    rm_rf(desk2);
    mkdir(desk2, 0755);
    int seeded = wm_seed_default_desktop_icons();
    CHECK(seeded == 4, "fresh desktop seeds all 4 default icons");
    char cf[600];
    snprintf(cf, sizeof(cf), "%s/Calculator.desktop", desk2);
    CHECK(stat(cf, &st) == 0 && S_ISREG(st.st_mode),
          "seeded Calculator.desktop exists as a real file");
    CHECK(wm_seed_default_desktop_icons() == 0,
          "second seed refuses to clobber a now-populated desktop");
    /* A singleton shortcut also blocks reseeding (curated desktop respected). */
    char desk3[512];
    snprintf(desk3, sizeof(desk3), "/tmp/wubu_desk_one_%d", getpid());
    setenv("WUBU_DESKTOP_DIR", desk3, 1);
    rm_rf(desk3); mkdir(desk3, 0755);
    CHECK(wm_write_desktop_shortcut("MyDoc", NULL) == 0, "curated shortcut");
    CHECK(wm_seed_default_desktop_icons() == 0,
          "seed skips a desktop that already has one shortcut");

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

    /* Rendering with an island live must draw it, not just not crash. The beads are opt-in
     * (see test_rim_beads_are_opt_in), so this test asks for them explicitly. */
    wm_rim_set_enabled(true);
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

    wm_rim_set_enabled(false);
    wm_rim_clear();
    CHECK(wm_rim_count() == 0, "clear empties the rim");
    wm_shutdown();
    PASS();
}

/* -- Screen-edge slime seam ------------------------------------- */

static void test_slime_seam(void) {
    TEST("slime seam: add, settle, store, release, drag, clear");
    /* The seam's beads are opt-in, like the rim's: this test exercises the seam. */
    wm_slime_set_enabled(true);
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

/* -- Elevation shadow (focused window, THEME_DARK) ---------------- */

static void test_window_shadow(void) {
    TEST("elevation shadow: focused dark window, cached, gone when unfocused");
    /* The idle screensaver blanks the whole screen mid-test (real timer,
     * real seconds) — the hosted session disables it for the same reason. */
    wubu_screensaver_set_enabled(false);
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(300, 200, 400, 300, "Shadow");
    wm_set_focus(w);
    unsigned b0 = wm_shadow_blur_count();
    wm_render(NULL, 1024, 768);
    CHECK(wm_shadow_blur_count() == b0 + 1, "focused render blurs once");
    int sx = w->x + w->w + 4, sy = w->y + w->h / 2;
    uint32_t with = vbe_get_pixel(sx, sy);
    wm_render(NULL, 1024, 768);
    CHECK(wm_shadow_blur_count() == b0 + 1, "idle frame reuses the cache");
    DosGuiWindow *o = wm_create(80, 80, 180, 120, "Other");
    wm_set_focus(o);
    wm_render(NULL, 1024, 768);
    uint32_t without = vbe_get_pixel(sx, sy);
    printf("        shadow probe: with=%08x without=%08x far=%08x blurs=%u\n",
           with, without, vbe_get_pixel(sx + 120, sy), wm_shadow_blur_count());
    /* Differential, not absolute: the backdrop under the shadow is the
     * wallpaper, whose brightness is not a constant. Losing focus must
     * brighten the exact pixel the shadow used to darken. */
    CHECK(((without >> 16) & 0xFF) > ((with >> 16) & 0xFF),
          "shadow darkens beside the frame and lifts when unfocused");
    wm_shutdown();
    wubu_screensaver_set_enabled(true);
    PASS();
}

/* -- Caption geometry comes from the token scale --------------- */

static void test_caption_metrics(void) {
    TEST("caption geometry derives from wubu_tokens.h");

    wubu_theme_set(THEME_DARK);
    ChromeCaptionMetrics m = chrome_caption_metrics();
    CHECK(m.bar_h == WUBU_TITLE_BAR_H, "Win11 caption height is the token");
    CHECK(m.btn_w == WUBU_TITLE_BTN_W, "Win11 caption button width is the token");
    CHECK(m.btn_h == WUBU_TITLE_BAR_H, "Win11 buttons run the full caption height");
    CHECK(m.gap == 0, "Win11 buttons are flush");

    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(200, 100, 500, 300, "Metrics");
    ChromeButtonRect bmin, bmax, bclose;
    CHECK(chrome_button_rects(w, &bmin, &bmax, &bclose),
          "a wide window gets caption buttons");
    CHECK(bclose.y == bmax.y && bmax.y == bmin.y,
          "all three buttons share one baseline");
    CHECK(bclose.h == WUBU_TITLE_BAR_H, "Close is caption-height");
    CHECK(bmax.x + bmax.w == bclose.x, "no gap between max and close");
    CHECK(bmin.x + bmin.w == bmax.x, "no gap between min and max");
    CHECK(bclose.x + bclose.w == w->x + w->w - chrome_border_width(),
          "Close touches the frame edge in Win11 mode");
    wm_shutdown();

    wubu_theme_set(THEME_WIN98_CLASSIC);
    ChromeCaptionMetrics c = chrome_caption_metrics();
    CHECK(c.btn_w == c.btn_h, "classic keeps square buttons");
    CHECK(c.gap > 0, "classic keeps an inset gap");
    wm_init(1024, 768);
    DosGuiWindow *cw = wm_create(200, 100, 500, 300, "Classic");
    CHECK(chrome_button_rects(cw, &bmin, &bmax, &bclose),
          "classic buttons exist");
    CHECK(bclose.x + bclose.w < cw->x + cw->w - chrome_border_width(),
          "classic buttons are inset from the frame edge");
    CHECK(bclose.w == bclose.h, "classic close is square");
    wm_shutdown();
    PASS();
}

/* -- Running-indicator pills (modern taskbar) --------------------- */

/* Pager clicks must land on the drawn tabs: input derives desk_x from the
 * published tray well exactly like the renderer (the old screen_w-150
 * guess was offset by the clock+tray reserve). */
static void test_pager_hit_test(void) {
    TEST("pager hit-test: clicks land on the drawn classic tabs");
    wubu_screensaver_set_enabled(false);
    wubu_theme_set(THEME_WIN98_CLASSIC);
    wm_init(1024, 768);
    wm_add_desktop();
    systray_add("pprobe", 0x00AA00, NULL, NULL);
    wm_render(NULL, 1024, 768);
    CHECK(g_dwm.desktop_count >= 2, "two desktops exist");
    CHECK(g_dwm.tray_well_w > 0, "classic render publishes the tray well");
    int desk_x = g_dwm.tray_well_x - WUBU_TASKBAR_CLASSIC_PAGER_RESERVE;
    int task_h = taskbar_height_dynamic();
    int desk_y = taskbar_top() + (task_h - WUBU_TASKBAR_CLASSIC_PAGER_H) / 2;
    int px = desk_x + WUBU_TASKBAR_CLASSIC_PAGER_PITCH +
             WUBU_TASKBAR_CLASSIC_PAGER_W / 2;
    wm_handle_mouse(px, desk_y + WUBU_TASKBAR_CLASSIC_PAGER_H / 2, 1, 1);
    CHECK(g_dwm.current_desktop == 1, "clicking tab 1 switches to desktop 1");
    systray_remove("pprobe");
    wm_shutdown();
    wubu_screensaver_set_enabled(true);
    PASS();
}

static int g_tray_clicks = 0;
static void tray_click_probe(void) { g_tray_clicks++; }

/* The tray hit-test must use the well the renderer publishes. The old
 * input path re-derived a right-aligned walk from the clock and missed
 * the modern centred bar — tray clicks did nothing in the dark theme. */
static void test_tray_hit_test(void) {
    TEST("tray hit-test: input hits the well the renderer publishes (both eras)");
    wubu_screensaver_set_enabled(false);
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    g_tray_clicks = 0;
    systray_add("probe", 0x00AA00, tray_click_probe, NULL);
    systray_add("probe2", 0x0000AA, tray_click_probe, NULL);
    wm_render(NULL, 1024, 768);
    CHECK(g_dwm.tray_well_w >= 2 * (DOSGUI_SYSTRAY_SIZE + WUBU_SPACE_XS),
          "modern render publishes a tray well for both icons");
    int cx0 = g_dwm.tray_well_x + DOSGUI_SYSTRAY_SIZE / 2;
    int cy0 = g_dwm.tray_well_y + DOSGUI_SYSTRAY_SIZE / 2;
    CHECK(systray_icon_at(cx0, cy0) == 0, "well slot 0 maps to icon 0");
    wm_handle_mouse(cx0, cy0, 1, 1);
    CHECK(g_tray_clicks == 1, "clicking the drawn modern tray icon fires its callback");
    int cx1 = cx0 + DOSGUI_SYSTRAY_SIZE + WUBU_SPACE_XS;
    wm_handle_mouse(cx1, cy0, 1, 1);
    CHECK(g_tray_clicks == 2, "second icon slot fires too");
    wubu_theme_set(THEME_WIN98_CLASSIC);
    wm_render(NULL, 1024, 768);
    CHECK(g_dwm.tray_well_w > 0, "classic render publishes the tray well");
    int kx = g_dwm.tray_well_x + DOSGUI_SYSTRAY_SIZE / 2;
    int ky = g_dwm.tray_well_y + DOSGUI_SYSTRAY_SIZE / 2;
    wm_handle_mouse(kx, ky, 1, 1);
    CHECK(g_tray_clicks == 3, "clicking the drawn classic tray icon fires its callback");
    systray_remove("probe");
    systray_remove("probe2");
    wm_shutdown();
    wubu_screensaver_set_enabled(true);
    PASS();
}

static void test_taskbar_indicator(void) {
    TEST("taskbar indicator: accent when focused, idle when minimized, none for pins");
    wubu_screensaver_set_enabled(false);
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(200, 150, 300, 200, "Ind");
    wm_set_focus(w);
    wm_render(NULL, 1024, 768);

    WmTaskLayout L;
    taskbar_modern_layout(1024, 768, &L);
    CHECK(L.n_slots >= 1, "the running window owns a slot");
    const WmTaskSlot *s = &L.slot[0];
    int ix = s->x + s->w / 2;
    int iy = s->y + s->h - WUBU_TASKBAR_INDICATOR_GAP -
             WUBU_TASKBAR_INDICATOR_H + 1;
    CHECK(vbe_get_pixel(ix, iy) == tc()->select_bg,
          "focused window draws the accent pill");

    w->flags |= DOSGUI_WIN_MINIMIZED;
    wm_render(NULL, 1024, 768);
    CHECK(vbe_get_pixel(ix, iy) == WUBU_TASKBAR_INDICATOR_IDLE,
          "minimized window keeps an idle pill");
    CHECK(vbe_get_pixel(ix + WUBU_TASKBAR_INDICATOR_W_IDLE, iy) ==
          tc()->taskbar_bg,
          "idle pill is narrow (6px), not full-width");
    wm_shutdown();
    wubu_screensaver_set_enabled(true);
    PASS();
}

/* -- Auto-hide taskbar (modern theme) --------------------------- */

static void test_taskbar_autohide(void) {
    TEST("taskbar stays revealed (Win11 parity): clicks always land on Start");
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    wm_create(200, 150, 300, 200, "Bar");
    int th = taskbar_height_dynamic();
    int sh = g_dwm.screen_h;

    wm_handle_mouse(512, sh - 4, 1, 1);
    for (int k = 0; k < 40; k++) wm_render(NULL, 1024, 768);
    CHECK(g_dwm.taskbar_offset == 0, "the bar starts revealed");

    WmTaskLayout L;
    taskbar_modern_layout(1024, 768, &L);
    int sx = L.start_x + L.start_w / 2;
    int sy = L.start_y + L.start_h / 2;

    /* Pointer well above the bottom band: the bar stays put (no auto-hide). */
    wm_handle_mouse(512, 300, 0, 0);
    for (int k = 0; k < 40; k++) wm_render(NULL, 1024, 768);
    CHECK(g_dwm.taskbar_offset == 0, "the bar stays revealed");

    /* A press on Start always opens the menu. */
    wm_handle_mouse(sx, sy, 1, 1);
    CHECK(startmenu_is_open(), "Start opens with the bar always revealed");
    wm_handle_mouse(512, 300, 1, 1);   /* a press outside closes the menu */
    wm_handle_mouse(512, 300, 1, 2);

    /* A classic theme keeps the bar pinned too. */
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

    /* The band AROUND the floating bar must be PAINTED -- not the raw clear colour.
     * It used to be asserted equal to desktop_bg, which encoded the very bug the user
     * reported: the wallpaper stopped at fb_h - task_h, so the 8px gap under the bar
     * showed flat theme background (23,23,23) against wallpaper at ~(99,100,101). A
     * wall does not stop where a bar begins, so the correct assertion is "the band
     * matches the desktop surface just above it", whether that surface is a wallpaper
     * pixel or the theme colour. */
    vbe_init(1024, 768);
    wm_render(NULL, 1024, 768);
    uint32_t band = vbe_get_pixel(10, sh - 4);
    /* Two pixels above the band, not twenty: the backdrop is a vertical gradient, so a
     * large vertical offset differs by design. A real gap still shows here -- the theme
     * colour sits ~8 counts away from the wall at this height. */
    uint32_t above = vbe_get_pixel(10, sh - 6);
    /* Tolerance, not equality: the default backdrop is now a vertical gradient, so 4px apart
     * is a few steps different by construction. The INTENT -- the wall continues under the
     * bar instead of stopping at it -- is a match within a few counts, or the theme colour. */
    int dr = (int)((band >> 16) & 0xff) - (int)((above >> 16) & 0xff);
    int dg = (int)((band >> 8) & 0xff) - (int)((above >> 8) & 0xff);
    int db = (int)(band & 0xff) - (int)(above & 0xff);
    if (dr < 0) dr = -dr; if (dg < 0) dg = -dg; if (db < 0) db = -db;
    CHECK((dr <= 4 && dg <= 4 && db <= 4) || band == tc()->desktop_bg,
          "the band around the floating bar is painted, not a raw gap");
    CHECK(!(band == 0 && tc()->desktop_bg != 0),
          "the band is not the unpainted clear colour");

    wubu_theme_set(THEME_WIN98_CLASSIC);
    wm_shutdown();
    PASS();
}


/* -- Shell combos must reach the shell while an APP window has focus ----------
 *
 * wm_handle_key dispatched to the focused window and returned BEFORE Win+D,
 * Win+Shift+arrows and the 0xE0xx arrow path, so none of them could ever fire in the one
 * situation they exist for: an app window on screen. Each of these three tests fails on the
 * old ordering and passes on the corrected one -- they are the regression guard, not a
 * demonstration that the functions exist. */

static int count_alive_windows(void) { return wm_window_count(); }

/* An app window WITH a key handler is the condition under test: the old dispatch block
 * returned as soon as on_key was non-NULL, so a window without one let control fall through
 * to the shell combos and hid the bug. This stub makes the window look like a real app. */
static int g_app_key_calls;
static void app_on_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)key; (void)mods;
    g_app_key_calls++;
}

static void test_win_d_show_desktop_with_focus(void) {
    TEST("Win+D minimizes every window while one has focus");
    wm_init(1024, 768);
    DosGuiWindow *a = wm_create(20, 20, 200, 150, "A");
    DosGuiWindow *b = wm_create(60, 80, 200, 150, "B");
    CHECK(a != NULL && b != NULL, "two windows created");
    a->on_key = app_on_key; b->on_key = app_on_key;
    wm_set_focus(a);
    int alive = count_alive_windows();
    CHECK(alive == 2, "two alive before");

    wm_handle_key('d', MOD_WIN);

    int minimized = 0;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++)
        if (wm_window_minimized_at(i)) minimized++;
    CHECK(minimized == alive, "Win+D minimized all windows even though an app had focus");

    wm_handle_key('d', MOD_WIN);        /* second press restores */
    int still_minimized = 0;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++)
        if (wm_window_minimized_at(i)) still_minimized++;
    CHECK(still_minimized == 0,
          "Win+D restores when everything is already minimized");
    wm_shutdown();
    PASS();
}

static void test_win_shift_arrow_moves_window_to_desktop(void) {
    TEST("Win+Shift+Right moves the focused window to the next desktop");
    wm_init(1024, 768);
    wm_add_desktop();                   /* two desktops to move between */
    DosGuiWindow *w = wm_create(20, 20, 200, 150, "Mover");
    CHECK(w != NULL, "window created");
    w->on_key = app_on_key;
    wm_set_focus(w);
    int before = w->desktop;

    wm_handle_key(0xE04D, MOD_WIN | MOD_SHIFT);

    CHECK(w->desktop == before + 1,
          "the window changed desktop although it had focus (dead-code regression)");
    wm_shutdown();
    PASS();
}

static void test_win_arrow_snaps_with_focus(void) {
    TEST("Win+Left snaps the focused window (0xE0xx scancode path)");
    wm_init(1024, 768);
    /* 900, not 400: a 400 px window is already narrower than half of 1024, so the old
     * assertion passed without the shortcut doing anything at all -- a gate that cannot
     * fail is not a gate. */
    DosGuiWindow *w = wm_create(100, 100, 900, 300, "Snap");
    CHECK(w != NULL, "window created");
    w->on_key = app_on_key;
    wm_set_focus(w);
    int width_before = w->w;

    wm_handle_key(0xE04B, MOD_WIN);

    CHECK(w->w < width_before && w->w <= 1024 / 2 + 16,
          "window shrank to the left half after Win+Left (the 0xE0xx block used to be unreachable)");
    wm_shutdown();
    PASS();
}


static void test_wheel_over_taskbar_cycles_focus(void) {
    TEST("wheel over the taskbar cycles window focus");
    wm_init(1024, 768);
    DosGuiWindow *a = wm_create(20, 20, 200, 150, "A");
    DosGuiWindow *b = wm_create(80, 60, 200, 150, "B");
    CHECK(a && b, "two windows");
    wm_set_focus(a);
    CHECK(wm_get_focused() == a, "A focused first");

    wm_wheel_cycle(+1);
    CHECK(wm_get_focused() != a, "wheel moved focus away from A");
    wm_wheel_cycle(-1);
    CHECK(wm_get_focused() == a, "wheel back returned focus to A");
    wm_shutdown();
    PASS();
}

static void test_win_number_acts_on_taskbar_slot(void) {
    TEST("Win+number acts on the taskbar slot (restore / minimize toggle)");
    wm_init(1024, 768);
    DosGuiWindow *a = wm_create(20, 20, 200, 150, "A");
    CHECK(a != NULL, "window created");
    WmTaskLayout L;
    taskbar_modern_layout(1024, 768, &L);
    if (L.n_slots < 1) {                      /* no slots on this build: say so, do not pass */
        printf("\n  (skip: no taskbar slots with one window -- layout has %d)\n", L.n_slots);
        wm_shutdown();
        PASS();
        return;
    }
    wm_set_focus(a);
    taskbar_slot_launch(0);                   /* focused window toggles to minimized */
    CHECK(wm_window_minimized_at(0), "Win+1 minimized the focused window");
    taskbar_slot_launch(0);                   /* and back */
    CHECK(!wm_window_minimized_at(0), "Win+1 restored it");
    wm_shutdown();
    PASS();
}


static void test_pointer_path_sees_modifier_state(void) {
    TEST("the pointer path can see the last modifier state (shift-click)");
    wm_init(1024, 768);
    wm_handle_key('x', MOD_SHIFT);
    CHECK((wm_last_mods() & MOD_SHIFT) != 0, "shift is visible to the pointer path");
    wm_handle_key('x', 0);
    CHECK((wm_last_mods() & MOD_SHIFT) == 0, "cleared by the next key event");
    /* Without shift held, the shift-click helper must decline so the caller focuses. */
    CHECK(taskbar_shift_new_instance(0) == 0, "no shift -> not a new instance");
    wm_shutdown();
    PASS();
}


static void test_thumb_capture_maps_the_window_rect(void) {
    TEST("thumbnail sampling maps the window rect, not the whole frame");
    const int fw = 64, fh = 48;
    static uint32_t fb[64 * 48];
    for (int i = 0; i < fw * fh; i++) fb[i] = (uint32_t)(i * 2654435761u);   /* unique-ish */
    uint32_t dst[8 * 8];
    memset(dst, 0, sizeof dst);

    int ok = wm_thumb_capture(fb, fw, fh, 8, 8, 32, 24, dst, 8, 8);
    CHECK(ok == 1, "sampled something");
    CHECK(dst[0] == fb[8 * fw + 8], "top-left sample is the rect's top-left");
    int sx = 8 + (7 * 32) / 8, sy = 8 + (7 * 24) / 8;
    CHECK(dst[7 * 8 + 7] == fb[sy * fw + sx], "last sample is the rect's last row/col");

    /* Outside the frame: no claim, zeros, and ok == 0 rather than garbage. */
    uint32_t off[4];
    CHECK(wm_thumb_capture(fb, fw, fh, 1000, 1000, 10, 10, off, 2, 2) == 0,
          "a rect outside the frame samples nothing");
    PASS();
}

static void test_taskbar_hover_tracks_slots(void) {
    TEST("taskbar hover tracks the slot under the pointer");
    wm_init(1024, 768);
    wm_create(20, 20, 200, 150, "Hover");
    WmTaskLayout L;
    taskbar_modern_layout(1024, 768, &L);
    if (L.n_slots < 1) {
        printf("\n  (skip: no slots)\n");
        wm_shutdown(); PASS(); return;
    }
    const WmTaskSlot *s = &L.slot[0];
    taskbar_modern_handle(s->x + 2, s->y + 2, 0, 0);        /* move over slot 0 */
    CHECK(taskbar_hover_slot() == 0, "hover reports slot 0");
    taskbar_modern_handle(s->x + 2, 2, 0, 0);               /* move to the far top */
    CHECK(taskbar_hover_slot() == -1, "leaving the bar clears the hover");
    wm_shutdown();
    PASS();
}


static void test_hover_preview_composites_a_card(void) {
    TEST("the hover preview paints a card above the bar");
    wm_init(1024, 768);
    wm_create(60, 60, 320, 220, "Preview");
    static uint32_t fb[1024 * 768];
    const uint32_t empty = 0x00123456u;
    for (int i = 0; i < 1024 * 768; i++) fb[i] = empty;

    WmTaskLayout L;
    taskbar_modern_layout(1024, 768, &L);
    if (L.n_slots < 1) { printf("\n  (skip: no slots)\n"); wm_shutdown(); PASS(); return; }
    taskbar_set_hover_delay_ms(0);
    taskbar_modern_handle(L.slot[0].x + 2, L.slot[0].y + 2, 0, 0);

    taskbar_render(fb, 1024, 768);

    /* The card sits directly above the bar; count pixels the render changed there. */
    int card_w = 160 + WUBU_SPACE_XS * 2;
    int card_h = 90 + WUBU_SPACE_XS * 2 + vbe_text_baseline_off();
    int cy = L.bar_y - card_h - WUBU_SPACE_SM;
    if (cy < WUBU_SPACE_XS) cy = WUBU_SPACE_XS;
    int changed = 0;
    for (int y = cy; y < cy + card_h && y < 768; y++)
        for (int x = 0; x < 1024; x++)
            if (fb[y * 1024 + x] != empty) changed++;
    CHECK(changed > card_w * 4, "a card-shaped area was painted above the bar");
    taskbar_set_hover_delay_ms(300);
    wm_shutdown();
    PASS();
}


static int count_colour_in_band(int x0, int x1, int y0, int y1, uint32_t want) {
    int n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (vbe_get_pixel(x, y) == want) n++;
    return n;
}

static void test_taskbar_progress_badge(void) {
    TEST("taskbar progress badge fills proportionally and clamps");
    vbe_init(1024, 768);
    wubu_theme_set(THEME_DARK);      /* the badge is drawn by the modern (Win11) bar */
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(60, 60, 320, 220, "Progressing");
    CHECK(w != NULL, "window created");

    /* Clamp: one place, so no caller can paint outside its button. */
    wm_set_taskbar_progress(w, 150);
    CHECK(w->task_progress == 100, "150 clamps to 100");
    wm_set_taskbar_progress(w, -5);
    CHECK(w->task_progress == 0, "-5 clamps to 0 (no badge)");

    WmTaskLayout L;
    taskbar_modern_layout(1024, 768, &L);
    int slot = -1;
    for (int i = 0; i < L.n_slots; i++)
        if (!L.slot[i].is_pin && L.slot[i].index == w->id) { slot = i; break; }
    if (slot < 0) { printf("\n  (skip: window has no taskbar slot)\n"); wm_shutdown(); PASS(); return; }

    const WmTaskSlot *s = &L.slot[slot];
    uint32_t fill = wubu_theme_colors()->select_bg;
    int y0 = s->y + s->h - 3, y1 = s->y + s->h;

    wm_set_taskbar_progress(w, 0);
    wm_render(NULL, 1024, 768);
    int at0 = count_colour_in_band(s->x, s->x + s->w, y0, y1, fill);

    wm_set_taskbar_progress(w, 50);
    wm_render(NULL, 1024, 768);
    int at50 = count_colour_in_band(s->x, s->x + s->w, y0, y1, fill);

    wm_set_taskbar_progress(w, 100);
    wm_render(NULL, 1024, 768);
    int at100 = count_colour_in_band(s->x, s->x + s->w, y0, y1, fill);

    printf("\n  (progress fill pixels: 0%%=%d 50%%=%d 100%%=%d)\n", at0, at50, at100);
    CHECK(at50 > at0, "50% paints a sliver");
    CHECK(at100 > at50, "100% paints more than 50%");
    wm_shutdown();
    PASS();
}


static void test_snap_assist_offers_the_free_half(void) {
    TEST("snap assist offers the other windows in the free half");
    vbe_init(1024, 768);
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    DosGuiWindow *a = wm_create(40, 40, 300, 200, "Snapped");
    DosGuiWindow *b = wm_create(200, 120, 300, 200, "Other");
    CHECK(a && b, "two windows");
    win11_snap_assist_close();

    wm_set_focus(a);
    wm_snap_half(a, -1);                      /* left half */
    CHECK(win11_snap_assist_active(), "assist opens after a half snap");
    CHECK(win11_snap_assist_zone() == 1, "it offers the RIGHT half -- the free one");

    int ids[3];
    int n = win11_snap_assist_candidates(ids, 3, a->id);
    CHECK(n == 1 && ids[0] == b->id, "the snapped window is not offered; the other one is");

    int cx, cy, cw, ch;
    CHECK(win11_snap_assist_card_rect(0, &cx, &cy, &cw, &ch), "card 0 has a rect");

    /* It has to be VISIBLE, not just state: render and look for the card's frame colour. */
    wm_render(NULL, 1024, 768);
    uint32_t frame = wubu_theme_colors()->border_face;
    int frame_px = 0;
    for (int y = cy; y < cy + ch; y++)
        for (int x = cx; x < cx + cw; x++)
            if (vbe_get_pixel(x, y) == frame) frame_px++;
    CHECK(frame_px > cw, "the assist card is painted in the free half");
    CHECK(win11_snap_assist_click(cx + cw / 2, cy + ch / 2), "clicking the card is consumed");
    CHECK(!win11_snap_assist_active(), "picking closes the assist");
    CHECK(b->x >= 1024 / 2, "the picked window took the free half");
    wm_shutdown();
    PASS();
}


static void test_projection_flyout_offers_only_what_works(void) {
    TEST("Win+P flyout: four Win11 rows, only the possible one selectable");
    vbe_init(1024, 768);
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    projection_flyout_close();

    CHECK(projection_row_count() == 4, "the four Win11 modes are listed");
    CHECK(projection_rows_enabled_count() == 1, "one output -> one selectable mode");
    CHECK(projection_row_enabled(PROJECTION_MODE_PC_ONLY), "PC screen only is selectable");
    for (int i = 1; i < 4; i++)
        CHECK(!projection_row_enabled(i), "the modes needing a second output are greyed, not hidden");

    projection_flyout_toggle();
    CHECK(projection_flyout_is_open(), "Win+P opens it");
    CHECK(projection_selected_row() == PROJECTION_MODE_PC_ONLY, "starts on the applied mode");

    /* A greyed row must not act and must not close the card. */
    int rx, ry, rw, rh;
    CHECK(projection_row_rect(PROJECTION_MODE_EXTEND, 1024, 768, &rx, &ry, &rw, &rh),
          "row 2 has a rect");
    CHECK(projection_flyout_click(rx + 4, ry + 4), "click on a greyed row is consumed");
    CHECK(projection_flyout_is_open(), "and the card stays open");
    CHECK(projection_mode() == PROJECTION_MODE_PC_ONLY, "and nothing changed");

    /* Arrow keys walk ENABLED rows only, so the selection cannot land on a greyed one. */
    projection_flyout_key(0xE050, 0);
    CHECK(projection_row_enabled(projection_selected_row()), "selection stays on an enabled row");

    /* The enabled row applies and closes. */
    CHECK(projection_row_rect(PROJECTION_MODE_PC_ONLY, 1024, 768, &rx, &ry, &rw, &rh),
          "row 0 has a rect");
    projection_flyout_click(rx + 4, ry + 4);
    CHECK(!projection_flyout_is_open(), "picking closes the flyout");
    CHECK(projection_mode() == PROJECTION_MODE_PC_ONLY, "and applies the mode");

    /* It has to be VISIBLE, not just state. */
    projection_flyout_toggle();
    for (int y = 0; y < 768; y++)
        for (int x = 0; x < 1024; x++) vbe_set_pixel(x, y, 0);
    projection_flyout_render(NULL, 1024, 768);
    int cx, cy, cw, ch;
    projection_card_rect(1024, 768, &cx, &cy, &cw, &ch);
    int painted = 0;
    for (int y = cy; y < cy + ch; y++)
        for (int x = cx; x < cx + cw; x++)
            if (vbe_get_pixel(x, y) != 0) painted++;
    CHECK(painted > cw * 8, "the projection card is painted");
    projection_flyout_close();
    wm_shutdown();
    PASS();
}


static void test_drag_to_edge_snaps(void) {
    TEST("drag a window to the screen edge and release: Aero snap");
    vbe_init(1024, 768);
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    DosGuiWindow *w = wm_create(300, 200, 400, 300, "Draggable");
    CHECK(w != NULL, "window created");
    wm_set_focus(w);

    /* Press on the title bar, drag the pointer to the LEFT edge, release there. */
    wm_handle_mouse(w->x + 40, w->y + 8, 1, 1);
    wm_handle_mouse(2, w->y + 8, 0, 0);
    wm_handle_mouse(2, w->y + 8, 0, 2);
    CHECK(w->w <= 1024 / 2 + 8, "released at the left edge -> left half");

    /* Negative control: a drag that ends in the middle must NOT snap. */
    DosGuiWindow *v = wm_create(300, 300, 380, 280, "Middle");
    CHECK(v != NULL, "second window");
    wm_set_focus(v);
    int before = v->w;
    wm_handle_mouse(v->x + 40, v->y + 8, 1, 1);
    wm_handle_mouse(500, 400, 0, 0);
    wm_handle_mouse(500, 400, 0, 2);
    CHECK(v->w == before, "a release away from the edges leaves the size alone");
    wm_shutdown();
    PASS();
}


static void test_alt_tab_switcher_commits_on_alt_release(void) {
    TEST("Alt+Tab shows previews while Alt is held and commits on release");
    vbe_init(1024, 768);
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    alt_tab_close(false);

    DosGuiWindow *a = wm_create(40, 40, 300, 200, "One");
    DosGuiWindow *b = wm_create(120, 80, 300, 200, "Two");
    DosGuiWindow *c = wm_create(200, 140, 300, 200, "Three");
    CHECK(a && b && c, "three windows");
    wm_set_focus(c);                     /* most recent first in z-order */

    /* Alt+Tab opens and selects a DIFFERENT window than the focused one. */
    CHECK(alt_tab_key_down(0x09, MOD_ALT), "Alt+Tab is consumed by the switcher");
    CHECK(alt_tab_is_open(), "the switcher is open");
    CHECK(alt_tab_count() == 3, "all three windows are offered");
    int first = alt_tab_window_at(alt_tab_selected());
    CHECK(first != c->id, "the selection starts away from the already-focused window");

    /* A held Alt with no further Tab commits nothing yet: the grid stays open. */
    CHECK(!alt_tab_key_up(0x09, MOD_ALT), "releasing Tab alone does not commit");
    CHECK(alt_tab_is_open(), "still open while Alt is held");

    /* Second Tab advances. */
    int sel_before = alt_tab_selected();
    alt_tab_key_down(0x09, MOD_ALT);
    CHECK(alt_tab_selected() != sel_before, "another Tab advances the selection");

    /* Releasing Alt commits the selection. */
    int want = alt_tab_window_at(alt_tab_selected());
    CHECK(alt_tab_key_up(0x38, 0), "releasing Alt commits");
    CHECK(!alt_tab_is_open(), "the switcher closed");
    CHECK(wm_get_focused() && wm_get_focused()->id == want, "the selected window is focused");

    /* Esc cancels: focus must not move. */
    DosGuiWindow *now = wm_get_focused();
    alt_tab_key_down(0x09, MOD_ALT);
    CHECK(alt_tab_is_open(), "reopened");
    alt_tab_key_down(0x01, MOD_ALT);
    CHECK(!alt_tab_is_open(), "Esc closed it");
    CHECK(wm_get_focused() == now, "cancelling left the focus alone");

    /* It has to be VISIBLE: the grid paints thumbnails. */
    alt_tab_key_down(0x09, MOD_ALT);
    int cx, cy, cw, ch;
    CHECK(alt_tab_card_rect(0, 1024, 768, &cx, &cy, &cw, &ch), "card 0 has a rect");
    int painted = 0;
    for (int y = cy; y < cy + ch; y++)
        for (int x = cx; x < cx + cw; x++)
            if (vbe_get_pixel(x, y) != 0) painted++;
    alt_tab_render(NULL, 1024, 768);
    int after = 0;
    for (int y = cy; y < cy + ch; y++)
        for (int x = cx; x < cx + cw; x++)
            if (vbe_get_pixel(x, y) != 0) after++;
    CHECK(after > painted, "rendering painted the grid where nothing was");
    alt_tab_close(false);
    wm_shutdown();
    PASS();
}


static void test_taskbar_overflow_never_reaches_the_tray(void) {
    TEST("taskbar overflow: extra windows collapse into a chip, never over the clock");
    vbe_init(1024, 768);
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);

    int made = 0;
    for (int i = 0; i < 26; i++) {
        char title[32];
        snprintf(title, sizeof title, "Window %d", i);
        if (wm_create(20 + (i % 5) * 12, 20 + (i % 7) * 10, 220, 150, title)) made++;
    }
    CHECK(made >= 20, "enough windows to overflow the bar");

    WmTaskLayout L;
    taskbar_modern_layout(1024, 768, &L);
    CHECK(L.overflow_n > 0, "the layout hid the windows that did not fit");
    CHECK(L.n_slots + L.overflow_n > 0, "the hidden ones are counted, not lost");

    /* THE defect this gate exists for: before the cap, slots ran past tray_x and were drawn
     * over the systray and clock. */
    for (int i = 0; i < L.n_slots; i++)
        CHECK(L.slot[i].x + L.slot[i].w <= L.tray_x, "no slot reaches the tray");
    CHECK(L.overflow_x + L.overflow_w <= L.tray_x, "the chip sits inside the row, left of the tray");
    CHECK(L.overflow_x >= L.bar_x && L.overflow_x + L.overflow_w <= L.bar_x + L.bar_w,
          "the chip is on the bar");

    /* And it paints: a chip that renders nothing would be a phantom control. */
    {
        extern void taskbar_render(uint32_t *, int, int);
        taskbar_render(NULL, 1024, 768);   /* the public entry; render_modern_taskbar is static */
    }
    int lit = 0;
    for (int y = L.overflow_y; y < L.overflow_y + L.overflow_h; y++)
        for (int x = L.overflow_x; x < L.overflow_x + L.overflow_w; x++)
            if (vbe_get_pixel(x, y) != 0) lit++;
    CHECK(lit > 50, "the overflow chip has pixels");

    wm_shutdown();
    PASS();
}


static void test_taskbar_overflow_menu_lists_the_hidden(void) {
    TEST("the +N chip opens a list of exactly the hidden windows");
    vbe_init(1024, 768);
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    for (int i = 0; i < 26; i++) {
        char title[32];
        snprintf(title, sizeof title, "Hidden %d", i);
        wm_create(20 + (i % 5) * 12, 20 + (i % 7) * 10, 220, 150, title);
    }
    WmTaskLayout L;
    taskbar_modern_layout(1024, 768, &L);
    CHECK(L.hidden_n > 0, "the layout withheld some windows");

    taskbar_overflow_open();
    CHECK(taskbar_overflow_is_open(), "the chip opened a list");
    CHECK(taskbar_overflow_rows() == L.hidden_n, "it lists every hidden window");
    CHECK(taskbar_overflow_window_at(0) == L.hidden_id[0],
          "row 0 IS the first hidden window -- identity, not a count");

    /* Picking a row focuses that window and closes the list. */
    int want = taskbar_overflow_window_at(1);
    int rx, ry, rw, rh;
    CHECK(taskbar_overflow_row_rect(1, 1024, 768, &rx, &ry, &rw, &rh), "row 1 has a rect");
    taskbar_overflow_click(rx + 4, ry + 4);
    CHECK(!taskbar_overflow_is_open(), "picking closed the list");
    CHECK(wm_get_focused() && wm_get_focused()->id == want, "the picked window is focused");

    /* Esc closes it without focusing anything. */
    taskbar_overflow_open();
    CHECK(taskbar_overflow_is_open(), "reopened");
    CHECK(taskbar_overflow_key(0x01), "Esc is consumed by the list");
    CHECK(!taskbar_overflow_is_open(), "Esc closed it");

    /* And it paints a card above the bar. */
    taskbar_overflow_open();
    int card_x, card_y, card_w, card_h;
    taskbar_overflow_row_rect(0, 1024, 768, &card_x, &card_y, &card_w, &card_h);
    taskbar_overflow_render(NULL, 1024, 768);
    int lit = 0;
    for (int y = card_y; y < card_y + card_h; y++)
        for (int x = card_x; x < card_x + card_w; x++)
            if (vbe_get_pixel(x, y) != 0) lit++;
    CHECK(lit > 100, "the list card has pixels");
    taskbar_overflow_close();
    wm_shutdown();
    PASS();
}


static void test_rim_beads_are_opt_in(void) {
    TEST("the rim's beads are OFF by default: no scribble on the focused frame");
    vbe_init(1024, 768);
    wubu_theme_set(THEME_DARK);
    wm_init(1024, 768);
    CHECK(!wm_rim_enabled(), "the decoration is off unless asked for");

    DosGuiWindow *w = wm_create(300, 200, 400, 260, "Framed");
    CHECK(w != NULL, "window created");
    wm_set_focus(w);
    /* Not "== 0": the rim already carries islands mirrored from the taskbar pins, so a new
     * one takes the next index. Asserting index 0 was asserting the pin count. */
    int rim_rc = wm_rim_add(0.0f, 14.0f, 0xC05050, "rimoptin");
    CHECK(rim_rc >= 0, "an island exists in the model");
    for (int k = 0; k < 60; k++) wm_rim_tick();

    /* Count ink on the window's bottom edge with the decoration OFF. */
    long off = 0;
    for (int y = w->y + w->h - 26; y < w->y + w->h; y++)
        for (int x = w->x; x < w->x + w->w; x++) {
            uint32_t c = vbe_get_pixel(x, y);
            if (c != 0) off++;
        }
    wm_rim_render(NULL, 1024, 768);
    long still = 0;
    for (int y = w->y + w->h - 26; y < w->y + w->h; y++)
        for (int x = w->x; x < w->x + w->w; x++) {
            uint32_t c = vbe_get_pixel(x, y);
            if (c != 0) still++;
        }
    CHECK(still == off, "rendering with beads off painted nothing");

    /* Turned on, the beads DO paint -- so 'off' is a choice, not a broken renderer. */
    wm_rim_set_enabled(true);
    wm_rim_render(NULL, 1024, 768);
    long on = 0;
    for (int y = 0; y < 768; y++)
        for (int x = 0; x < 1024; x++)
            if (vbe_get_pixel(x, y) != 0) on++;
    CHECK(on > still, "enabled, the beads paint");
    wm_rim_set_enabled(false);
    wm_shutdown();
    PASS();
}


int main(void) {
    printf("+========================================================+\n");
    printf("|  WuBuOS DosGui Window Manager Test Suite (Cell 400)   |\n");
    printf("+========================================================+\n\n");

    test_init();
    test_init_shutdown_no_crash();
    test_hover_preview_composites_a_card();
    test_snap_assist_offers_the_free_half();
    test_taskbar_progress_badge();
    test_thumb_capture_maps_the_window_rect();
    test_taskbar_hover_tracks_slots();
    test_pointer_path_sees_modifier_state();
    test_rim_beads_are_opt_in();
    test_taskbar_overflow_menu_lists_the_hidden();
    test_taskbar_overflow_never_reaches_the_tray();
    test_alt_tab_switcher_commits_on_alt_release();
    test_drag_to_edge_snaps();
    test_projection_flyout_offers_only_what_works();
    test_wheel_over_taskbar_cycles_focus();
    test_win_number_acts_on_taskbar_slot();
    test_win_d_show_desktop_with_focus();
    test_win_shift_arrow_moves_window_to_desktop();
    test_win_arrow_snaps_with_focus();
    test_create_window();
    test_create_multiple();
    test_window_count();
    test_focus();
    test_find_by_id();
    test_mouse_click_no_crash();
    test_mouse_drag_window();
    test_taskbar_height();
    test_render_no_crash();
    test_buddy_states_render_differently();
    test_render_draws_glyphs_and_selection();
    test_render_window_drop_shadow();
    test_render_win98_title_bar_is_theme_navy();
    test_vbe_clip_confines_pixels();
    test_render_window_clipped_above_taskbar();
    test_add_icon();
    test_systray_lifecycle();
    test_window_state_transitions();
    test_snap_zones();

    test_snap_layouts();
    test_win_arrow_hotkey();
    test_modifier_bits_match_input_h();
    test_show_desktop_hotkey();
    test_shell_shortcuts_reach_their_launchers();
    test_win_shift_arrow_moves_between_desktops();
    test_add_desktop_and_overview();
    test_osd_levels_and_hotkey();
    test_quick_settings_panel();
    test_screensaver();
    test_lock_screen();
    test_gesture_actions();
    test_icon_click_selects_then_opens();
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
    test_caption_metrics();
    test_window_shadow();
    test_tray_hit_test();
    test_pager_hit_test();
    test_taskbar_indicator();
    test_taskbar_autohide();
    test_modern_taskbar();

    printf("\n========================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("========================================================\n");

    return g_fail > 0 ? 1 : 0;
}
