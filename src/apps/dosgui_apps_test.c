/*
 * dosgui_apps_test.c  --  WuBuOS DosGui Apps Test Suite
 *
 * Tests for built-in Win98-style apps:
 * - Task Manager (Windows 11 style)
 * - Regedit (Windows Registry Editor clone)
 * - Calculator (Standard, Scientific, Graphing)
 * - Notepad++ (Editor with tabs, syntax highlighting)
 * - WuBu Canvas (layered Photoshop-class image editor)
 * - File Manager (9P/Styx operations)
 */

#include "dosgui_apps.h"
#include "calc/calc.h"
#include "notepad/notepad.h"
#include "wubu_canvas.h"
#include "taskmgr/taskmgr.h"
#include "regedit/regedit.h"
#include "fm/fm.h"
#include "repl/repl.h"
#include "control/control.h"
#include "editor/editor.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Stubs for WM functions not needed by tests */
DosGuiWindow* dosgui_wm_spawn_holyc_term(int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h;
    return NULL;
}

/* Also stub out the external functions in the app registry */
void dosgui_explorer_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h;
}

void dosgui_terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h;
}

/* Stubs for container functions */
int wubu_ct_start_bwrap(void* ct) { (void)ct; return 0; }
void wubu_ct_destroy(void* ct) { (void)ct; }

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

static void test_notepad_create_destroy(void) {
    TEST("notepad: create and destroy");
    NotepadState *np = notepad_create();
    CHECK(np != NULL, "notepad created");
    CHECK(np_tab_count(np) == 0, "tab count 0");
    notepad_destroy(np);
    PASS();
}

static void test_notepad_new_tab(void) {
    TEST("notepad: new tab creation via API");
    NotepadState *np = notepad_create();
    int old_count = np_tab_count(np);
    notepad_new_tab(np);
    CHECK(np_tab_count(np) == old_count + 1, "tab count increased");
    CHECK(np_active_tab(np) == np_tab_count(np) - 1, "new tab active");
    notepad_destroy(np);
    PASS();
}

static void test_notepad_lang_detect(void) {
    TEST("notepad: language detection via API");
    CHECK(notepad_detect_lang("test.c") == NP_LANG_C, ".c -> C");
    CHECK(notepad_detect_lang("test.cpp") == NP_LANG_CPP, ".cpp -> C++");
    CHECK(notepad_detect_lang("test.py") == NP_LANG_PYTHON, ".py -> Python");
    CHECK(notepad_detect_lang("test.HC") == NP_LANG_HOLYC, ".HC -> HolyC");
    CHECK(notepad_detect_lang("test.sh") == NP_LANG_SHELL, ".sh -> Shell");
    CHECK(notepad_detect_lang("Makefile") == NP_LANG_MAKEFILE, "Makefile");
    CHECK(notepad_detect_lang("test.json") == NP_LANG_JSON, ".json -> JSON");
    CHECK(notepad_detect_lang("test.xml") == NP_LANG_XML, ".xml -> XML");
    CHECK(notepad_detect_lang("test.txt") == NP_LANG_NONE, ".txt -> None");
    PASS();
}

static void test_notepad_launch(void) {
    TEST("notepad: launch returns window");
    DosGuiWindow *win = notepad_launch();
    CHECK(win != NULL, "window created");
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
    DosGuiWindow *win = dosgui_launch_canvas();
    CHECK(win != NULL, "window created");
    PASS();
}

/* ================================================================
 * Task Manager Tests (Windows 11 Style)
 * ================================================================ */

static void test_tmgr_create_destroy(void) {
    TEST("taskmgr: create and destroy");
    TaskManagerState *tm = taskmgr_create();
    CHECK(tm != NULL, "taskmgr created");
    taskmgr_destroy(tm);
    PASS();
}

static void test_tmgr_refresh(void) {
    TEST("taskmgr: refresh processes via API");
    TaskManagerState *tm = taskmgr_create();
    taskmgr_refresh_processes(tm);
    CHECK(taskmgr_process_count(tm) == 5, "5 mock processes");
    CHECK(strcmp(taskmgr_process_name(tm, 0), "systemd") == 0, "first is systemd");
    CHECK(taskmgr_process_cpu(tm, 3) == 15.0, "browser 15% CPU");
    CHECK(taskmgr_total_cpu(tm) == 24.4, "total CPU");
    taskmgr_destroy(tm);
    PASS();
}

