/*
 * wm.h  --  COMPATIBILITY SHIM
 *
 * The original "My Seed" window manager (NanoShell-style) was replaced by
 * the DosGui window manager (dosgui_wm.c / dosgui_wm.h).  Several apps still
 * carry the old `WmWindow` / `wm_*` API.  Rather than leave those apps
 * referencing a deleted implementation (form without function), this shim
 * routes the old identifiers to the live, fully-implemented DosGui WM.
 *
 * NOTE: this is NOT a blanket `#define wm_ dosgui_wm_` — that would corrupt
 * unrelated identifiers such as the Wayland `xdg_wm_base_*` symbols used by
 * src/hosted/hosted.c.  Only the genuine old-WM entry points are aliased.
 */

#ifndef MYSEED_WM_H
#define MYSEED_WM_H

#include "dosgui_wm.h"
#include "wubu_theme.h"

/* -- Type aliases (identical field layout) ----------------------- */
typedef DosGuiWindow  WmWindow;
typedef DosGuiWinFlags WinFlags;

/* -- Constant aliases -------------------------------------------- */
#define WM_MAX_WINDOWS     DOSGUI_MAX_WINDOWS
#define WM_TITLE_HEIGHT    20
#define WM_BORDER_WIDTH    3

#define WIN_UNUSED    DOSGUI_WIN_UNUSED
#define WIN_VISIBLE   DOSGUI_WIN_NORMAL
#define WIN_MINIMIZED DOSGUI_WIN_MINIMIZED
#define WIN_MAXIMIZED DOSGUI_WIN_MAXIMIZED
#define WIN_FOCUSED   DOSGUI_WIN_FOCUSED
#define WIN_NOCLOSE   DOSGUI_WIN_UNUSED   /* no direct equiv; map to unused */

/* -- Function aliases (old WM API -> live DosGui WM) ------------ */
#define wm_init                dosgui_wm_init
#define wm_shutdown            dosgui_wm_shutdown
#define wm_create_window       dosgui_wm_create
#define wm_destroy_window      dosgui_wm_destroy
#define wm_set_focus           dosgui_wm_set_focus
#define wm_get_focused         dosgui_wm_get_focused
#define wm_find_by_id          dosgui_wm_find_by_id
#define wm_window_count        dosgui_wm_window_count
#define wm_render              dosgui_wm_render
#define wm_invalidate          dosgui_wm_invalidate
#define wm_invalidate_all      dosgui_wm_invalidate_all
#define wm_handle_key          dosgui_wm_handle_key
#define wm_handle_mouse        dosgui_wm_handle_mouse
#define wm_resize              dosgui_wm_resize
#define wm_move                dosgui_wm_move
#define wm_maximize            dosgui_wm_maximize
#define wm_minimize            dosgui_wm_minimize

#endif /* MYSEED_WM_H */
