/*
 * wubu_desktop_shot.c  --  Full desktop-environment verification capture
 *
 * Exercises EVERY a11y action + desktop chrome in one run, capturing a PPM
 * frame after each step so the frames can be triple-verified:
 *
 *   0. Clean desktop (wallpaper + taskbar + Start)
 *   1. Window created; a11y cluster visible (4 controls)
 *   2. Green A-orb drag -> window MOVED
 *   3. Yellow Y-pill CLICK -> window MINIMIZED (taskbar button appears)
 *   4. Taskbar button click -> window RESTORED
 *   5. Yellow Y-pill DRAG -> window ROTATED 90° (w/h swapped, center kept)
 *   6. Purple corner drag -> window RESIZED (wider + taller)
 *   7. Red B-orb click -> window CLOSED
 *   8. Second window + click-drag B on it -> PURGE + close
 *
 * Every pixel comes from the real WM + theme + a11y renderer.
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

/* Hosted test stubs (see wubu_a11y_shot.c). */
int mem_init(size_t n) { (void)n; return 0; }
void *mem_alloc(size_t size) { (void)size; return (void*)1; }
int input_init(void) { return 0; }
#include "src/gui/dosgui_wm.h"
#include "src/gui/wubu_theme.h"
#include "src/gui/wubu_a11y.h"
#include "src/tools/screenshot.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SHOT_W 1024
#define SHOT_H 768
#define OUT_DIR "/tmp/wubu_desktop_shots"

static int g_frame = 0;
static void render_frame(const char *label) {
    char path[512];
    snprintf(path, sizeof(path), OUT_DIR "/%02d_%s.ppm", g_frame, label);
    wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    printf("  frame %02d: %s -> %s\n", g_frame, label, path);
    g_frame++;
}

/* Cluster geometry — must match WubaA11yCluster in wubu_a11y.c. */
#define PANEL_OFFX (-8)
#define PANEL_OFFY (-8)
static void y_center(int wx, int wy, int *cx, int *cy) {
    int px = wx + PANEL_OFFX, py = wy + PANEL_OFFY;
    *cx = px + 26;  *cy = py + 13;
}
static void a_center(int wx, int wy, int *cx, int *cy) {
    int px = wx + PANEL_OFFX, py = wy + PANEL_OFFY;
    *cx = px + 26;  *cy = py + 52;
}
static void b_center(int wx, int wy, int *cx, int *cy) {
    int px = wx + PANEL_OFFX, py = wy + PANEL_OFFY;
    *cx = px + 58;  *cy = py + 52;
}
static void corner_center(int wx, int wy, int w, int h, int *cx, int *cy) {
    *cx = wx + w - 22;  *cy = wy + h - 22;
}

static void tick(void) {
    dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
    vbe_swap();
}

int main(void) {
    srand(42);
    printf("WuBuOS desktop verification capture: %dx%d\n", SHOT_W, SHOT_H);
    system("rm -rf " OUT_DIR);
    system("mkdir -p " OUT_DIR);

    mem_init(8 * 1024 * 1024);
    if (vbe_init(SHOT_W, SHOT_H) != 0) {
        printf("vbe_init failed\n");
        return 1;
    }
    input_init();

    wubu_theme_set(THEME_WIN98_CLASSIC);
    dosgui_wm_init(SHOT_W, SHOT_H);
    wubu_a11y_set_enabled(true);

    /* Frame 0: clean desktop. */
    tick();
    render_frame("00_clean_desktop");

    /* Frame 1: window with cluster. */
    DosGuiWindow *win = dosgui_wm_create(200, 150, 340, 220, "A11y Demo");
    if (!win) { printf("window creation failed\n"); return 1; }
    dosgui_wm_set_focus(win);
    tick();
    render_frame("01_cluster_over_window");

    /* Frame 2: green A drag -> move. */
    {
        int ax, ay;
        a_center(win->x, win->y, &ax, &ay);
        dosgui_wm_handle_mouse(ax, ay, 1, 1);
        tick();
        dosgui_wm_handle_mouse(ax + 50, ay + 30, 1, 0);
        dosgui_wm_handle_mouse(ax + 50, ay + 30, 1, 2);
        tick();
        render_frame("02_green_drag_move");
    }

    /* Frame 3: yellow Y click -> minimize. */
    {
        int ycx, ycy;
        y_center(win->x, win->y, &ycx, &ycy);
        dosgui_wm_handle_mouse(ycx, ycy, 1, 1);
        tick();
        dosgui_wm_handle_mouse(ycx, ycy, 1, 2);
        tick();
        render_frame("03_yellow_minimize");
    }

    /* Frame 4: click the taskbar button -> restore. */
    {
        /* Taskbar button sits at x = 72 + n*titlewidth... first button. */
        int task_h = dosgui_taskbar_height();
        int by = SHOT_H - task_h + (task_h - 22) / 2;
        int bx = 72;   /* Start button w = 60 + margins */
        dosgui_wm_handle_mouse(bx + 10, by + 10, 1, 1);
        tick();
        dosgui_wm_handle_mouse(bx + 10, by + 10, 1, 2);
        tick();
        render_frame("04_taskbar_restore");
    }

    /* Frame 5: yellow Y DRAG -> rotate 90°. */
    {
        int ycx, ycy;
        y_center(win->x, win->y, &ycx, &ycy);
        dosgui_wm_handle_mouse(ycx, ycy, 1, 1);       /* press on pill */
        tick();
        dosgui_wm_handle_mouse(ycx + 20, ycy, 1, 0);  /* drag 20px right */
        dosgui_wm_handle_mouse(ycx + 20, ycy, 1, 2);  /* release -> rotate */
        tick();
        render_frame("05_yellow_drag_rotate");
    }

    /* Frame 6: purple corner drag -> resize. */
    {
        int kx, ky;
        corner_center(win->x, win->y, win->w, win->h, &kx, &ky);
        dosgui_wm_handle_mouse(kx, ky, 1, 1);
        tick();
        dosgui_wm_handle_mouse(kx + 60, ky + 40, 1, 0);
        dosgui_wm_handle_mouse(kx + 60, ky + 40, 1, 2);
        tick();
        render_frame("06_purple_corner_resize");
    }

    /* Frame 7: red B click -> close. */
    {
        int bcx, bcy;
        b_center(win->x, win->y, &bcx, &bcy);
        dosgui_wm_handle_mouse(bcx, bcy, 1, 1);
        tick();
        dosgui_wm_handle_mouse(bcx, bcy, 1, 2);
        tick();
        render_frame("07_red_close");
    }

    /* Frame 8: second window; B drag -> purge + close. */
    {
        DosGuiWindow *win2 = dosgui_wm_create(400, 300, 300, 180, "Purge Target");
        if (!win2) { printf("window 2 creation failed\n"); return 1; }
        dosgui_wm_set_focus(win2);
        tick();
        render_frame("08_purge_target_window");

        int bcx, bcy;
        b_center(win2->x, win2->y, &bcx, &bcy);
        dosgui_wm_handle_mouse(bcx, bcy, 1, 1);
        tick();
        dosgui_wm_handle_mouse(bcx + 30, bcy + 30, 1, 0);
        dosgui_wm_handle_mouse(bcx + 30, bcy + 30, 1, 2);
        tick();
        render_frame("09_red_drag_purge_close");
    }

    printf("Done. %d frames written to %s\n", g_frame, OUT_DIR);
    return 0;
}
