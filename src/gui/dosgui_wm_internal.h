/*
 * dosgui_wm_internal.h  --  Internal shared state for dosgui_wm module
 *
 * Exposes g_dwm and internal functions to sub-modules (HolyD terminal,
 * context menus, etc.) while keeping them private from the public API.
 *
 * NOTE: DosGuiIcon, DosGuiContextMenu, and all public types/constants
 * are already defined in dosgui_wm.h -- do NOT redefine them here.
 */
#ifndef WUBU_DOSGUI_WM_INTERNAL_H
#define WUBU_DOSGUI_WM_INTERNAL_H

#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../gui/wubu_wallpaper.h"
#include "../gui/wubu_settings.h"
#include "../runtime/wubu_session.h"   /* session split (GAME mode) */
#include "../runtime/wubu_compat_db.h" /* per-title ProtonDB profile */
#include "../hosted/hosted.h"          /* hosted_state_t (the Play action) */
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

/* -- HolyD Terminal Instance (private to sub-module) --------------- */
typedef struct {
    char  buffer[32][256];    /* Rollback buffer of output lines */
    char  input[256];         /* Current input buffer */
    char  history[16][256];   /* Command history ring */
    int   hist_pos;           /* Current position in history ring */
    int   hist_count;         /* Number of history entries */
    int   scroll_y;           /* Vertical scroll offset */
    int   cursor_pos;         /* Cursor position in input line */
    bool  initialized;        /* Whether term has been initialized */
} HolycTerm;

/* -- Global WM State (matches the struct in dosgui_wm.c) ----------- */
typedef struct {
    int             screen_w, screen_h;
    DosGuiWindow    windows[DOSGUI_MAX_WINDOWS];
    int             next_id;
    int             focused_id;

    /* Z-order: indices into windows[], bottom..top */
    int             zorder[DOSGUI_MAX_WINDOWS];
    int             nz;

    /* Drag state */
    int             drag_id;
    int             drag_ox, drag_oy;

    /* Resize state (Chicago/Mac edge + corner resize) */
    int             resize_id;
    int             resize_edge;   /* 0 none, bits: 1=left 2=right 4=top 8=bottom */
    int             resize_ox, resize_oy;  /* grab offset within window */
    int             resize_ow, resize_oh;  /* original w/h at grab */

    /* Desktop icons */
    DosGuiIcon       icons[DOSGUI_MAX_ICONS];
    int             icon_count;

    /* Icon drag state */
    int             drag_icon_id;
    int             drag_icon_ox, drag_icon_oy;

    /* Rubber-band drag-select lasso (Classic Mac OS / Win98 lesson):
     * press on empty desktop, drag a rect, release -> select icons inside. */
    bool            lasso_active;
    int             lasso_x0, lasso_y0;   /* anchor (press point) */
    int             lasso_x1, lasso_y1;   /* current corner */

    /* Wallpaper */
    uint32_t       *wallpaper;
    int             wallpaper_w, wallpaper_h;
    int             wallpaper_mode; /* 0=center, 1=tile, 2=stretch, 3=fit, 4=fill */
    uint32_t       *logo;           /* bundled logo badge (overlay, not scaled) */
    int             logo_w, logo_h;

    /* Desktop view options (Stream 3) */
    bool            auto_arrange;   /* Snap icons into a top-left column/grid */

    /* Virtual desktops */
    int             current_desktop;
    int             desktop_count;

    /* System Tray */
    DosGuiSysTrayIcon systray_icons[DOSGUI_MAX_SYSTRAY_ICONS];
    int             systray_count;

    /* Notification Center */
    DosGuiNotification notifications[DOSGUI_NOTIF_CENTER_MAX];
    int             notif_count;
    int             next_notif_id;
    bool            notif_center_open;

    /* Last real time for clock */
    time_t          last_clock_update;

    /* Mouse state: mouse_x/y = INSTANT input target (logic uses these);
     * cursor_x/y = eased RENDER position (human lag, fast-but-readable). */
    int             mouse_x, mouse_y;
    int             cursor_x, cursor_y;
    int             ticks;

    /* Taskbar clock (Win98 parity): recessed well + its own popup menu. */
    bool            clock_menu_open;
    int             clock_well_x, clock_well_y, clock_well_w, clock_well_h;
} DosGuiWM;

extern DosGuiWM g_dwm;

/* -- Internal WM Functions (shared between sub-modules) ------------ */
void raise_win(int i);
void close_win(int i);
int  hit_test(int x, int y);
const WubuThemeColors *tc(void);
const WubuTheme *theme(void);
char *dosgui_taskbar_get_clock_str(void);
/* Clock popup menu (Win98 parity): click the clock well to open. */
void dosgui_clock_menu_toggle(void);
void dosgui_clock_menu_close(void);
int  dosgui_clock_menu_is_open(void);
void dosgui_clock_menu_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h);
void dosgui_clock_menu_render(uint32_t *fb, int fb_w, int fb_h);
/* Last-rendered clock well (clickable button) rect; zeros until first render. */
void dosgui_clock_well(int *x, int *y, int *w, int *h);
int  title_bar_height(void);
int  taskbar_height_dynamic(void);
int  border_width(void);
int  theme_radius(void);
void load_default_wallpaper(void);
void draw_wallpaper(int fb_w, int fb_h);
void snap_icon_to_grid(DosGuiIcon *icon);
int  icon_grid_x(int x);
int  icon_grid_y(int y);

/* Icon glyph rendering (Chicago -> XP): recognizable 32x32 pixel-art per
 * icon type, themed via the 4 supplied colors (face/light/dark/accent). */
void dosgui_wm_draw_icon_glyph(DeskIconType type, int ox, int oy,
                               uint32_t c0, uint32_t c1,
                               uint32_t c2, uint32_t c3);
void dosgui_wm_draw_icon_selection(int ox, int oy);
void snap_window_to_gaad(DosGuiWindow *w);
int  dosgui_icon_hit_test(int mx, int my);
int  dosgui_icon_select_in_rect(int x0, int y0, int x1, int y1);
DosGuiWindow *dosgui_wm_spawn_holyd_term(int x, int y, int w, int h);
int  spawn_window(int x, int y, int w, int h, const char *title);

/* -- Desktop view options (Stream 3) -- */
void dosgui_wm_set_auto_arrange(bool on);
bool dosgui_wm_get_auto_arrange(void);
/* Re-flow all live icons into the top-left auto-arrange column. */
void reflow_all_icons_column(void);
/* Re-flow all live icons alphabetically into a top-left column grid. */
void dosgui_wm_sort_icons_by_name(void);
/* Desktop icon sort modes (ReactOS "Arrange Icons By"): Name/Size/Type/Date. */
typedef enum {
    DOSGUI_SORT_NAME = 0,
    DOSGUI_SORT_SIZE,
    DOSGUI_SORT_TYPE,
    DOSGUI_SORT_DATE
} DosGuiSortMode;
void dosgui_wm_sort_icons(DosGuiSortMode mode);
/* Create a real folder / text document in ~/Desktop, re-enumerate + re-flow. */
int dosgui_wm_new_folder(void);
int dosgui_wm_new_text_doc(void);
/* Write a real .desktop shortcut into ~/Desktop; returns 0 on success. */
int  dosgui_wm_write_desktop_shortcut(const char *name, const char *exec);
/* Re-enumerate desktop icons from ~/Desktop (real filesystem refresh). */
void dosgui_wm_refresh_desktop(void);

/* Access the hosted process state (for session split: GAME mode launch). */
hosted_state_t *dosgui_wm_get_hosted_state(void);

#endif /* WUBU_DOSGUI_WM_INTERNAL_H */