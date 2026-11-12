/*
 * wm_internal.h  --  Internal shared state for wm module
 *
 * Exposes g_dwm and internal functions to sub-modules (HolyC terminal,
 * context menus, etc.) while keeping them private from the public API.
 *
 * NOTE: DosGuiIcon, DosGuiContextMenu, and all public types/constants
 * are already defined in wm.h -- do NOT redefine them here.
 */
#ifndef WUBU_DOSGUI_WM_INTERNAL_H
#define WUBU_DOSGUI_WM_INTERNAL_H

#include "wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "shell/wubu_wallpaper.h"
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

/* -- Global WM State (matches the struct in wm.c) ----------- */
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
    bool            overview_open;   /* Win+Tab: desktop overview grid */

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

    /* Auto-hide (modern theme): 0 = fully revealed, task_h = fully hidden. */
    int             taskbar_offset;

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
char *taskbar_get_clock_str(void);
/* Clock popup menu (Win98 parity): click the clock well to open. */
void clock_menu_toggle(void);
void clock_menu_close(void);
int  clock_menu_is_open(void);
void clock_menu_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h);
void clock_menu_render(uint32_t *fb, int fb_w, int fb_h);
/* Last-rendered clock well (clickable button) rect; zeros until first render. */
void clock_well(int *x, int *y, int *w, int *h);
int  title_bar_height(void);
int  taskbar_height_dynamic(void);
/* The y of the taskbar's TOP edge, including the auto-hide offset.  Every
 * render and hit-test must use this, or the bar and its clicks drift apart. */
int  taskbar_top(void);
/* -- Modern (Win11-class) taskbar, shared by render + hit-test ----------
 * THEME_DARK uses a floating rounded bar instead of the full-width classic
 * one.  Both the renderer and the pointer path call taskbar_modern_layout(),
 * so the bar is drawn and clicked in the same place by construction. */
typedef struct {
    int x, y, w, h;      /* button rect */
    int is_pin;          /* 0 = window task, 1 = pinned task */
    int index;           /* window index, or taskbar pin index */
} WmTaskSlot;

typedef struct {
    int bar_x, bar_y, bar_w, bar_h;
    int start_x, start_y, start_w, start_h;
    /* Win11 shell buttons between Start and the window list:
     * 0 = Search, 1 = Task View, 2 = Widgets. */
    int shell_x[3], shell_y, shell_w, shell_h;
    int clock_x, clock_y, clock_w, clock_h;
    int tray_x, tray_y, tray_w;   /* systray strip, left of the clock */
    int n_slots;
    WmTaskSlot slot[48];
} WmTaskLayout;

void taskbar_modern_layout(int fb_w, int fb_h, WmTaskLayout *L);
bool taskbar_modern_handle(int x, int y, int btn, int kind);
int  border_width(void);
int  theme_radius(void);
void load_default_wallpaper(void);
void draw_wallpaper(int fb_w, int fb_h);
void draw_desktop_bg(int fb_w, int fb_h);
void snap_icon_to_grid(DosGuiIcon *icon);
int  icon_grid_x(int x);
int  icon_grid_y(int y);

/* Icon glyph rendering (Chicago -> XP): recognizable 32x32 pixel-art per
 * icon type, themed via the 4 supplied colors (face/light/dark/accent). */
void wm_draw_icon_glyph(DeskIconType type, int ox, int oy,
                               uint32_t c0, uint32_t c1,
                               uint32_t c2, uint32_t c3);
void wm_draw_icon_selection(int ox, int oy);
void snap_window_to_gaad(DosGuiWindow *w);
void snap_window_to_zone(DosGuiWindow *w, int zone);
int  icon_hit_test(int mx, int my);
int  icon_select_in_rect(int x0, int y0, int x1, int y1);
DosGuiWindow *wm_spawn_holyc_term(int x, int y, int w, int h);
int  spawn_window(int x, int y, int w, int h, const char *title);

/* -- Desktop view options (Stream 3) -- */
void wm_set_auto_arrange(bool on);
bool wm_get_auto_arrange(void);
/* Re-flow all live icons into the top-left auto-arrange column. */
void reflow_all_icons_column(void);
/* Re-flow all live icons alphabetically into a top-left column grid. */
void wm_sort_icons_by_name(void);
/* Desktop icon sort modes (ReactOS "Arrange Icons By"): Name/Size/Type/Date. */
typedef enum {
    DOSGUI_SORT_NAME = 0,
    DOSGUI_SORT_SIZE,
    DOSGUI_SORT_TYPE,
    DOSGUI_SORT_DATE
} DosGuiSortMode;
void wm_sort_icons(DosGuiSortMode mode);
/* Create a real folder / text document in ~/Desktop, re-enumerate + re-flow. */
int wm_new_folder(void);
int wm_new_text_doc(void);
/* Write a real .desktop shortcut into ~/Desktop; returns 0 on success. */
int  wm_write_desktop_shortcut(const char *name, const char *exec);
/* Re-enumerate desktop icons from ~/Desktop (real filesystem refresh). */
void wm_refresh_desktop(void);

/* Access the hosted process state (for session split: GAME mode launch). */
hosted_state_t *wm_get_hosted_state(void);

#endif /* WUBU_DOSGUI_WM_INTERNAL_H */