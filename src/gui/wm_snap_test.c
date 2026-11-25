/*
 * wm_snap_test.c -- the Win11 window-keyboard geometry as BEHAVIOUR.
 *
 * The shell had a snap-layout flyout that drew six layouts and applied none, and
 * no keyboard snapping at all: Win+Left (the first shortcut a Windows user
 * reaches for) did nothing. wm_snap_half() is the geometry behind Win+Left and
 * Win+Right; this gate pins the arithmetic and the state transitions, because
 *
 *   w = (screen_w - 3*gap) / 2   and   right_x = screen_w - gap - w
 *
 * is exactly the kind of expression that looks right and leaves a 16 px gap on
 * one side only. Win+Up/Win+Down are wm_maximize()/wm_restore()/wm_minimize(),
 * already covered by their own code paths; the KEY BINDING lives in wm_input.c
 * and needs a running WM to exercise, so it is not asserted here.
 */
#include "wm_internal.h"
#include "wubu_gui_metrics.h"

#include <stdio.h>
#include <string.h>

/* The WM owns these in the real build; the test owns them here. */
DosGuiWM g_dwm;
int taskbar_height(void) { return WUBU_METRIC_TASKBAR_H; }
DosGuiWindow *wm_create(int x, int y, int w, int h, const char *title) {
    (void)x; (void)y; (void)w; (void)h; (void)title; return NULL;
}

static int g_fail = 0;
#define CHECK(c, msg) do { \
    if (c) printf("  [PASS] %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); g_fail++; } } while (0)

static int g_resizes = 0;
static void on_resize(DosGuiWindow *win, int w, int h) { (void)win; (void)w; (void)h; g_resizes++; }

int main(void) {
    memset(&g_dwm, 0, sizeof g_dwm);
    g_dwm.screen_w = 1024;
    g_dwm.screen_h = 768;

    const int gap = WUBU_METRIC_GAP_SM;
    const int work_h = 768 - WUBU_METRIC_TASKBAR_H;
    const int half_w = (1024 - 3 * gap) / 2;

    DosGuiWindow win;
    memset(&win, 0, sizeof win);
    win.x = 100; win.y = 100; win.w = 400; win.h = 300;
    win.on_resize = on_resize;

    printf("== Win11 keyboard snap geometry (%dx%d, gap %d) ==\n", 1024, 768, gap);

    wm_snap_half(&win, -1);
    CHECK(win.x == gap, "left half starts one gap in");
    CHECK(win.w == half_w, "left half is (screen - 3 gaps) / 2");
    CHECK(win.y == gap && win.h == work_h - 2 * gap, "vertically inside the work area");
    CHECK(!wm_is_maximized(&win), "a snapped window is NOT maximized (Win+Down must restore it)");

    wm_snap_half(&win, +1);
    CHECK(win.x == 1024 - gap - half_w, "right half ends one gap from the edge");
    CHECK(win.x + win.w == 1024 - gap, "right half touches the gap, not the glass");
    CHECK(win.w == half_w, "both halves are the same width");

    /* A maximized window must be un-maximized before being halved, or it would
     * keep the MAXIMIZED flag and Win+Down would restore a half rather than the
     * floating rect. */
    wm_maximize(&win);
    CHECK(wm_is_maximized(&win), "maximize sets the flag");
    wm_snap_half(&win, -1);
    CHECK(!wm_is_maximized(&win), "snapping a maximized window clears MAXIMIZED");
    CHECK(win.w == half_w, "and gives it the half width");

    /* A minimized window cannot be seen; snapping must bring it back. */
    wm_minimize(&win);
    CHECK(wm_is_minimized(&win), "minimize sets the flag");
    wm_snap_half(&win, +1);
    CHECK(!wm_is_minimized(&win), "snapping a minimized window restores it");

    /* Six, and the count is worth stating: three snaps, then maximize, then the
     * snap that un-maximizes (wm_restore fires one) and re-sizes (one), then the
     * snap that restores a minimized window and sizes it. An expectation of 5
     * here was wrong -- the gate caught the test, which is the point of counting
     * side effects instead of asserting a rectangle. */
    CHECK(g_resizes == 6, "on_resize fires on every real geometry change (6)");

    /* The work area is what a snap may use: a half must never cover the taskbar. */
    CHECK(win.y + win.h <= work_h, "no part of a snapped window covers the taskbar");

    printf("%s\n", g_fail ? "FAILED" : "PASS");
    return g_fail ? 1 : 0;
}
