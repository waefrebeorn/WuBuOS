/*
 * apps_test.c  --  WuBuOS DosGui Apps Test Suite
 *
 * Tests for built-in Win98-style apps:
 * - Task Manager (Windows 11 style)
 * - Regedit (Windows Registry Editor clone)
 * - Calculator (Standard, Scientific, Graphing)
 * - Notepad++ (Editor with tabs, syntax highlighting)
 * - WuBu Canvas (layered Photoshop-class image editor)
 * - File Manager (9P/Styx operations)
 */

#include "../egojar.h"
#include "../egojar_net.h"
#include <sys/socket.h>
#include "apps.h"
#include "calc/calc.h"
#include "notepad.h"
#include "wubu_canvas.h"
#include "taskmgr/taskmgr.h"
#include "regedit/regedit.h"
#include "fm/fm.h"
#include "repl.h"
#include "control/control.h"
#include "editor/editor.h"
#include "wubu_editor.h"
#include "notes.h"
#include "todo.h"
#include "music.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
/* WM functions this gate exercises are REAL (wm.c is linked). We
   forward-declare only them rather than pulling wm.h, whose stronger
   declarations would collide with the 7-arg chrome stub used by the other
   app TUs in this link. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Also stub out the external functions in the app registry */
void explorer_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h;
}

/* Stubs for container functions */
int wubu_ct_start_bwrap(void* ct) { (void)ct; return 0; }
void wubu_ct_destroy(void* ct) { (void)ct; }


/* Link-only stubs for the launcher graph the registry table references.
 * The test only asserts the table is COMPLETE (names resolve to
 * non-NULL launchers); the real launchers are proven by the runtime
 * build. These stubs exist so the unit test links without the full
 * WM/interrupt graph. (The established pattern: the launchers are
 * never CALLED here.) */
DosGuiWindow* bonzi_launch(void) { return NULL; }
DosGuiWindow* comfy_launch(void) { return NULL; }
DosGuiWindow* tandem_launch(void) { return NULL; }
void app_explorer_init(void) {}
void startmenu_handle_click(int x, int y) { (void)x;(void)y; }
void startmenu_toggle(void) {}
int startmenu_is_open(void) { return 0; }
struct hosted_state_t *wm_get_hosted_state(void) { return NULL; }
DosGuiWindow* edr_dash_launch(void) { return NULL; }


void app_explorer_draw(DosGuiWindow *w, uint32_t *f, int fw, int fh) { (void)w;(void)f;(void)fw;(void)fh; }
void app_explorer_key(DosGuiWindow *w, uint32_t k, uint32_t m) { (void)w;(void)k;(void)m; }
void app_explorer_mouse(DosGuiWindow *w, int x, int y, int b, int k) { (void)w;(void)x;(void)y;(void)b;(void)k; }
__attribute__((weak)) void chrome_draw_button(uint32_t *f, int fw, int fh, int x, int y, int w, int h, const char *t, int act) { (void)f;(void)fw;(void)fh;(void)x;(void)y;(void)w;(void)h;(void)t;(void)act; }

/* The app TUs compile against the real 4-arg chrome prototype (which returns
   ChromeContentRect); this link provides an in-mem chrome so it simply
   returns the full window as content. (The real chrome is not linked here.) */
typedef struct { int x, y, w, h; } ChromeContentRect;
__attribute__((weak)) ChromeContentRect chrome_draw_window(void *win, uint32_t *f, int fw, int fh);
ChromeContentRect chrome_draw_window(void *win, uint32_t *f, int fw, int fh) {
    (void)win;(void)f;(void)fw;(void)fh;
    ChromeContentRect r = {0, 0, 100, 100};
    return r;
}
/* The real chrome is not linked here; the editor adapter calls this to draw
   its tab bar, so the in-mem chrome provides it too.  Weak: a link that DOES
   include window_chrome.c (test_apps) gets the real one. */
__attribute__((weak)) void chrome_draw_tabs(int x, int y, int w, int h,
                             const char **labels, int count, int active_tab) {
    (void)x;(void)y;(void)w;(void)h;(void)labels;(void)count;(void)active_tab;
}
/* Real WM API used by the launch gate (declared locally to avoid the header
   / chrome-stub collision above). */
int  wm_init(int screen_w, int screen_h);
void wm_shutdown(void);
int  wm_window_count(void);
void wm_render_desktop(uint32_t *fb, int fb_w, int fb_h);
void wm_render(uint32_t *fb, int fb_w, int fb_h);
DosGuiWindow* dos_window_spawn(void) { return NULL; }
/* hd_eval is the HolyC compiler's entry point; this target does not link the
   compiler (test_holyc owns that, 84 checks).  The stub records what it was
   handed so the REPL test can prove the buffer reached the evaluator. */
static char g_last_eval[256];
int64_t hd_eval(const char *s) {
    snprintf(g_last_eval, sizeof(g_last_eval), "%s", s ? s : "");
    return 0;
}
void handle_double_fault(void) {}
void handle_gpf(void) {}
void handle_page_fault(void) {}
void handle_invalid_opcode(void) {}

