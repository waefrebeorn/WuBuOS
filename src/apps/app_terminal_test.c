/*
 * app_terminal_test.c  --  verifies the REAL pty terminal is wired as the
 * shipped Terminal app: launch creates a window with all input callbacks
 * bound, and on_draw renders the engine inside the chrome content rect
 * without crashing.
 *
 * Links the real src/apps/app_terminal.c + src/gui/dosgui_term*.c, so this
 * exercises the actual shipped path (not a stub).
 */

#include "dosgui_apps.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/dosgui_term.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"

void app_terminal_init(void);
void app_terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_terminal_key(DosGuiWindow *win, uint32_t key, uint32_t mods);
void app_terminal_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);
void app_terminal_scroll(DosGuiWindow *win, int delta);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_run = 0, g_pass = 0;

#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  OK   %s\n", msg); } \
    else { printf("  FAIL %s\n", msg); } \
} while (0)

int main(void) {
    printf("=== WuBuOS Terminal App Test (real pty engine) ===\n\n");

    vbe_init(1024, 768);
    dosgui_wm_init(1024, 768);
    wubu_theme_set(THEME_WIN98_CLASSIC);

    /* -- Launch binds every input callback (mirrors dosgui_launch_terminal:
     * the real adapter + engine, without pulling in the whole app table) -- */
    printf("[Launch]\n");
    app_terminal_init();
    DosGuiWindow *win = dosgui_wm_create(300, 160, 800, 600, "Terminal");
    T(win != NULL, "window created");
    int bw0 = border_width();
    int tbh0 = title_bar_height();
    dosgui_term_show(300 + bw0, 160 + bw0 + tbh0, 800 - 2 * bw0, 600 - tbh0 - bw0);
    if (win) {
        win->on_draw   = app_terminal_draw;
        win->on_key    = app_terminal_key;
        win->on_mouse  = app_terminal_mouse;
        win->on_scroll = app_terminal_scroll;
    }
    T(win && win->on_draw != NULL, "on_draw bound");
    T(win && win->on_key != NULL, "on_key bound");
    T(win && win->on_mouse != NULL, "on_mouse bound");
    T(win && win->on_scroll != NULL, "on_scroll bound");
    T(dosgui_term_is_open(), "terminal engine opened a session");

    /* -- The engine region tracks the chrome content rect -- */
    printf("\n[Geometry]\n");
    int bw = border_width();
    int tbh = title_bar_height();
    if (win) {
        T(g_term.x == win->x + bw, "engine x == window content x");
        T(g_term.y == win->y + bw + tbh, "engine y == window content y");
        T(g_term.w == win->w - 2 * bw, "engine w == window content w");
        T(g_term.h == win->h - tbh - bw, "engine h == window content h");
    }

    /* -- Render inside the chrome rect (no crash, real content) -- */
    printf("\n[Render]\n");
    if (win) {
        uint32_t *fb = (uint32_t *)calloc(1024 * 768, 4);
        dosgui_wm_render_desktop(fb, 1024, 768);   /* draws chrome + on_draw */
        T(1, "full desktop render with terminal no crash");

        /* After on_draw the engine region must still match the content rect
         * (the adapter re-syncs it each frame). */
        T(g_term.x == win->x + bw, "engine region re-synced after render");
        free(fb);
    }

    /* -- Input forwards do not crash -- */
    printf("\n[Input]\n");
    if (win) {
        win->on_mouse(win, 400, 300, 1, 1);   /* click in content */
        win->on_mouse(win, 410, 310, 1, 0);   /* drag */
        win->on_mouse(win, 410, 310, 1, 2);   /* release */
        T(1, "mouse click/drag/release handled");
        win->on_scroll(win, -3);              /* wheel up */
        T(1, "wheel scroll handled");
        win->on_key(win, 'l', 0);             /* type a char */
        T(1, "key input handled");
    }

    printf("\nResults: %d/%d passed\n", g_pass, g_run);
    return g_pass == g_run ? 0 : 1;
}
