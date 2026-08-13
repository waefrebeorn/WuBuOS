/*
 * wubu_desktop_shot.c  --  Full desktop-environment verification capture.
 *
 * Exercises EVERY a11y action + desktop chrome + Bonzi Buddy companion in one
 * run, capturing a PPM frame after each step so the frames can be triple-DA:
 *
 *   0.  Clean desktop (wallpaper + taskbar + Start + Bonzi Buddy)
 *   1.  Window created; a11y cluster visible (4 controls)
 *   2.  Green A-orb drag -> window MOVED
 *   3.  Yellow Y-pill CLICK -> window MINIMIZED (taskbar button appears)
 *   4.  Taskbar button click -> window RESTORED
 *   5.  Yellow Y-pill DRAG -> window ROTATED 90 (w/h swapped, center kept)
 *   6.  Purple corner drag -> window RESIZED (wider + taller)
 *   7.  Red B-orb click -> window CLOSED
 *   8.  Second window + click-drag B on it -> PURGE + close
 *   9.  Start menu opens; Companion toggle checkbox visible (enabled)
 *  10.  Toggle Companion off via Start menu; Bonzi Buddy disappears
 *
 * Every pixel comes from the real WM + theme + a11y + bonzi renderer.
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
#include "src/gui/wubu_bonzi.h"
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

/* Cluster geometry — must match wubu_a11y.c. Reference-proportional
 * (reference_trace.svg, 1280x1156 canvas): green A (0.363w, 0.401h) LEFT
 * of red B (0.579w, 0.402h) SAME row; yellow crescent (0.289w, 0.216h)
 * up-left. Radii 2.6:1 (real GC hardware: A 16.747mm, B 6.449mm). Purple
 * resize crescents at the window's bottom corners. */
/* Cluster click positions — must match wubu_a11y.c CORNER-ANCHORED
 * geometry (GameCube cluster at top-left corner, purple beans at bottom
 * corners). For the Win98 theme: border_width=1, title_bar_height=20.
 *   anchor = (wx + 1 + 4, wy + 20 + 4) = (wx+5, wy+24)
 *   Y = anchor + (16,16) = (wx+21, wy+40)
 *   A = anchor + (30,42) = (wx+35, wy+66)
 *   B = anchor + (64,42) = (wx+69, wy+66)
 *   purple BL = (wx+23, wy+h-23); BR = (wx+w-23, wy+h-23) */
static void y_center(int wx, int wy, int w, int h, int *cx, int *cy) {
    (void)w; (void)h; *cx = wx + 21; *cy = wy + 40;
}
static void a_center(int wx, int wy, int w, int h, int *cx, int *cy) {
    (void)w; (void)h; *cx = wx + 35; *cy = wy + 66;
}
static void b_center(int wx, int wy, int w, int h, int *cx, int *cy) {
    (void)w; (void)h; *cx = wx + 69; *cy = wy + 66;
}
static void p_bl(int wx, int wy, int w, int h, int *cx, int *cy) {
    (void)w; *cx = wx + 23; *cy = wy + h - 23;
}
static void p_br(int wx, int wy, int w, int h, int *cx, int *cy) {
    *cx = wx + w - 23; *cy = wy + h - 23;
}

static void tick(void) {
    dosgui_wm_render_desktop(NULL, SHOT_W, SHOT_H);
    if (dosgui_startmenu_is_open())
        dosgui_startmenu_render(NULL, SHOT_W, SHOT_H);
    vbe_swap();
}