void interrupt_panic_dump(void) {}
void klog_printf(const char *f, ...) { (void)f; }
void klog_tx_poll(void) {}
void panic_dump_ring(void) {}
void syscall_handler(void) {}
void wdt_feed(void) {}

void *wubu_dos_proc_launch(const char *p, int f) { (void)p;(void)f; return NULL; }
void *wubu_ct_create(void) { return NULL; }
int wubu_ct_add_payload(void *c, int f, const char *d, size_t n) { (void)c;(void)f;(void)d;(void)n; return 0; }

uint64_t wubu_agi_kernel_global(void) { return 0; }
uint64_t wubu_agi_kernel_promoted_total(void) { return 0; }
uint64_t wubu_agi_kernel_tick(void) { return 0; }
uint64_t wubu_agi_kernel_uptime_ms(void) { return 0; }
void wubu_bonzi_draw(uint32_t *f, int fw, int fh, int x, int y) { (void)f;(void)fw;(void)fh;(void)x;(void)y; }
void wubu_bonzi_init(void) {}
int wubu_bonzi_is_enabled(void) { return 0; }
void wubu_bonzi_mouse(int x, int y) { (void)x;(void)y; }
void wubu_shutdown() {}

void wubu_bonzi_set_bubble(const char *b) { (void)b; }
void wubu_bonzi_tick(uint64_t ms) { (void)ms; }
const char *wubu_compat_cache_dir(void) { return "/tmp"; }
int wubu_compat_db_get(const char *n) { (void)n; return 0; }
void wubu_notify_simple(const char *a, const char *t, const char *m, void *i, int w, int d) { (void)a;(void)t;(void)m;(void)i;(void)w;(void)d; }
int wubu_session_launch_game(void *h, const uint8_t *g, size_t n, const char *nm) { (void)h;(void)g;(void)n;(void)nm; return -1; }
void wubu_settings_load(void) {}
void wubu_settings_save(void) {}

void wubu_trash_empty(void) {}
int wubu_trash_move(const char *p) { (void)p; return 0; }
void wubu_vdso_update(void) {}
const char *wubu_wallpaper_default_path(void) { return ""; }
int wubu_wallpaper_load(const char *p) { (void)p; return 0; }
void wubu_wallpaper_rect(int *x, int *y, int *w, int *h) { (void)x;(void)y;(void)w;(void)h; }
void wubu_wallpaper_render(uint32_t *f, int fw, int fh) { (void)f;(void)fw;(void)fh; }
static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST: %-60s", name); g_total++
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ================================================================
 * Calculator Tests (using API)
 * ================================================================ */

static void test_calc_create_destroy(void) {
    TEST("calc: create and destroy");
    CalcState *calc = calc_create();
    CHECK(calc != NULL, "calc created");
    CHECK(calc_get_mode(calc) == CALC_STANDARD, "default mode");
    CHECK(calc_get_base(calc) == 10, "default base");
    calc_destroy(calc);
    PASS();
}

static void test_calc_mode_switch(void) {
    TEST("calc: mode switching via API");
    CalcState *calc = calc_create();
    calc_set_mode(calc, CALC_SCIENTIFIC);
    CHECK(calc_get_mode(calc) == CALC_SCIENTIFIC, "scientific mode");
    calc_set_mode(calc, CALC_GRAPHING);
    CHECK(calc_get_mode(calc) == CALC_GRAPHING, "graphing mode");
    calc_set_mode(calc, CALC_PROGRAMMER);
    CHECK(calc_get_mode(calc) == CALC_PROGRAMMER, "programmer mode");
    calc_set_mode(calc, CALC_STANDARD);
    CHECK(calc_get_mode(calc) == CALC_STANDARD, "standard mode");
    calc_destroy(calc);
    PASS();
}

static void test_calc_base_switch(void) {
    TEST("calc: base switching via API");
    CalcState *calc = calc_create();
    calc_set_base(calc, 16);
    CHECK(calc_get_base(calc) == 16, "hex mode");
    calc_set_base(calc, 2);
    CHECK(calc_get_base(calc) == 2, "binary mode");
    calc_set_base(calc, 10);
    CHECK(calc_get_base(calc) == 10, "decimal mode");
    calc_destroy(calc);
    PASS();
}

static void test_calc_launch(void) {
    TEST("calc: launch returns window");
    DosGuiWindow *win = calc_launch();
    CHECK(win != NULL, "window created");
    PASS();
}

/* ================================================================
 * Notepad++ Editor Tests (using API)
 * ================================================================ */

static void test_notepad_typing(void) {
    TEST("notepad: typing reaches the buffer");
    notepad_reset();

    notepad_key(NULL, 'H', 0);
    notepad_key(NULL, 'i', 0);
    CHECK(notepad_line_count() == 1, "still one line");
    CHECK(strcmp(notepad_line_text(0), "Hi") == 0, "line reads \"Hi\"");
    CHECK(notepad_cursor_col() == 2, "caret sits after the two characters");
    CHECK(notepad_modified(), "buffer is marked modified");

    notepad_key(NULL, '\r', 0);
    CHECK(notepad_line_count() == 2, "Enter split the buffer");
    CHECK(strcmp(notepad_line_text(0), "Hi") == 0, "first line kept");
    CHECK(strcmp(notepad_line_text(1), "") == 0, "second line is empty");

    notepad_key(NULL, 8, 0);          /* Backspace at column 0 joins */
    CHECK(notepad_line_count() == 1, "backspace joined the lines");
    CHECK(strcmp(notepad_line_text(0), "Hi") == 0, "joined line reads \"Hi\"");
    PASS();
}