static void test_tmgr_tab_switch(void) {
    TEST("taskmgr: tab switching via API");
    TaskManagerState *tm = taskmgr_create();
    taskmgr_set_tab(tm, TM_TAB_PERFORMANCE);
    CHECK(taskmgr_get_active_tab(tm) == TM_TAB_PERFORMANCE, "performance tab");
    taskmgr_set_tab(tm, TM_TAB_DETAILS);
    CHECK(taskmgr_get_active_tab(tm) == TM_TAB_DETAILS, "details tab");
    taskmgr_set_tab(tm, TM_TAB_SERVICES);
    CHECK(taskmgr_get_active_tab(tm) == TM_TAB_SERVICES, "services tab");
    taskmgr_set_tab(tm, TM_TAB_STARTUP);
    CHECK(taskmgr_get_active_tab(tm) == TM_TAB_STARTUP, "startup tab");
    taskmgr_set_tab(tm, TM_TAB_USERS);
    CHECK(taskmgr_get_active_tab(tm) == TM_TAB_USERS, "users tab");
    taskmgr_set_tab(tm, TM_TAB_PROCESSES);
    CHECK(taskmgr_get_active_tab(tm) == TM_TAB_PROCESSES, "processes tab");
    taskmgr_destroy(tm);
    PASS();
}

static void test_tmgr_perf_history(void) {
    TEST("taskmgr: performance history via API");
    TaskManagerState *tm = taskmgr_create();
    taskmgr_add_cpu_sample(tm, 10.0);
    taskmgr_add_cpu_sample(tm, 20.0);
    taskmgr_add_cpu_sample(tm, 30.0);
    CHECK(taskmgr_hist_idx(tm) == 3, "3 samples");
    taskmgr_destroy(tm);
    PASS();
}

static void test_tmgr_kill_process(void) {
    TEST("taskmgr: kill process via API");
    TaskManagerState *tm = taskmgr_create();
    taskmgr_refresh_processes(tm);
    int before = taskmgr_process_count(tm);
    taskmgr_kill_process(tm, taskmgr_process_pid(tm, 0));
    CHECK(taskmgr_process_count(tm) == before - 1, "process removed");
    taskmgr_destroy(tm);
    PASS();
}

static void test_tmgr_launch(void) {
    TEST("taskmgr: launch returns window");
    DosGuiWindow *win = taskmgr_launch();
    CHECK(win != NULL, "window created");
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

static void test_repl_create_destroy(void) {
    TEST("repl: create and destroy");
    REPLState *repl = repl_create();
    CHECK(repl != NULL, "repl created");
    repl_destroy(repl);
    PASS();
}

static void test_repl_add_line(void) {
    TEST("repl: add line via API");
    REPLState *repl = repl_create();
    repl_add_line(repl, "test line");
    repl_destroy(repl);
    PASS();
}

static void test_repl_launch(void) {
    TEST("repl: launch returns window");
    DosGuiWindow *win = repl_launch();
    CHECK(win != NULL, "window created");
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

/* ================================================================
 * Main Test Runner
 * ================================================================ */

int main(void) {
    printf("\n=== WuBuOS DosGui Apps Test Suite ===\n\n");
    
    printf("-- Calculator --\n");
    test_calc_create_destroy();
    test_calc_mode_switch();
    test_calc_base_switch();
    test_calc_launch();
    
    printf("\n-- Notepad++ Editor --\n");
    test_notepad_create_destroy();
    test_notepad_new_tab();
    test_notepad_lang_detect();
    test_notepad_launch();
    
    printf("\n-- WuBu Canvas (image editor) --\n");
    test_canvas_create_destroy();
    test_canvas_layers();
    test_canvas_brush_composite();
    test_canvas_undo_redo();
    test_canvas_launch();
    
    printf("\n-- Task Manager (Win11 Style) --\n");
    test_tmgr_create_destroy();
    test_tmgr_refresh();
    test_tmgr_tab_switch();
    test_tmgr_perf_history();
    test_tmgr_kill_process();
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
    test_repl_create_destroy();
    test_repl_add_line();
    test_repl_launch();
    
    printf("\n-- Control Panel --\n");
    test_control_create_destroy();
    test_control_set_tab();
    test_control_launch();
    
    printf("\n-- Editor --\n");
    test_editor_create_destroy();
    test_editor_launch();
    
    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("==================================================\n");
    return g_fail > 0 ? 1 : 0;
}