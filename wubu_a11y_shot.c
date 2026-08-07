/*
 * wubu_a11y_shot.c  --  Real-GUI video capture of the accessibility cluster
 *
 * Drives the genuine compositor (VBE framebuffer + DosGuiWindowManager)
 * through the a11y control cluster, capturing a PPM frame after each
 * interaction:
 *   1. Window created; a11y cluster drawn over it (4 controls visible)
 *   2. Green A-orb: press + drag -> window moves
 *   3. Yellow Y-pill: click -> window minimizes (taskbar button reflects)
 *   4. Purple corner: press + drag -> window resizes
 *   5. Red B-orb: click -> window closes (purged to trash)
 *
 * Every pixel comes from the real WM + theme engine + a11y renderer.
 * No desktop environment needed — just the WM and one window.
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

/* Hosted test stubs: mem_init/input_init live in bare-metal kernel sources
 * (memory.c/input.c) which pull wdt/irq/agi_kernel symbols. The VBE/WM layer
 * only needs a heap and an input queue — stub them here for the capture binary. */
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
#define OUT_DIR "/tmp/wubu_a11y_shots"

static int g_frame = 0;
static void render_frame(const char *label) {
    char path[512];
    snprintf(path, sizeof(path), OUT_DIR "/%02d_%s.ppm", g_frame, label);
    wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    printf("  frame %02d: %s -> %s\n", g_frame, label, path);
    g_frame++;
}

/* Cluster geometry — must match WubaA11yCluster in wubu_a11y.c/h.
 * Panel anchors at (win_x + WUBU_A11Y_PANEL_OFFX, win_y + WUBU_A11Y_PANEL_OFFY)
 * = (win_x - 8, win_y - 8). Orb centers inside the panel: */
#define PANEL_OFFX (-8)
#define PANEL_OFFY (-8)
static void y_center(int wx, int wy, int *cx, int *cy) {
    int px = wx + PANEL_OFFX, py = wy + PANEL_OFFY;
    *cx = px + 26;  /* pill_center: px + A11Y_PILL_W/2 + 8 */
    *cy = py + 13;  /* pill_center: py + A11Y_PILL_H/2 + 6 */
}
static void a_center(int wx, int wy, int *cx, int *cy) {
    int px = wx + PANEL_OFFX, py = wy + PANEL_OFFY;
    *cx = px + 26;  /* orb_a_center */
    *cy = py + 52;  /* orb_a_center */
}
static void b_center(int wx, int wy, int *cx, int *cy) {
    int px = wx + PANEL_OFFX, py = wy + PANEL_OFFY;
    *cx = px + 58;  /* orb_b_center */
    *cy = py + 52;  /* orb_b_center */
}
static void corner_center(int wx, int wy, int w, int h, int *cx, int *cy) {
    *cx = wx + w - 22;  /* corner_rect: bottom-right purple resize corner */
    *cy = wy + h - 22;
}

int main(void) {
    srand(42);
    printf("WuBuOS a11y cluster video capture: %dx%d\n", SHOT_W, SHOT_H);
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

    /* Create a real window for the cluster to anchor on. */
    DosGuiWindow *win = dosgui_wm_create(200, 150, 340, 220, "A11y Demo");
    if (!win) {
        printf("window creation failed\n");
        return 1;
    }
    dosgui_wm_set_focus(win);
    dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);

    /* Frame 1: window with the a11y cluster visible (4 colored orbs + corner). */
    vbe_swap();
    render_frame("01_cluster_over_window");

    /* Frame 2: Green A-orb press + drag -> window moves. */
    {
        int ax, ay;
        a_center(win->x, win->y, &ax, &ay);
        dosgui_wm_handle_mouse(ax, ay, 1, 1);           /* press A (down)  */
        dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
        vbe_swap();
        dosgui_wm_handle_mouse(ax + 50, ay + 30, 1, 0);  /* drag          */
        dosgui_wm_handle_mouse(ax + 50, ay + 30, 1, 2);  /* release         */
        dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
        vbe_swap();
        render_frame("02_green_drag_move");
    }

    /* Frame 3: Yellow Y-pill click -> minimize. */
    {
        int ycx, ycy;
        y_center(win->x, win->y, &ycx, &ycy);
        dosgui_wm_handle_mouse(ycx, ycy, 1, 1);  /* press   */
        dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
        vbe_swap();
        dosgui_wm_handle_mouse(ycx, ycy, 1, 2);  /* release */
        dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
        vbe_swap();
        render_frame("03_yellow_minimize");
    }

    /* Restore window for subsequent tests. */
    dosgui_wm_restore(win);
    dosgui_wm_set_focus(win);
    dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
    vbe_swap();

    /* Frame 4: Purple corner resize drag. */
    {
        int kx, ky;
        corner_center(win->x, win->y, win->w, win->h, &kx, &ky);
        dosgui_wm_handle_mouse(kx, ky, 1, 1);           /* press corner   */
        dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
        vbe_swap();
        dosgui_wm_handle_mouse(kx + 40, ky + 30, 1, 0);  /* drag SE        */
        dosgui_wm_handle_mouse(kx + 40, ky + 30, 1, 2);  /* release        */
        dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
        vbe_swap();
        render_frame("04_purple_resize");
    }

    /* Frame 5: Red B-orb click -> close (purge). */
    {
        int bcx, bcy;
        b_center(win->x, win->y, &bcx, &bcy);
        dosgui_wm_handle_mouse(bcx, bcy, 1, 1);  /* press   */
        dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
        vbe_swap();
        dosgui_wm_handle_mouse(bcx, bcy, 1, 2);  /* release -> close */
        dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
        vbe_swap();
        render_frame("05_red_close_purge");
    }

    printf("Done. %d frames written to %s\n", g_frame, OUT_DIR);
    return 0;
}