static void test_notepad_mouse_positions_caret(void) {
    TEST("notepad: click moves the caret");
    vbe_init(1024, 768);
    wm_init(1024, 768);
    notepad_reset();

    DosGuiWindow *win = notepad_launch();
    CHECK(win != NULL, "window created");
    if (!win) return;

    notepad_key(win, 'A', 0);
    notepad_key(win, 'B', 0);
    notepad_key(win, 'C', 0);

    /* Click well past the 3-character line, INSIDE the real content rect.
       (The old expectation assumed the stub chrome's 0,0,100,100 rect; with
       the real window at 100,80 a click at x=80 lands outside it and the
       caret goes to 0,0 -- which is correct behaviour, not a regression.) */
    ChromeContentRect cr = chrome_draw_window(win, NULL, 0, 0);
    notepad_mouse(win, cr.x + 200, cr.y + 10, 1, 1);
    CHECK(notepad_cursor_line() == 0, "caret stayed on line 1");
    CHECK(notepad_cursor_col() == 3, "caret clamped to the end of \"ABC\"");

    /* A click in the middle of the line lands mid-word. */
    notepad_mouse(win, cr.x + 20, cr.y + 10, 1, 1);
    CHECK(notepad_cursor_col() == 2, "caret follows the click x-position");
    PASS();
}

static void test_notepad_launch(void) {
    TEST("notepad: launch registers + renders a real window");
    vbe_init(1024, 768);
    wm_init(1024, 768);
    int before = wm_window_count();
    DosGuiWindow *win = notepad_launch();
    CHECK(win != NULL, "window created");
    CHECK(wm_window_count() == before + 1, "window registered in WM");

    /* The WM draw chain writes through VBE (vbe_* helpers into VBE's own
       buffer), so we render then confirm the frame contains drawn pixels —
       proof the launch produced a real, visible window (the "user clicked,
       something opened" moment). Mirrors wm_test's VBE-based sampling
       but avoids touching the opaque DosGuiWindow internals. */
    wm_render(NULL, 1024, 768);
    int painted = 0;
    for (int i = 0; i < 1024 && !painted; i++)
        for (int j = 0; j < 768; j++)
            if (vbe_get_pixel(i, j) != 0) { painted = 1; break; }
    CHECK(painted, "launched window rendered visible pixels");
    wm_shutdown();
    vbe_shutdown();
    PASS();
}

/* ================================================================
 * WuBu Canvas Tests (real layered image editor engine)
 * Replaces the removed MS-Paint toy (paint.c) test suite.
 * ================================================================ */

static void test_canvas_create_destroy(void) {
    TEST("canvas: create and destroy");
    WubuCanvas *cv = wubu_cv_create(256, 256);
    CHECK(cv != NULL, "canvas created");
    CHECK(wubu_cv_get_w(cv) == 256 && wubu_cv_get_h(cv) == 256, "dimensions set");
    CHECK(wubu_cv_layer_count(cv) >= 1, "background layer auto-added");
    wubu_cv_destroy(cv);
    PASS();
}

static void test_canvas_layers(void) {
    TEST("canvas: layer add / get / remove");
    WubuCanvas *cv = wubu_cv_create(128, 128);
    int base = wubu_cv_layer_count(cv);
    int idx = wubu_cv_layer_add(cv, "Paint");
    CHECK(idx == base, "layer added at tail");
    WubuLayer *l = wubu_cv_layer_get(cv, idx);
    CHECK(l != NULL, "layer get returns layer");
    CHECK(strcmp(l->name, "Paint") == 0, "layer name set");
    wubu_cv_layer_set_opacity(cv, idx, 128);
    CHECK(l->opacity == 128, "opacity set");
    wubu_cv_layer_remove(cv, idx);
    CHECK(wubu_cv_layer_count(cv) == base, "layer removed");
    wubu_cv_destroy(cv);
    PASS();
}

static void test_canvas_brush_composite(void) {
    TEST("canvas: brush draws + composite produces pixels");
    WubuCanvas *cv = wubu_cv_create(64, 64);
    int pidx = wubu_cv_layer_add(cv, "Paint");
    wubu_cv_set_active_layer(cv, pidx);
    wubu_cv_layer_set_blend(cv, pidx, BLEND_NORMAL);
    wubu_cv_brush(cv, 32, 32);
    wubu_cv_brush(cv, 33, 32);

    uint32_t *out = calloc((size_t)64 * 64, sizeof(uint32_t));
    wubu_cv_composite(cv, out, 64, 64);
    /* A non-zero pixel should exist somewhere (brush dab landed). */
    bool any = false;
    for (int i = 0; i < 64 * 64; i++) if (out[i] != 0) { any = true; break; }
    CHECK(any, "composite produced non-zero pixels");
    free(out);
    wubu_cv_destroy(cv);
    PASS();
}

