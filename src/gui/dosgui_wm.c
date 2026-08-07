/*
 * dosgui_wm.c  --  WuBuOS DosGui Window Manager facade
 *
 * Cell 400: Fable Windowing Agent — THEMED EDITION.
 * Ports ZealOS/WuBuDos bare-metal window management into WuBuOS.
 * Based on Mythos Fable's wm.c (filipvabrousek/osdev).
 *
 * This file is the thin orchestration facade. The heavy concerns are split
 * into self-contained modules (C11 opaque-safe, no god headers):
 *   dosgui_wm_window.c  -- window lifecycle, z-order, focus, virtual desktops,
 *                           desktop-icon registry + persistence
 *   dosgui_wm_input.c   -- key + mouse dispatch / hit-testing
 *   dosgui_wm_layout.c  -- themed window chrome + wallpaper
 *   dosgui_wm_render.c  -- full-frame composition (desktop + windows + taskbar)
 *   dosgui_wm_icons.c / _systray.c / _ctxmenu.c / _holyc_term.c / _desktop.c /
 *     _taskbar.c -- further sub-systems
 *
 * Features summary: draggable themed windows, z-order + focus, taskbar with
 * Start orb + window buttons + clock + systray, virtual desktops, desktop
 * icons, wallpaper, maximize/minimize, full theme engine (Win98/XP/WuBu/Zune).
 */

/* -- Includes ------------------------------------------------------ */
#include "dosgui_wm_internal.h"
#include "wubu_wallpaper.h"
#include "wubu_bonzi.h"

#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

/* The one true WM state (referenced by every sub-module via the internal
 * header's `extern DosGuiWM g_dwm;`). */
DosGuiWM g_dwm = {0};

/* -- Theme Helpers (used by chrome + render sub-modules) ----------- */

const WubuThemeColors *tc(void) { return wubu_theme_colors(); }
const WubuTheme *theme(void) { return wubu_theme_get(); }

/* ================================================================
 * PUBLIC LIFECYCLE
 * ================================================================ */

int dosgui_wm_init(int screen_w, int screen_h) {
    memset(&g_dwm, 0, sizeof(g_dwm));
    g_dwm.screen_w = screen_w;
    g_dwm.screen_h = screen_h;
    g_dwm.focused_id = -1;
    g_dwm.drag_id = -1;
    g_dwm.resize_id = -1;
    g_dwm.drag_icon_id = -1;
    g_dwm.current_desktop = 0;
    g_dwm.desktop_count = 1;   /* Single desktop by default (like Win98); the
                                 pager only appears when >1 is configured. */
    g_dwm.systray_count = 0;
    g_dwm.notif_count = 0;
    g_dwm.next_notif_id = 1;
    g_dwm.notif_center_open = false;
    g_dwm.last_clock_update = 0;
    load_default_wallpaper();
    /* Desktop companion (Bonzi Buddy): the friendly AGI gateway.
     * Positioned in the lower-left corner, above windows. Click opens the
     * HolyC/AGI terminal. Idle animation (bob + blink) driven by the WM
     * frame tick in dosgui_wm_frame(). */
    wubu_bonzi_init(8, screen_h - 64);
    return 0;
}

/* -- Human typing engine ---------------------------------------------
 * The AGI / automation "types" into a window with HUMAN rhythm: keystrokes
 * land spaced 28-51ms apart with jitter, plus an occasional 240ms "thinking"
 * pause and a 120-220ms reaction delay before the first key.  Deterministic
 * (seeded LCG) so tests can assert the exact progress after N ticks. */

static struct {
    DosGuiWindow *win;
    char         text[256];
    int          pos;         /* next char to deliver */
    int          delay_ms;    /* time until the next keystroke */
    uint32_t     rng;
} g_typer;

static uint32_t typer_rand(uint32_t *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

static int typer_next_delay(uint32_t *s) {
    uint32_t r = typer_rand(s);
    int d = 28 + (int)(r % 24);          /* 28..51 ms per keystroke */
    if ((r >> 16) % 23 == 0) d += 240;   /* occasional thinking pause */
    return d;
}

void dosgui_wm_typer_start(DosGuiWindow *win, const char *text) {
    if (!win || !text) return;
    memset(&g_typer, 0, sizeof(g_typer));
    g_typer.win = win;
    strncpy(g_typer.text, text, sizeof(g_typer.text) - 1);
    g_typer.rng = 0x9E3779B9u ^ (uint32_t)(size_t)win;
    /* human reaction delay before the first key */
    g_typer.delay_ms = 120 + (int)(typer_rand(&g_typer.rng) % 100);
}

void dosgui_wm_typer_tick(int dt_ms) {
    if (!g_typer.win) return;
    if (!g_typer.win->alive) { g_typer.win = NULL; return; }
    g_typer.delay_ms -= dt_ms;
    while (g_typer.delay_ms <= 0 && g_typer.pos < (int)strlen(g_typer.text)) {
        char c = g_typer.text[g_typer.pos++];
        if (g_typer.win->on_key)
            g_typer.win->on_key(g_typer.win, c, 0);
        g_typer.delay_ms += typer_next_delay(&g_typer.rng);
    }
    if (g_typer.pos >= (int)strlen(g_typer.text))
        g_typer.win = NULL;   /* finished */
}

int dosgui_wm_typer_busy(void) { return g_typer.win != NULL; }
int dosgui_wm_typer_pos(void)  { return g_typer.pos; }

/* The eased (human-lag) cursor render position. */
void dosgui_wm_cursor_pos(int *x, int *y) {
    if (x) *x = g_dwm.cursor_x;
    if (y) *y = g_dwm.cursor_y;
}

void dosgui_wm_shutdown(void) {
    if (g_dwm.wallpaper) {
        free(g_dwm.wallpaper);
        g_dwm.wallpaper = NULL;
    }
    memset(&g_dwm, 0, sizeof(g_dwm));
}

/* ================================================================
 * PLATFORM LIFECYCLE HOOKS
 * The hosted binary (src/hosted/hosted.c) provides the real
 * implementation (tears down the Wayland surface). Standalone app
 * binaries link these weak no-op defaults so they build without
 * pulling in the full hosted stack.
 * ================================================================ */

__attribute__((weak))
void dosgui_platform_shutdown(void) {
    /* No-op for standalone app binaries. */
}

__attribute__((weak))
void dosgui_shutdown(void) {
    /* No-op for standalone app binaries. */
}

/* EOF */
