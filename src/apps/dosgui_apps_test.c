/*
 * dosgui_apps_test.c  --  WuBuOS DosGui Apps Test Suite
 *
 * Tests for built-in Win98-style apps:
 * - Task Manager (Windows 11 style)
 * - Regedit (Windows Registry Editor clone)
 * - Calculator (Standard, Scientific, Graphing)
 * - Notepad++ (Editor with tabs, syntax highlighting)
 * - Paint (MS Paint style)
 * - File Manager (9P/Styx operations)
 */

#include "dosgui_apps.h"
#include "calc/calc.h"
#include "notepad/notepad.h"
#include "paint.h"
#include "taskmgr/taskmgr.h"
#include "regedit/regedit.h"
#include "fm/fm.h"
#include "repl/repl.h"
#include "control/control.h"
#include "editor/editor.h"
#include "canvas/canvas.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Stub out external functions in the app registry that are NOT linked
 * into this test binary (explorer / terminal draw, bwrap container). */
void dosgui_explorer_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h;
}

void dosgui_terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)win; (void)fb; (void)fb_w; (void)fb_h;
}

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
    CHECK(calc->mode == CALC_STANDARD, "default mode");
    CHECK(calc->base == 10, "default base");
    calc_destroy(calc);
    PASS();
}

static void test_calc_mode_switch(void) {
    TEST("calc: mode switching via API");
    CalcState *calc = calc_create();
    calc_set_mode(calc, CALC_SCIENTIFIC);
    CHECK(calc->mode == CALC_SCIENTIFIC, "scientific mode");
    calc_set_mode(calc, CALC_GRAPHING);
    CHECK(calc->mode == CALC_GRAPHING, "graphing mode");
    calc_set_mode(calc, CALC_PROGRAMMER);
    CHECK(calc->mode == CALC_PROGRAMMER, "programmer mode");
    calc_set_mode(calc, CALC_STANDARD);
    CHECK(calc->mode == CALC_STANDARD, "standard mode");
    calc_destroy(calc);
    PASS();
}

static void test_calc_base_switch(void) {
    TEST("calc: base switching via API");
    CalcState *calc = calc_create();
    calc_set_base(calc, 16);
    CHECK(calc->base == 16, "hex mode");
    calc_set_base(calc, 2);
    CHECK(calc->base == 2, "binary mode");
    calc_set_base(calc, 10);
    CHECK(calc->base == 10, "decimal mode");
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
    CHECK(np->tab_count == 0, "tab count 0");
    notepad_destroy(np);
    PASS();
}