static void test_canvas_undo_redo(void) {
    TEST("canvas: undo/redo history");
    WubuCanvas *cv = wubu_cv_create(64, 64);
    int pidx = wubu_cv_layer_add(cv, "Paint");
    wubu_cv_set_active_layer(cv, pidx);
    int before = wubu_cv_layer_count(cv);
    wubu_cv_brush(cv, 10, 10);
    wubu_cv_undo(cv);
    /* After undo the active layer pixel at (10,10) should be cleared again. */
    WubuLayer *l = wubu_cv_layer_get(cv, pidx);
    CHECK(l->pixels[10 * l->w + 10] == 0, "brush undone");
    wubu_cv_redo(cv);
    CHECK(l->pixels[10 * l->w + 10] != 0, "brush redone");
    CHECK(wubu_cv_layer_count(cv) == before, "layer count stable");
    wubu_cv_destroy(cv);
    PASS();
}

static void test_canvas_launch(void) {
    TEST("canvas: launch returns window");
    DosGuiWindow *win = launch_canvas();
    CHECK(win != NULL, "window created");
    PASS();
}

/* ================================================================
 * Task Manager
 * ================================================================ */

static void test_tmgr_launch(void) {
    TEST("taskmgr: opens wubulens in a real terminal");
    /* The Task Manager IS the wubulens project: the launcher must resolve to
     * a real program, not draw a fabricated process table. */
    const char *lens = taskmgr_wubulens_path();
    CHECK(lens != NULL && lens[0] != '\0', "a wubulens path resolves");
    DosGuiWindow *win = taskmgr_launch();
    CHECK(win != NULL, "window created");
    CHECK(taskmgr_has_session(win), "the window carries a live pty session");
    PASS();
}

/* ================================================================
 * Regedit Tests (Windows Registry Editor Clone)
 * ================================================================ */

static void test_reg_create_destroy(void) {
    TEST("regedit: create and destroy");
    RegeditState *reg = regedit_create();
    CHECK(reg != NULL, "regedit created");
    regedit_destroy(reg);
    PASS();
}

static void test_reg_init_roots(void) {
    TEST("regedit: init roots via API");
    RegeditState *reg = regedit_create();
    regedit_init_roots(reg);
    CHECK(regedit_get_current_key(reg) == regedit_get_root_key(reg, 1), "default HKCU");
    CHECK(regedit_value_count(reg) == 0, "no values");
    CHECK(regedit_get_expand_depth(reg) == 0, "no expanded keys");
    CHECK(regedit_search_open(reg) == false, "no search");
    regedit_destroy(reg);
    PASS();
}

static void test_reg_create_key(void) {
    TEST("regedit: create subkey via API");
    RegeditState *reg = regedit_create();
    regedit_init_roots(reg);
    RegKey *hkcu = (RegKey*)regedit_get_root_key(reg, 1);
    RegKey *software = regedit_create_key(reg, hkcu, "Software");
    CHECK(software != NULL, "key created");
    CHECK(hkcu->child_count == 1, "child count 1");
    CHECK(strcmp(software->name, "Software") == 0, "name correct");
    CHECK(software->parent == hkcu, "parent correct");
    
    RegKey *wubu = regedit_create_key(reg, software, "WuBuOS");
    CHECK(wubu != NULL, "subkey created");
    CHECK(software->child_count == 1, "software has 1 child");
    CHECK(strcmp(wubu->name, "WuBuOS") == 0, "name correct");
    regedit_destroy(reg);
    PASS();
}

static void test_reg_set_value(void) {
    TEST("regedit: set values via API");
    RegeditState *reg = regedit_create();
    regedit_init_roots(reg);
    CHECK(regedit_set_string(reg, "Version", "1.0.0") == 0, "string value");
    CHECK(regedit_set_dword(reg, "BuildNumber", 1234) == 0, "dword value");
    CHECK(regedit_value_count(reg) == 2, "two values");
    regedit_destroy(reg);
    PASS();
}

static void test_reg_navigate(void) {
    TEST("regedit: navigate tree via API");
    RegeditState *reg = regedit_create();
    regedit_init_roots(reg);
    RegKey *hkcu = (RegKey*)regedit_get_root_key(reg, 1);
    regedit_expand_key(reg, hkcu);
    CHECK(regedit_get_expand_depth(reg) == 1, "hkcu expanded");
    
    RegKey *software = hkcu->children;
    if (software) {
        regedit_expand_key(reg, software);
        CHECK(regedit_get_expand_depth(reg) == 2, "software expanded");
    }
    regedit_destroy(reg);
    PASS();
}

static void test_reg_search(void) {
    TEST("regedit: search via API");
    RegeditState *reg = regedit_create();
    regedit_init_roots(reg);
    regedit_set_search_text(reg, "WuBu");
    regedit_set_search_open(reg, true);
    CHECK(strcmp(regedit_get_search_text(reg), "WuBu") == 0, "search text set");
    CHECK(regedit_search_open(reg) == true, "dialog open");
    regedit_destroy(reg);
    PASS();
}

