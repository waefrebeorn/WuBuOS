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
    return 0;
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