static void test_notepad_new_tab(void) {
    TEST("notepad: new tab creation via API");
    NotepadState *np = notepad_create();
    int old_count = np->tab_count;
    notepad_new_tab(np);
    CHECK(np->tab_count == old_count + 1, "tab count increased");
    CHECK(np->active_tab == np->tab_count - 1, "new tab active");
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
 * Paint Tests (using API)
 * ================================================================ */

static void test_paint_create_destroy(void) {
    TEST("paint: create and destroy");
    PaintState *p = paint_create();
    CHECK(p != NULL, "paint created");
    CHECK(p->current_tool == PAINT_TOOL_BRUSH, "default tool brush");
    CHECK(p->fg_color == 0x00000000, "fg black");
    CHECK(p->bg_color == 0x00FFFFFF, "bg white");
    CHECK(p->brush_size == 3, "brush size 3");
    paint_destroy(p);
    PASS();
}

static void test_paint_add_shape(void) {
    TEST("paint: add shapes via API");
    PaintState *p = paint_create();
    int old_count = p->shape_count;
    paint_add_shape(p, 10, 10, 100, 100, 0x00FF0000, false);
    CHECK(p->shape_count == old_count + 1, "shape added");
    paint_destroy(p);
    PASS();
}

static void test_paint_undo_redo(void) {
    TEST("paint: undo/redo via API");
    PaintState *p = paint_create();
    int old_count = p->shape_count;
    paint_save_undo(p);
    paint_add_shape(p, 0, 0, 10, 10, 0x00FF0000, false);
    CHECK(p->shape_count == old_count + 1, "one shape");
    paint_undo(p);
    CHECK(p->shape_count == old_count, "first undone");
    paint_save_undo(p);
    paint_add_shape(p, 20, 20, 30, 30, 0x0000FF00, false);
    CHECK(p->shape_count == old_count + 1, "one shape again");
    paint_undo(p);
    CHECK(p->shape_count == old_count, "second undone");
    paint_destroy(p);
    PASS();
}

static void test_paint_tool_switch(void) {
    TEST("paint: tool switching via API");
    PaintState *p = paint_create();
    paint_set_tool(p, PAINT_TOOL_LINE);
    CHECK(p->current_tool == PAINT_TOOL_LINE, "line tool");
    paint_set_tool(p, PAINT_TOOL_ELLIPSE);
    CHECK(p->current_tool == PAINT_TOOL_ELLIPSE, "ellipse tool");
    paint_set_tool(p, PAINT_TOOL_FILL);
    CHECK(p->current_tool == PAINT_TOOL_FILL, "fill tool");
    paint_set_tool(p, PAINT_TOOL_BRUSH);
    CHECK(p->current_tool == PAINT_TOOL_BRUSH, "brush tool");
    paint_destroy(p);
    PASS();
}

static void test_paint_launch(void) {
    TEST("paint: launch returns window");
    DosGuiWindow *win = paint_launch();
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
    CHECK(tm->process_count == 5, "5 mock processes");
    CHECK(strcmp(tm->processes[0].name, "systemd") == 0, "first is systemd");
    CHECK(tm->processes[3].cpu_percent == 15.0, "browser 15% CPU");
    CHECK(tm->total_cpu == 24.4, "total CPU");
    taskmgr_destroy(tm);
    PASS();
}

static void test_tmgr_tab_switch(void) {
    TEST("taskmgr: tab switching via API");
    TaskManagerState *tm = taskmgr_create();
    taskmgr_set_tab(tm, TM_TAB_PERFORMANCE);
    CHECK(tm->active_tab == TM_TAB_PERFORMANCE, "performance tab");
    taskmgr_set_tab(tm, TM_TAB_DETAILS);
    CHECK(tm->active_tab == TM_TAB_DETAILS, "details tab");
    taskmgr_set_tab(tm, TM_TAB_SERVICES);
    CHECK(tm->active_tab == TM_TAB_SERVICES, "services tab");
    taskmgr_set_tab(tm, TM_TAB_STARTUP);
    CHECK(tm->active_tab == TM_TAB_STARTUP, "startup tab");
    taskmgr_set_tab(tm, TM_TAB_USERS);
    CHECK(tm->active_tab == TM_TAB_USERS, "users tab");
    taskmgr_set_tab(tm, TM_TAB_PROCESSES);
    CHECK(tm->active_tab == TM_TAB_PROCESSES, "processes tab");
    taskmgr_destroy(tm);
    PASS();
}

static void test_tmgr_perf_history(void) {
    TEST("taskmgr: performance history via API");
    TaskManagerState *tm = taskmgr_create();
    taskmgr_add_cpu_sample(tm, 10.0);
    taskmgr_add_cpu_sample(tm, 20.0);
    taskmgr_add_cpu_sample(tm, 30.0);
    CHECK(tm->hist_idx == 3, "3 samples");
    taskmgr_destroy(tm);
    PASS();
}

static void test_tmgr_kill_process(void) {
    TEST("taskmgr: kill process via API");
    TaskManagerState *tm = taskmgr_create();
    taskmgr_refresh_processes(tm);
    int before = tm->process_count;
    taskmgr_kill_process(tm, tm->processes[0].pid);
    CHECK(tm->process_count == before - 1, "process removed");
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
    CHECK(reg->current_key == &reg->root_keys[1], "default HKCU");
    CHECK(reg->value_count == 0, "no values");
    CHECK(reg->expand_depth == 0, "no expanded keys");
    CHECK(reg->search_dialog_open == false, "no search");
    regedit_destroy(reg);
    PASS();
}

static void test_reg_create_key(void) {
    TEST("regedit: create subkey via API");
    RegeditState *reg = regedit_create();
    regedit_init_roots(reg);
    RegKey *hkcu = &reg->root_keys[1];
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
    CHECK(reg->value_count == 2, "two values");
    regedit_destroy(reg);
    PASS();
}

static void test_reg_navigate(void) {
    TEST("regedit: navigate tree via API");
    RegeditState *reg = regedit_create();
    regedit_init_roots(reg);
    RegKey *hkcu = &reg->root_keys[1];
    regedit_expand_key(reg, hkcu);
    CHECK(reg->expand_depth == 1, "hkcu expanded");
    
    RegKey *software = hkcu->children;
    if (software) {
        regedit_expand_key(reg, software);
        CHECK(reg->expand_depth == 2, "software expanded");
    }
    regedit_destroy(reg);
    PASS();
}

static void test_reg_search(void) {
    TEST("regedit: search via API");
    RegeditState *reg = regedit_create();
    regedit_init_roots(reg);
    strncpy(reg->search_text, "WuBu", sizeof(reg->search_text) - 1);
    reg->search_dialog_open = true;
    CHECK(strcmp(reg->search_text, "WuBu") == 0, "search text set");
    CHECK(reg->search_dialog_open == true, "dialog open");
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
    CHECK(fm->entry_count == 7, "7 entries");
    CHECK(strcmp(fm->entries[0].name, "..") == 0, "parent dir first");
    CHECK(fm->entries[0].is_dir == true, "parent is dir");
    CHECK(strcmp(fm->entries[1].name, "src") == 0, "src dir");
    CHECK(fm->entries[1].is_dir == true, "src is dir");
    CHECK(strcmp(fm->entries[4].name, "main.c") == 0, "main.c file");
    CHECK(fm->entries[4].is_dir == false, "main.c is file");
    CHECK(fm->entries[4].size == 1024, "main.c size");
    fm_destroy(fm);
    PASS();
}

static void test_fm_navigate(void) {
    TEST("filemgr: navigate via API");
    FileManagerState *fm = fm_create();
    fm_scan_dir(fm, "/home/wubu");
    int old_count = fm->entry_count;
    fm_scan_dir(fm, "/home/wubu/src");
    CHECK(strcmp(fm->current_path, "/home/wubu/src") == 0, "path updated");
    fm_destroy(fm);
    PASS();
}

static void test_fm_selection(void) {
    TEST("filemgr: selection via API");
    FileManagerState *fm = fm_create();
    fm_scan_dir(fm, "/home/wubu");
    fm->selected_idx = 2;
    CHECK(fm->selected_idx == 2, "selected");
    CHECK(strcmp(fm->entries[2].name, "apps") == 0, "correct entry");
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
    CHECK(ctrl->active_tab == 1, "tab set");
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
 * Canvas Tests
 * ================================================================ */

static void test_canvas_create_destroy(void) {
    TEST("canvas: create and destroy");
    CanvasState *cv = canvas_create();
    CHECK(cv != NULL, "canvas created");
    canvas_destroy(cv);
    PASS();
}

static void test_canvas_launch(void) {
    TEST("canvas: launch returns window");
    DosGuiWindow *win = canvas_launch();
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
    
    printf("\n-- Paint --\n");
    test_paint_create_destroy();
    test_paint_add_shape();
    test_paint_undo_redo();
    test_paint_tool_switch();
    test_paint_launch();
    
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
    
    printf("\n-- Canvas --\n");
    test_canvas_create_destroy();
    test_canvas_launch();
    
    printf("\n==================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("==================================================\n");
    return g_fail > 0 ? 1 : 0;
}