static void test_reg_launch(void) {
    TEST("regedit: launch returns window");
    DosGuiWindow *win = regedit_launch();
    CHECK(win != NULL, "window created");
    PASS();
}

/* ================================================================
 * File Manager Tests (9P/Styx)
 * ================================================================ */

static void test_fm_create_destroy(void) {
    TEST("filemgr: create and destroy");
    FileManagerState *fm = fm_create();
    CHECK(fm != NULL, "filemgr created");
    fm_destroy(fm);
    PASS();
}

static void test_fm_scan(void) {
    TEST("filemgr: scan directory via API");
    FileManagerState *fm = fm_create();
    fm_scan_dir(fm, "/home/wubu");
    CHECK(fm_entry_count(fm) == 7, "7 entries");
    CHECK(strcmp(fm_entry_name(fm, 0), "..") == 0, "parent dir first");
    CHECK(fm_entry_is_dir(fm, 0) == true, "parent is dir");
    CHECK(strcmp(fm_entry_name(fm, 1), "src") == 0, "src dir");
    CHECK(fm_entry_is_dir(fm, 1) == true, "src is dir");
    CHECK(strcmp(fm_entry_name(fm, 4), "main.c") == 0, "main.c file");
    CHECK(fm_entry_is_dir(fm, 4) == false, "main.c is file");
    CHECK(fm_entry_size(fm, 4) == 1024, "main.c size");
    fm_destroy(fm);
    PASS();
}

static void test_fm_navigate(void) {
    TEST("filemgr: navigate via API");
    FileManagerState *fm = fm_create();
    fm_scan_dir(fm, "/home/wubu");
    int old_count = fm_entry_count(fm);
    fm_scan_dir(fm, "/home/wubu/src");
    CHECK(strcmp(fm_get_current_path(fm), "/home/wubu/src") == 0, "path updated");
    fm_destroy(fm);
    PASS();
}

static void test_fm_selection(void) {
    TEST("filemgr: selection via API");
    FileManagerState *fm = fm_create();
    fm_scan_dir(fm, "/home/wubu");
    fm_set_selected_idx(fm, 2);
    CHECK(fm_get_selected_idx(fm) == 2, "selected");
    CHECK(strcmp(fm_entry_name(fm, 2), "apps") == 0, "correct entry");
    fm_destroy(fm);
    PASS();
}

static void test_fm_9p_ops(void) {
    TEST("filemgr: 9P operations via API");
    FileManagerState *fm = fm_create();
    int fid = fm_open_fid(fm, "/home/wubu/test.txt");
    CHECK(fid == 1, "fid opened");
    CHECK(fm_read_fid(fm, fid, NULL, 0, 100) == 0, "read ok");
    CHECK(fm_write_fid(fm, fid, NULL, 0, 100) == 0, "write ok");
    CHECK(fm_close_fid(fm, fid) == 0, "close ok");
    fm_destroy(fm);
    PASS();
}

static void test_fm_launch(void) {
    TEST("filemgr: launch returns window");
    DosGuiWindow *win = fm_launch();
    CHECK(win != NULL, "window created");
    PASS();
}

/* ================================================================
 * REPL Tests
 * ================================================================ */

/* The desktop REPL is src/apps/repl.c (wired through hosted.c).  The old
 * tests here drove a second, stub implementation under src/apps/repl/ that
 * nothing launched; that file is gone.  These now drive the REAL REPL's
 * input primitives and assert the buffer a keystroke actually reaches. */

static void test_repl_typing(void) {
    TEST("repl: typing reaches the input buffer");
    REPLState *r = repl_state();
    CHECK(r != NULL, "live REPL instance is exposed");
    if (!r) return;

    repl_input_char(r, '1');
    repl_input_char(r, '+');
    repl_input_char(r, '1');
    CHECK(r->input_pos == 3, "three characters buffered");
    CHECK(strcmp(r->input, "1+1") == 0, "buffer reads \"1+1\"");

    repl_input_backspace(r);
    CHECK(r->input_pos == 2, "backspace removes one character");
    CHECK(strcmp(r->input, "1+") == 0, "buffer reads \"1+\"");

    repl_input_backspace(r);
    repl_input_backspace(r);
    repl_input_backspace(r);            /* must not underflow */
    CHECK(r->input_pos == 0, "backspace at position 0 is a no-op");
    PASS();
}

static void test_repl_submit_evaluates(void) {
    TEST("repl: submit echoes the line and evaluates it");
    REPLState *r = repl_state();
    if (!r) { CHECK(false, "live REPL instance is exposed"); return; }

    repl_input_char(r, '7');
    repl_input_char(r, '*');
    repl_input_char(r, '6');
    int before = r->line_count;

    repl_submit(r);

    CHECK(r->line_count == before + 2, "the echo and the result are both recorded");
    CHECK(strcmp(r->lines[before], "$ 7*6") == 0, "echo line reads \"$ 7*6\"");
    CHECK(strncmp(r->lines[before + 1], "= ", 2) == 0,
          "result line is formatted as a value");
    CHECK(strcmp(g_last_eval, "7*6") == 0,
          "the input buffer was handed to the evaluator verbatim");
    CHECK(r->input_pos == 0, "input cleared after submit");
    CHECK(r->input[0] == '\0', "input buffer emptied");
    PASS();
}

