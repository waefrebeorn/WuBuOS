/*
 * wubu_gui_shot.c  --  Real WuBuOS GUI capture harness
 *
 * Inits the genuine compositor (VBE + DosGui WM + desktop), launches REAL app
 * engines through the app registry (not placeholders), renders through the
 * single dosgui_wm_render() entry point, and writes PPM frames. This is the
 * honest way to verify the GUI: every pixel comes from the actual WM, theme
 * engine, and wired app draw callbacks.
 *
 * Minimal includes; depends only on public headers.
 */

#ifndef VBE_HOSTED
#define VBE_HOSTED
#endif
#ifndef WUBU_NO_LIBM
#define WUBU_NO_LIBM
#endif
#include "src/kernel/vbe.h"
#include "src/kernel/memory.h"
#include "src/kernel/input.h"
#include "src/gui/dosgui_wm.h"
#include "src/gui/dosgui_desktop.h"
#include "src/gui/wubu_theme.h"
#include "src/apps/dosgui_apps.h"
#include "src/tools/screenshot.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SHOT_W 1024
#define SHOT_H 768
#define OUT_DIR "/tmp/wubu_gui_shots"

static void render_frame(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), OUT_DIR "/%s.ppm", name);
    wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    printf("  captured %s\n", path);
}

int main(void) {
    srand(12345);  /* deterministic layout so captured frames are reproducible */
    printf("WuBuOS real-GUI capture: %dx%d\n", SHOT_W, SHOT_H);
    system("rm -rf " OUT_DIR);
    system("mkdir -p " OUT_DIR);

    mem_init(8 * 1024 * 1024);
    if (vbe_init(SHOT_W, SHOT_H) != 0) { printf("vbe_init failed\n"); return 1; }
    input_init();

    /* Phase 1: Win98 Classic desktop - set theme BEFORE init so desktop uses it */
    wubu_theme_set(THEME_WIN98_CLASSIC);
    dosgui_wm_init(SHOT_W, SHOT_H);
    dosgui_desktop_init();

    /* Phase 1: clean Win98 Classic desktop with real icons. */
    dosgui_desktop_tick();
    dosgui_desktop_render(NULL, SHOT_W, SHOT_H);
    vbe_swap();
    render_frame("01_desktop_win98");

    /* Phase 2: launch genuine app engines through the registry. */
    /* Use a deterministic TILED, NON-OVERLAPPING layout so every app's window */
    /* is independently visible and the captured frame is honestly verifiable */
    /* (no app hidden behind another). Positions are fixed; the random seed is */
    /* kept only so any app-internal placement stays reproducible. */
    DosGuiWindow *calc = dosgui_app_launch_by_name("Calculator");   /* (179,81) 280x380 */
    DosGuiWindow *note = dosgui_app_launch_by_name("Notepad");      /* (100,80) 500x400 */
    DosGuiWindow *ctrl = dosgui_app_launch_by_name("Control Panel");/* (80,60) 520x440 */
    DosGuiWindow *expl = dosgui_app_launch_by_name("File Manager"); /* (100,80) 800x600 */
    DosGuiWindow *canvas = dosgui_app_launch_by_name("WuBu Canvas");/* (120,90) 900x640 */
    /* Re-tile to avoid overlap: place each in its own quadrant column. */
    if (calc)   { calc->x = 24;   calc->y = 56;   calc->w = 300;   calc->h = 420; }
    if (note)   { note->x = 344;  note->y = 56;   note->w = 360;   note->h = 300; }
    if (ctrl)   { ctrl->x = 344;  ctrl->y = 372;  ctrl->w = 360;   ctrl->h = 340; }
    if (expl)   { expl->x = 724;  expl->y = 56;   expl->w = 280;   expl->h = 656; }
    if (canvas) { canvas->x = 24;  canvas->y = 492; canvas->w = 300; canvas->h = 220; }
    /* Bring the Calculator to the front so its title renders active (navy) —
     * an honest capture shows a focused app, not a grayed-out background one. */
    if (calc) dosgui_wm_set_focus(calc);
    fprintf(stderr, "[layout] calc=(%d,%d,%d,%d) note=(%d,%d,%d,%d) ctrl=(%d,%d,%d,%d) expl=(%d,%d,%d,%d) canvas=(%d,%d,%d,%d)\n",
            calc?calc->x:0, calc?calc->y:0, calc?calc->w:0, calc?calc->h:0,
            note?note->x:0, note?note->y:0, note?note->w:0, note?note->h:0,
            ctrl?ctrl->x:0, ctrl?ctrl->y:0, ctrl?ctrl->w:0, ctrl?ctrl->h:0,
            expl?expl->x:0, expl?expl->y:0, expl?expl->w:0, expl?expl->h:0,
            canvas?canvas->x:0, canvas?canvas->y:0, canvas?canvas->w:0, canvas?canvas->h:0);

    /* Drive the calculator with a real computation so the display shows 42. */
    if (calc && calc->on_key) {
        /* 6 * 7 = 42 */
        calc->on_key(calc, '6', 0);
        calc->on_key(calc, '*', 0);
        calc->on_key(calc, '7', 0);
        calc->on_key(calc, '=', 0);
    }
    /* Type into Notepad so it shows real text. */
    if (note && note->on_key) {
        const char *msg = "WuBuOS Notepad - real text engine";
        for (const char *p = msg; *p; p++) note->on_key(note, (uint32_t)*p, 0);
        note->on_key(note, '\r', 0);
        note->on_key(note, 'b', 0); note->on_key(note, 'e', 0);
        note->on_key(note, ' ', 0); note->on_key(note, 't', 0);
        note->on_key(note, 'h', 0); note->on_key(note, 'e', 0);
    }
    /* File Manager: navigate once so it shows a real directory listing. */
    if (expl && expl->on_key) { expl->on_key(expl, '/', 0); expl->on_key(expl, '\r', 0); }

    dosgui_desktop_tick();
    dosgui_desktop_render(NULL, SHOT_W, SHOT_H);
    vbe_swap();
    render_frame("02_apps_win98");

    /* Phase 3: Control Panel tab cycling (real engine). */
    if (ctrl && ctrl->on_key) { ctrl->on_key(ctrl, '.', 0); ctrl->on_key(ctrl, '.', 0); }
    dosgui_desktop_tick();
    dosgui_desktop_render(NULL, SHOT_W, SHOT_H);
    vbe_swap();
    render_frame("03_controlpanel");

    /* Phase 4: XP Luna Blue theme (gradient titles, Luna taskbar). */
    wubu_theme_set(THEME_XP_LUNA_BLUE);
    dosgui_desktop_tick();
    dosgui_desktop_render(NULL, SHOT_W, SHOT_H);
    vbe_swap();
    render_frame("04_apps_xp");

    /* Phase 5: WuBu Green theme. */
    wubu_theme_set(THEME_WUBU_CUSTOM);
    dosgui_desktop_tick();
    dosgui_desktop_render(NULL, SHOT_W, SHOT_H);
    vbe_swap();
    render_frame("05_apps_wubu");

    printf("Done. Frames in %s\n", OUT_DIR);
    return 0;
}
