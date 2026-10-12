/*
 * app_terminal.c  --  WuBuOS Terminal: in-shell binding for the REAL
 * pty-backed terminal engine (dosgui_term.c + _ansi/_pty/_tabs/_render).
 *
 * The engine is a complete VT/ANSI terminal: real forkpty shell sessions,
 * tab bar, scrollback ring buffer, text selection, copy/paste and
 * wheel-scroll. It draws in absolute screen space using its own
 * g_term.x/y/w/h region. This file is the thin WM adapter:
 *
 *   - on_draw:  draw the window chrome (ADR-001), then sync g_term's
 *               region to the chrome CONTENT rect and render the engine
 *               inside it.
 *   - on_key:   forward to dosgui_term_handle_key.
 *   - on_mouse: translate window-relative -> absolute (the engine
 *               compares against g_term.y / g_term.y+h) and forward.
 *   - on_scroll: forward the wheel to dosgui_term_scroll.
 *
 * C11, minimal includes, self-contained.
 */

#include "dosgui_apps.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/dosgui_window_chrome.h"
#include "../gui/dosgui_term.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdint.h>

/* -- Draw (on_draw callback) -------------------------------------- */

void app_terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    if (!win) return;

    /* ADR-001: chrome first, then content inside the chrome rect. */
    ChromeContentRect content = dosgui_chrome_draw_window(win, fb, fb_w, fb_h);

    /* Sync the engine's region to the chrome content rect so all of its
     * internal g_term.x/y/w/h math lands inside the window. */
    g_term.x = content.x;
    g_term.y = content.y;
    g_term.w = content.w;
    g_term.h = content.h;

    if (!dosgui_term_is_open()) return;

    /* Drain PTY output so the frame reflects new data, then render. */
    dosgui_term_pty_read();
    dosgui_term_render(fb, fb_w, fb_h);
}

/* -- Input forwards ------------------------------------------------ */

void app_terminal_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win;
    if (!dosgui_term_is_open()) return;
    dosgui_term_handle_key(key, mods);
}

void app_terminal_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    if (!win || !dosgui_term_is_open()) return;
    /* The WM delivers window-relative coords; the engine's hit-tests
     * compare against absolute g_term.y / g_term.y + h. Translate. */
    dosgui_term_handle_mouse(x + win->x, y + win->y, btn, kind);
}

void app_terminal_scroll(DosGuiWindow *win, int delta) {
    (void)win;
    if (!dosgui_term_is_open()) return;
    dosgui_term_scroll(delta);
}

/* -- Init ---------------------------------------------------------- */

static int g_terminal_ready = 0;

void app_terminal_init(void) {
    if (g_terminal_ready) return;
    dosgui_term_init();
    g_terminal_ready = 1;
}