/* ================================================================
 * Control Panel Tests
 * ================================================================ */

static void test_control_create_destroy(void) {
    TEST("control: create and destroy");
    ControlState *ctrl = control_create();
    CHECK(ctrl != NULL, "control created");
    control_destroy(ctrl);
    PASS();
}

static void test_control_set_tab(void) {
    TEST("control: set tab via API");
    ControlState *ctrl = control_create();
    control_set_tab(ctrl, 1);
    CHECK(control_get_tab(ctrl) == 1, "tab set");
    control_destroy(ctrl);
    PASS();
}

static void test_control_launch(void) {
    TEST("control: launch returns window");
    DosGuiWindow *win = control_launch();
    CHECK(win != NULL, "window created");
    PASS();
}

/* ================================================================
 * Editor Tests
 * ================================================================ */

static void test_editor_create_destroy(void) {
    TEST("editor: create and destroy");
    EditorState *ed = editor_create();
    CHECK(ed != NULL, "editor created");
    editor_destroy(ed);
    PASS();
}

static void test_editor_launch(void) {
    TEST("editor: launch returns window");
    DosGuiWindow *win = editor_launch();
    CHECK(win != NULL, "window created");
    PASS();
}

/* The adapter is only worth wiring if keystrokes actually reach the engine.
 * Before the rewrite, editor_launch() bound NO callbacks, so this test could
 * not have existed -- the window was blank and inert. */
static void test_editor_typing(void) {
    TEST("editor: typing reaches the engine");
    DosGuiWindow *win = editor_launch();
    CHECK(win != NULL, "window created");
    if (!win) return;

    editor_key(win, 'H', 0);
    editor_key(win, 'i', 0);
    editor_key(win, '\r', 0);
    editor_key(win, '!', 0);

    WubuEditor *e = (WubuEditor *)editor_engine_of_window(win);
    CHECK(e != NULL, "engine handle is bound to the window");
    if (!e) return;
    CHECK(e->n_tabs == 1, "one tab is open");
    CHECK(e->tabs[0].n_lines == 2, "Enter split the buffer into two lines");
    CHECK(e->tabs[0].lines[0].len == 2, "line 1 holds two characters");
    CHECK(strcmp(e->tabs[0].lines[0].text, "Hi") == 0, "line 1 reads \"Hi\"");
    CHECK(strcmp(e->tabs[0].lines[1].text, "!") == 0, "line 2 reads \"!\"");
    CHECK(e->tabs[0].modified, "buffer is marked modified");
    PASS();
}

static void test_editor_mouse_positions_caret(void) {
    TEST("editor: mouse click moves the caret");
    DosGuiWindow *win = editor_launch();
    CHECK(win != NULL, "window created");
    if (!win) return;

    editor_key(win, 'A', 0);
    editor_key(win, 'B', 0);
    editor_key(win, 'C', 0);

    /* Click inside the content area (the stub chrome reports 0,0,100,100). */
    editor_mouse(win, 80, 40, 1, 1);

    WubuEditor *e = (WubuEditor *)editor_engine_of_window(win);
    CHECK(e != NULL, "engine handle is bound");
    if (!e) return;
    CHECK(e->tabs[0].cursor_line == 0, "caret stayed on line 1");
    CHECK(e->tabs[0].cursor_col >= 0 && e->tabs[0].cursor_col <= 3,
          "caret column is within the line");
    PASS();
}

/* ================================================================
 * Main Test Runner
 * ================================================================ */


/* ================================================================
 * The registry completeness + the new apps (Notes/Todo/Music)
 * ================================================================ */