/* Start menu click helpers. The menu opens from the bottom-left. */
static int start_btn_cx(void) {
    int task_h = dosgui_taskbar_height();
    return 32;  /* center-ish of 60px Start button */
}
static int start_btn_cy(void) {
    int task_h = dosgui_taskbar_height();
    return SHOT_H - task_h + 12;
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

    /* Desktop icons (grid 0..2, row 0 -> x=20, y=20 + 60*col). */
    dosgui_icon_add_ex("Computer",  DESK_ICON_APP,  NULL, 0, 0, 0, NULL);
    dosgui_icon_add_ex("Documents", DESK_ICON_FOLDER, NULL, 1, 0, 0, NULL);
    dosgui_icon_add_ex("Recycle",   DESK_ICON_FILE, NULL, 2, 0, 0, NULL);

    /* Frame 0: clean desktop (wallpaper gradient + taskbar + Start + buddy). */
    tick();
    render_frame("00_clean_desktop");

    /* Frames 0a-0h: idle animation (bob cycle + blink), 8 frames at ~300ms each.
     * Demonstrates the smooth cosine bob, squash/stretch, and blink cycle. */
    for (int i = 0; i < 8; i++) {
        tick();
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "00a_idle_%d", i);
        render_frame(lbl);
    }

    /* Frame 1: window with cluster. */
    DosGuiWindow *win = dosgui_wm_create(200, 150, 340, 220, "A11y Demo");
    if (!win) { printf("window creation failed\n"); return 1; }
    dosgui_wm_set_focus(win);
    tick();
    render_frame("01_cluster_over_window");

    /* Frame 1b: purple Y crescents REVEAL when the cursor nears the window's
     * bottom edge (Windows edge-detection — invisible until hovered). */
    {
        int pxx, pyy;
        p_bl(win->x, win->y, win->w, win->h, &pxx, &pyy);
        dosgui_wm_handle_mouse(pxx, pyy, 1, 0);   /* hover near purple BL */
        tick();
        render_frame("01b_purple_reveal");
    }

    /* Frame 2: green A drag -> move. */
    {
        int ax, ay;
        a_center(win->x, win->y, win->w, win->h, &ax, &ay);
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
        y_center(win->x, win->y, win->w, win->h, &ycx, &ycy);
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

    /* Frame 5: yellow Y DRAG -> rotate 90. */
    {
        int ycx, ycy;
        y_center(win->x, win->y, win->w, win->h, &ycx, &ycy);
        dosgui_wm_handle_mouse(ycx, ycy, 1, 1);       /* press on pill */
        tick();
        dosgui_wm_handle_mouse(ycx + 20, ycy, 1, 0);  /* drag 20px right */
        dosgui_wm_handle_mouse(ycx + 20, ycy, 1, 2);  /* release -> rotate */
        tick();
        render_frame("05_yellow_drag_rotate");
    }

    /* Frame 6: purple Y crescent drag (bottom-left) -> resize. */
    {
        int kx, ky;
        p_bl(win->x, win->y, win->w, win->h, &kx, &ky);
        dosgui_wm_handle_mouse(kx, ky, 1, 1);
        tick();
        dosgui_wm_handle_mouse(kx + 60, ky + 40, 1, 0);
        dosgui_wm_handle_mouse(kx + 60, ky + 40, 1, 2);
        tick();
        render_frame("06_purple_crescent_resize");
    }

    /* Frame 7: red B click -> close. */
    {
        int bcx, bcy;
        b_center(win->x, win->y, win->w, win->h, &bcx, &bcy);
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
        b_center(win2->x, win2->y, win2->w, win2->h, &bcx, &bcy);
        dosgui_wm_handle_mouse(bcx, bcy, 1, 1);
        tick();
        dosgui_wm_handle_mouse(bcx + 30, bcy + 30, 1, 0);
        dosgui_wm_handle_mouse(bcx + 30, bcy + 30, 1, 2);
        tick();
        render_frame("09_red_drag_purge_close");

        /* Frame 9b: STIM — a PARTIAL red drag (< 30px) is feedback, not a
         * close: the orb pulses with a warm ring, the window stays. */
        DosGuiWindow *win3 = dosgui_wm_create(260, 260, 240, 160, "Stim Demo");
        if (win3) {
            dosgui_wm_set_focus(win3);
            tick();
            int bx3, by3;
            b_center(win3->x, win3->y, win3->w, win3->h, &bx3, &by3);
            dosgui_wm_handle_mouse(bx3, by3, 1, 1);
            tick();
            dosgui_wm_handle_mouse(bx3 + 15, by3 + 15, 1, 0); /* partial */
            dosgui_wm_handle_mouse(bx3 + 15, by3 + 15, 1, 2); /* release */
            tick(); tick(); tick();   /* ~3 frames of the pulse ring */
            render_frame("09b_red_stim_pulse");
            printf("  [stim] window alive after partial drag: %d\n",
                   (int)win3->alive);
            dosgui_wm_destroy(win3);
        }
    }

    /* Frame 10: open Start menu (via simulated Start button press). */
    {
        int task_h = dosgui_taskbar_height();
        int by = SHOT_H - task_h + (task_h - 24) / 2;
        dosgui_wm_handle_mouse(10, by + 12, 1, 1);  /* press Start */
        tick();
        dosgui_wm_handle_mouse(10, by + 12, 1, 2);  /* release Start */
        tick();
        render_frame("10_startmenu_open");
    }

    /* Frame 11: click the Companion toggle item -> Bonzi Buddy disappears. */
    {
        int task_h = dosgui_taskbar_height();
        int mh = 24;
        int menu_y = SHOT_H - task_h - (10 * mh + 4);
        int cx = 56 + 18;  /* content_x + checkbox + gap */
        int cy = menu_y + 2 + 9 * mh + mh / 2;
        dosgui_wm_handle_mouse(cx, cy, 1, 1);  /* press Companion */
        tick();
        dosgui_wm_handle_mouse(cx, cy, 1, 2);  /* release */
        tick();
        render_frame("11_toggle_companion_off");
    }

    /* Re-enable the companion for the remaining frames. */
    wubu_bonzi_set_enabled(true);
    tick();

    /* Frame 12: rubber-band drag-select lasso (Classic Mac / Win98 lesson).
     * Icons sit at (20,20), (80,20), (140,20) [32x32 boxes, labels below].
     * Press empty desktop at (10,15), drag to (180,55): the band covers all
     * three icon boxes. */
    {
        dosgui_wm_handle_mouse(10, 15, 1, 1);   /* press empty desktop */
        tick();
        dosgui_wm_handle_mouse(180, 55, 1, 0);  /* drag to cover the icons */
        tick();
        render_frame("12_lasso_drag");
        dosgui_wm_handle_mouse(180, 55, 1, 2);  /* release -> select */
        tick();
        render_frame("13_lasso_selected");
        int nsel = 0;
        for (int i = 0; i < 3; i++)
            if (dosgui_icon_get(i) && dosgui_icon_get(i)->selected) nsel++;
        printf("  [lasso] icons selected: %d/3\n", nsel);
    }

    /* Frame 14: Balloon Help (System 7 lesson) — hover an icon, the buddy
     * bubble retargets to that icon's tip. */
    {
        dosgui_wm_handle_mouse(36, 36, 1, 0);   /* hover "Computer" icon */
        tick();
        render_frame("14_balloon_tip");
    }

    /* Frame 15: click the taskbar clock well -> its own popup menu
     * (Win98 parity): date + time + mini calendar. */
    {
        tick();  /* ensure the well rect is current */
        int wx, wy, ww, wh;
        dosgui_clock_well(&wx, &wy, &ww, &wh);
        printf("  [clock] well=(%d,%d %dx%d) menu=%d\n",
               wx, wy, ww, wh, dosgui_clock_menu_is_open());
        int cxw = wx + ww / 2;
        int cyw = wy + wh / 2;
        dosgui_wm_handle_mouse(cxw, cyw, 1, 1);  /* press clock well */
        tick();
        dosgui_wm_handle_mouse(cxw, cyw, 1, 2);  /* release */
        tick();
        render_frame("15_clock_menu");
        printf("  [clock] menu open after click: %d\n", dosgui_clock_menu_is_open());
        dosgui_clock_menu_close();
        tick();
    }

    /* Human-lag cursor: a long mouse jump, then render — the DRAWN cursor
     * must be partway (eased), not instantly at the target. */
    {
        int cx0, cy0;
        dosgui_wm_cursor_pos(&cx0, &cy0);
        dosgui_wm_handle_mouse(512, 384, 1, 0);  /* teleport the target */
        tick();                                   /* one eased frame */
        int cx1, cy1;
        dosgui_wm_cursor_pos(&cx1, &cy1);
        printf("  [cursor] eased %d,%d -> %d,%d (target 512,384) after 1 frame\n",
               cx0, cy0, cx1, cy1);
        /* 2 more frames: should keep converging */
        tick(); tick();
        int cx2, cy2;
        dosgui_wm_cursor_pos(&cx2, &cy2);
        printf("  [cursor] after 3 frames: %d,%d\n", cx2, cy2);
    }

    /* Human typing: keys land with human rhythm, not all at once. */
    {
        DosGuiWindow *tw = dosgui_wm_create(300, 200, 320, 160, "Type Test");
        if (tw) {
            dosgui_wm_typer_start(tw, "hello wubu");
            int p0 = dosgui_wm_typer_pos();
            for (int i = 0; i < 4; i++) tick();   /* ~64ms: reaction delay */
            int p1 = dosgui_wm_typer_pos();
            for (int i = 0; i < 30; i++) tick();  /* ~480ms more */
            int p2 = dosgui_wm_typer_pos();
            int busy = dosgui_wm_typer_busy();
            printf("  [typer] pos after 0/4/34 ticks: %d/%d/%d (len 10), busy=%d\n",
                   p0, p1, p2, busy);
            dosgui_wm_destroy(tw);
        }
    }

    printf("Done. %d frames written to %s\n", g_frame, OUT_DIR);
    return 0;
}
