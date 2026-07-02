/*
 * dosgui_wm_internal.h  --  Internal shared state for dosgui_wm module
 *
 * Exposes g_dwm and internal functions to sub-modules (HolyC terminal,
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
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

/* -- HolyC Terminal Instance (private to sub-module) --------------- */
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

    /* Desktop icons */
    DosGuiIcon       icons[DOSGUI_MAX_ICONS];
    int             icon_count;

    /* Icon drag state */
    int             drag_icon_id;
    int             drag_icon_ox, drag_icon_oy;

    /* Wallpaper */
    uint32_t       *wallpaper;
    int             wallpaper_w, wallpaper_h;
    int             wallpaper_mode; /* 0=center, 1=tile, 2=stretch */

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

    /* Mouse state */
    int             mouse_x, mouse_y;
    int             ticks;
} DosGuiWM;

extern DosGuiWM g_dwm;

/* -- Internal WM Functions (shared between sub-modules) ------------ */
void raise_win(int i);
void close_win(int i);
int  hit_test(int x, int y);
void draw_window(int idx);
void draw_desktop_bg(int fb_w, int fb_h);
const WubuThemeColors *tc(void);
const WubuTheme *theme(void);
int  title_bar_height(void);
int  taskbar_height_dynamic(void);
int  border_width(void);
int  theme_radius(void);
void load_default_wallpaper(void);
void draw_wallpaper(int fb_w, int fb_h);
void snap_icon_to_grid(DosGuiIcon *icon);
int  icon_grid_x(int x);
int  icon_grid_y(int y);
void snap_window_to_gaad(DosGuiWindow *w);
int  spawn_window(int x, int y, int w, int h, const char *title);

#endif /* WUBU_DOSGUI_WM_INTERNAL_H */