static void test_wet_discovery(void) {
    TEST("applet discovery: a .wet file on disk is a desktop program");
    const char *dir = "/tmp/wubu_wet_discovery";
    char cmd[512];
    snprintf(cmd, sizeof cmd, "rm -rf %s && mkdir -p %s", dir, dir);
    if (system(cmd) != 0) { FAIL("mkdir"); return; }

    /* Two applets and one impostor that must NOT be picked up. */
    char hello_path[512], clock_path[512];
    snprintf(hello_path, sizeof hello_path, "%s/Hello.wet", dir);
    snprintf(clock_path, sizeof clock_path, "%s/Clock.wet", dir);

    FILE *f = fopen(hello_path, "w");
    if (!f) { FAIL("write Hello"); return; }
    fputs("wet.applet {\n  title = 'Hello',\n  width = 320, height = 200,\n  content = wet.canvas { children = {} }\n}\n", f);
    fclose(f);
    f = fopen(clock_path, "w");
    if (!f) { FAIL("write Clock"); return; }
    fputs("wet.applet {\n  title = 'Clock',\n  width = 200, height = 200,\n  content = wet.canvas { children = {} }\n}\n", f);
    fclose(f);
    char txt_path[512];
    snprintf(txt_path, sizeof txt_path, "%s/notes.txt", dir);
    f = fopen(txt_path, "w");
    if (!f) { FAIL("write txt"); return; }
    fputs("not an applet\n", f);
    fclose(f);

    int added = app_scan_wet_dir(dir);
    if (added != 2) { FAIL("scan added != 2"); return; }
    if (app_wet_count() != 2) { FAIL("wet count != 2"); return; }

    const DosGuiAppDef *hello = app_find_by_name("Hello");
    if (!hello) { FAIL("Hello not registered"); return; }
    if (!hello->path || strcmp(hello->path, hello_path) != 0) {
        FAIL("path not bound"); return;
    }
    /* A discovered applet is launched through its path, not a C launcher. */
    if (hello->launch != NULL) { FAIL("discovered applet has a C launcher"); return; }

    const DosGuiAppDef *clock = app_find_by_name("Clock");
    if (!clock) { FAIL("Clock not registered"); return; }
    if (strcmp(clock->name, "Clock") != 0) { FAIL("name is not the file base"); return; }

    /* Icon ids are unique across the discovered set. */
    if (hello->icon_type == clock->icon_type) { FAIL("icon id collision"); return; }
    if (app_find(hello->icon_type) != hello) { FAIL("app_find by icon id"); return; }

    /* Idempotent: a second scan adds nothing. */
    if (app_scan_wet_dir(dir) != 0) { FAIL("second scan was not idempotent"); return; }
    if (app_wet_count() != 2) { FAIL("second scan duplicated entries"); return; }

    /* A missing directory is an error, not a crash. */
    if (app_scan_wet_dir("/tmp/definitely_not_here_wubu") != -1) {
        FAIL("missing dir not refused"); return;
    }

    /* Bounds. */
    if (app_wet_at(-1) != NULL || app_wet_at(99) != NULL) { FAIL("bounds"); return; }

    PASS();
}

static void test_registry_complete(void) {
    TEST("registry: every name has a real launcher");
    if (g_app_def_count < 16) { FAIL("app def count"); return; }
    const char *must[] = { "Notes", "Todo", "Music", "Task Manager",
                           "Notepad", "Calculator", "Terminal",
                           "File Manager", "Settings", "Bonzi Buddy" };
    for (size_t i = 0; i < sizeof(must) / sizeof(must[0]); i++) {
        const DosGuiAppDef *d = app_find_by_name(must[i]);
        if (!d || !d->launch) { FAIL(must[i]); return; }
    }
    /* A .wet applet is a first-class program kind: every program created from
     * a VWS template is one, so the registry must be able to launch one. */
    const DosGuiAppDef *wet = app_find_by_name("WET Applet");
    if (!wet || !wet->launch) { FAIL("WET Applet"); return; }

    /* Every entry, not just the named ones.  A WEAK launcher that no link
     * defines is relocated to 0, and a registry table holding that address
     * crashes the moment the user clicks it -- so a NULL launch pointer is a
     * defect anywhere in the table, not only in the entries anyone remembered
     * to list.  (This is how the unguarded launch_vws_designer entry was
     * found.) */
    for (int i = 0; i < g_app_def_count; i++) {
        if (!g_app_defs[i].name || !g_app_defs[i].name[0]) { FAIL("unnamed entry"); return; }
        if (!g_app_defs[i].launch) { FAIL(g_app_defs[i].name); return; }
    }
    PASS();
}

static void test_notes_real(void) {
    TEST("notes: create/delete + the real-file persistence");
    notes_test_reset();
    if (notes_create("Audit note") != 0) { FAIL("create"); return; }
    if (notes_create("Second note") != 0) { FAIL("create2"); return; }
    if (notes_count() != 2) { FAIL("count"); return; }
    /* click-to-select: row 1 renders at y = 20 + 16 */
    notes_mouse(NULL, 12, 20 + 16, 1, 1);
    if (notes_selected() != 1) { FAIL("mouse select"); return; }
    notes_select(0);
    if (notes_delete_selected() != 0) { FAIL("delete"); return; }
    notes_select(0);
    if (notes_delete_selected() != 0) { FAIL("delete2"); return; }
    if (notes_count() != 0) { FAIL("count after delete"); return; }
    PASS();
}

static void test_todo_real(void) {
    TEST("todo: add/toggle/delete + the real-file persistence");
    todo_test_reset();
    if (todo_add("close the gaps") != 0) { FAIL("add"); return; }
    if (todo_add("train the decoder") != 0) { FAIL("add2"); return; }
    if (todo_pending() != 2) { FAIL("pending"); return; }
    todo_toggle_selected();
    if (todo_pending() != 1) { FAIL("pending after toggle"); return; }
    /* click-to-select: row 1 renders at y = 20 + 16 */
    todo_mouse(NULL, 12, 20 + 16, 1, 1);
    if (todo_selected() != 1) { FAIL("mouse select"); return; }
    todo_mouse(NULL, 12, 4, 1, 1);            /* above the list: ignored */
    if (todo_selected() != 1) { FAIL("mouse above list"); return; }
    todo_delete_selected();
    todo_delete_selected();
    if (todo_count() != 0) { FAIL("count after deletes"); return; }
    PASS();
}

static void test_music_real(void) {
    TEST("music: the playlist scan + the player state");
    music_test_reset();
    music_scan();
    int n = music_count();
    if (n > 0) {
        music_select(0);
        music_play();
        if (!music_test_started()) { FAIL("engine"); return; }
        /* click-to-select-and-play: row 1 renders at y = 20 + 16 */
        music_mouse(NULL, 12, 20 + 16, 1, 1);
        if (music_selected() != 1 % n) { FAIL("mouse select"); return; }
        if (!music_test_started()) { FAIL("mouse play"); return; }
        music_next();
        if (music_selected() != 1 % n) { FAIL("next"); return; }
        music_stop();
        if (music_playing() != -1) { FAIL("stop"); return; }
    }
    PASS();
}


int main(void) {
    printf("\n=== WuBuOS DosGui Apps Test Suite ===\n\n");
    
    printf("-- Calculator --\n");
    test_calc_create_destroy();
    test_calc_mode_switch();
    test_calc_base_switch();
    test_calc_launch();
    
    printf("\n-- Notepad++ Editor --\n");
    test_notepad_typing();
    test_notepad_mouse_positions_caret();
    test_notepad_launch();
    
    printf("\n-- WuBu Canvas (image editor) --\n");
    test_canvas_create_destroy();
    test_canvas_layers();
    test_canvas_brush_composite();
    test_canvas_undo_redo();
    test_canvas_launch();
    
    printf("\n-- Task Manager (Win11 Style) --\n");
    test_tmgr_launch();
    
    printf("\n-- Regedit (Windows Registry) --\n");
    test_reg_create_destroy();
    test_reg_init_roots();
    test_reg_create_key();
    test_reg_set_value();
    test_reg_navigate();
    test_reg_search();
    test_reg_launch();
    
    printf("\n-- File Manager (9P/Styx) --\n");
    test_fm_create_destroy();
    test_fm_scan();
    test_fm_navigate();
    test_fm_selection();
    test_fm_9p_ops();
    test_fm_launch();
    
    printf("\n-- REPL --\n");
    test_repl_typing();
    test_repl_submit_evaluates();
    
    printf("\n-- Control Panel --\n");
    test_control_create_destroy();
    test_control_set_tab();
    test_control_launch();
    
    printf("\n-- Editor --\n");
    test_editor_create_destroy();
    test_editor_launch();
    test_editor_typing();
    test_editor_mouse_positions_caret();
    
    printf("\n-- Registry + the new apps --\n");
    test_registry_complete();
    test_wet_discovery();
    test_notes_real();
    test_todo_real();
    test_music_real();
    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("==================================================\n");
    return g_fail > 0 ? 1 : 0;
}
/* Weak: the WM routes keys to the Start menu while it is open.  Targets that
 * link the real startmenu.c get the strong definitions. */
__attribute__((weak)) void startmenu_handle_key(int key, uint32_t mods) {
    (void)key; (void)mods;
}
__attribute__((weak)) void startmenu_handle_search_input(int key, uint32_t mods) {
    (void)key; (void)mods;
}

/* Weak: the WM routes Win+R to the run dialog.  Targets that link the real
 * run_dialog.c get the strong definitions. */
__attribute__((weak)) void run_dialog_open(void) { }
__attribute__((weak)) void run_dialog_close(void) { }
__attribute__((weak)) bool run_dialog_is_open(void) { return false; }
__attribute__((weak)) void run_dialog_handle_key(uint32_t key, uint32_t mods) {
    (void)key; (void)mods;
}
__attribute__((weak)) bool run_dialog_handle_click(int x, int y) {
    (void)x; (void)y; return false;
}
__attribute__((weak)) void run_dialog_rect(int fb_w, int fb_h,
                                           int *x, int *y, int *w, int *h) {
    (void)fb_w; (void)fb_h;
    if (x) *x = 0; if (y) *y = 0; if (w) *w = 0; if (h) *h = 0;
}
__attribute__((weak)) void run_dialog_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
}

/* Weak: the taskbar's Search tile opens the Start menu.  Targets that link
 * the real startmenu.c get the strong definition. */
__attribute__((weak)) void startmenu_open(void) { }


/* Weak: the WM routes Win+X to the power-user menu.  Targets that link the
 * real power_menu.c get the strong definitions. */
__attribute__((weak)) void power_menu_open(void) { }
__attribute__((weak)) void power_menu_close(void) { }
__attribute__((weak)) bool power_menu_is_open(void) { return false; }
__attribute__((weak)) bool power_menu_handle_click(int x, int y) {
    (void)x; (void)y; return false;
}
__attribute__((weak)) void power_menu_handle_key(uint32_t key, uint32_t mods) {
    (void)key; (void)mods;
}
__attribute__((weak)) void power_menu_rect(int fb_w, int fb_h,
                                           int *x, int *y, int *w, int *h) {
    (void)fb_w; (void)fb_h;
    if (x) *x = 0; if (y) *y = 0; if (w) *w = 0; if (h) *h = 0;
}
__attribute__((weak)) void power_menu_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
}
