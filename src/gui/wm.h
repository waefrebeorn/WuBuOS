/*
 * wm.h  --  WuBuOS DosGui Window Manager
 *
 * Cell 400: Fable Windowing Agent — ports ZealOS/WuBuDos bare-metal
 * window management into the hosted Arch desktop.
 *
 * This IS the "TempleOS on a RamDisk" windowing system, running as
 * a subsystem inside WuBuOS on Wayland. Fable's kernel (Mythos Fable)
 * provided the sauce: window chrome, drag/z-order/focus, taskbar,
 * cursor, close boxes. This module adapts that sauce to use WuBuOS's
 * VBE renderer, theme engine, GAAD snap, and virtual desktops.
 *
 * Architecture:
 *   WuBuOS hosted (Wayland) → VBE backbuffer → wm render
 *   Desktop icons → launch apps → windows managed by this WM
 *   Fable apps (Notepad, Palette, Bounce, Welcome) run as window content
 */

#ifndef WUBU_WM_H
#define WUBU_WM_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "shell/wm_rim.h"   /* islands on the focused window's rim */
#include "shell/wm_slime.h"  /* the screen-edge slime seam */

#define DOSGUI_MAX_WINDOWS    32
#define DOSGUI_MAX_DESKTOPS    9
#define DOSGUI_TITLE_H        20
#define DOSGUI_TASK_H         28
#define DOSGUI_BORDER          2
#define DOSGUI_ICON_LABEL_H   16  /* Height reserved for icon label text */

typedef enum {
    DOSGUI_WIN_UNUSED   = 0,
    DOSGUI_WIN_NORMAL   = 1,
    DOSGUI_WIN_FOCUSED  = 2,
    DOSGUI_WIN_MINIMIZED = 4,
    DOSGUI_WIN_MAXIMIZED = 8,
} DosGuiWinFlags;

typedef struct DosGuiWindow DosGuiWindow;
typedef struct DosGuiWindow {
    int            id;
    DosGuiWinFlags flags;
    int            x, y, w, h;
    int            min_x, min_y, min_w, min_h; /* Saved before maximize */
    int            desktop;                    /* Virtual desktop index (0..8) */
    char           title[64];
    bool           alive;
    bool           is_modal;                   /* Modal dialog flag */
    DosGuiWindow  *parent;                     /* Parent window for modal dialogs */
    /* Content render callback */
    void         (*on_draw)(DosGuiWindow *win, uint32_t *fb,
                            int fb_w, int fb_h);
    /* Content input callbacks (optional) */
    void         (*on_key)(DosGuiWindow *win, uint32_t key, uint32_t mods);
    void         (*on_mouse)(DosGuiWindow *win, int x, int y, int btn, int kind);
    /* Resize callback */
    void         (*on_resize)(DosGuiWindow *win, int w, int h);
    void          *user_data;
    uint32_t      icon_color;   /* Fallback colored box for title bar icon */
} DosGuiWindow;

/* -- Lifecycle ---------------------------------------------------- */

int  wm_init(int screen_w, int screen_h);
void wm_shutdown(void);

/* Human typing (human-lag input): "type" text into a window with human
 * rhythm — 28-51ms per key with jitter, an occasional ~240ms thinking pause,
 * and a 120-220ms reaction delay before the first key.  Driven by the WM
 * frame tick (wm_typer_tick is called every render). */
void wm_typer_start(DosGuiWindow *win, const char *text);
void wm_typer_tick(int dt_ms);
int  wm_typer_busy(void);   /* 1 while a typing session is active */
int  wm_typer_pos(void);    /* chars delivered so far */

/* The eased (human-lag) cursor render position, for verification. */
void wm_cursor_pos(int *x, int *y);

/* -- Window Management ------------------------------------------- */

DosGuiWindow *wm_create(int x, int y, int w, int h,
                                const char *title);
void           wm_destroy(DosGuiWindow *win);
void           wm_set_focus(DosGuiWindow *win);
DosGuiWindow  *wm_get_focused(void);
DosGuiWindow  *wm_find_by_id(int id);
int            wm_window_count(void);

/* Window resize and state management */
void           wm_resize(DosGuiWindow *win, int w, int h);
void           wm_move(DosGuiWindow *win, int x, int y);
void           wm_maximize(DosGuiWindow *win);
void           wm_minimize(DosGuiWindow *win);
void           wm_show_desktop(void);   /* Win+D: minimize all / restore */
void           wm_restore(DosGuiWindow *win);
bool           wm_is_maximized(DosGuiWindow *win);
bool           wm_is_minimized(DosGuiWindow *win);

/* Modal dialog support */
DosGuiWindow *wm_create_modal(int x, int y, int w, int h,
                                      const char *title,
                                      DosGuiWindow *parent);
bool           wm_is_modal(DosGuiWindow *win);

/* -- Virtual Desktop Migration ------------------------------------- */

void wm_move_window_to_desktop(DosGuiWindow *win, int desktop);
int  wm_get_current_desktop(void);
void wm_set_current_desktop(int desktop);
void wm_add_desktop(void);   /* Win+Ctrl+D: create a desktop */
/* Win+Tab desktop overview grid. */
void wm_overview_toggle(void);
bool wm_overview_is_open(void);
void wm_overview_render(uint32_t *fb, int fb_w, int fb_h);
bool wm_overview_handle_click(int mx, int my, int fb_w, int fb_h);

/* Touchpad gesture dispatch (called from the render loop). */
void wm_gesture_poll(void);
void wm_gesture_dispatch(void);

/* Win+A quick settings panel. */
void quick_settings_toggle(void);
bool quick_settings_is_open(void);
void quick_settings_render(uint32_t *fb, int fb_w, int fb_h);
bool quick_settings_handle_click(int mx, int my, int fb_w, int fb_h);
void quick_settings_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h);
/* Tile geometry for the overview grid (shared by the renderer and hit-test). */
void wm_overview_tile_rect(int fb_w, int fb_h, int desktop,
                                  int *x, int *y, int *w, int *h);
void wm_move_focused_window(int delta);

/* -- Input ------------------------------------------------------- */

void wm_handle_key(uint32_t key, uint32_t mods);
void wm_handle_mouse(int x, int y, int btn, int kind);
/* Current rendered cursor position (set by the input path; used by the AGI
 * automation layer to press/release buttons at the live cursor). */
void wm_get_mouse(int *x, int *y);
/* kind: 0=move, 1=down, 2=up */

/* -- Rendering --------------------------------------------------- */

void wm_render(uint32_t *fb, int fb_w, int fb_h);
void wm_render_desktop(uint32_t *fb, int fb_w, int fb_h);

/* -- Taskbar / Desktop ------------------------------------------- */

#define DOSGUI_TASKBAR_H        28
#define DOSGUI_SYSTRAY_SIZE     24
#define DOSGUI_CLOCK_W          80

int  taskbar_height(void);
void taskbar_render(uint32_t *fb, int fb_w, int fb_h);

/* -- System Tray / Notification Area ------------------------------ */

#define DOSGUI_MAX_SYSTRAY_ICONS 16

typedef struct {
    char name[32];
    uint32_t icon_color;    /* Simple colored box for now */
    bool visible;
    void (*on_click)(void);
    void (*on_right_click)(void);
    int notification_count;  /* Badge count */
} DosGuiSysTrayIcon;

int  systray_add(const char *name, uint32_t color,
                        void (*on_click)(void),
                        void (*on_right_click)(void));
void systray_remove(const char *name);
int systray_update_color(const char *name, uint32_t color);
uint32_t systray_color(const char *name);
void systray_set_notification_count(const char *name, int count);

/* -- Notification Center ------------------------------------------ */

#define DOSGUI_NOTIF_CENTER_MAX 32

typedef struct {
    uint32_t id;
    char app_name[64];
    char summary[128];
    char body[256];
    uint32_t timestamp;     /* Unix time */
    int urgency;            /* 0=low, 1=normal, 2=critical */
    bool read;
    bool expanded;
} DosGuiNotification;

int  notif_center_add(const char *app_name, const char *summary,
                               const char *body, int urgency);
void notif_center_mark_read(uint32_t id);
void notif_center_clear(void);
void notif_center_render(uint32_t *fb, int fb_w, int fb_h);
bool notif_center_is_open(void);
void notif_center_toggle(void);

/* -- Clock -------------------------------------------------------- */

void taskbar_update_clock(time_t now);

/* -- Desktop Icons ----------------------------------------------- */

#define DOSGUI_MAX_ICONS 64
#define DOSGUI_ICON_SIZE 32
#define DOSGUI_ICON_GAP  8

typedef enum {
    DESK_ICON_APP       = 0,  /* Launches an app */
    DESK_ICON_SHORTCUT  = 1,  /* Points to executable/file */
    DESK_ICON_FOLDER    = 2,  /* Opens folder in file manager */
    DESK_ICON_DRIVE     = 3,  /* Mounted drive/volume */
    DESK_ICON_FILE      = 4,  /* Opens with default app */
    DESK_ICON_URL       = 5,  /* Opens URL in browser */
} DeskIconType;

typedef struct {
    char name[32];
    int  x, y;          /* Grid position in pixels */
    int  grid_x, grid_y;
    bool alive;
    DeskIconType type;      /* Type of icon */
    char target[256];      /* Executable path, file path, URL, etc. */
    char icon_path[256];   /* Optional custom icon image */
    uint32_t icon_color;   /* Fallback colored box */
    bool selected;          /* For multi-select/drag */
    void (*on_click)(void); /* Legacy click handler */
    void (*on_execute)(void); /* Execute action */
} DosGuiIcon;

#define DOSGUI_MAX_ICONS 64
#define DOSGUI_ICON_SIZE 32
#define DOSGUI_ICON_GAP  8

/* -- Context Menu ------------------------------------------------- */

#define DOSGUI_MAX_CTX_ITEMS 16

typedef enum {
    CTX_ITEM_ACTION = 0,    /* Regular action */
    CTX_ITEM_SEPARATOR = 1,
    CTX_ITEM_SUBMENU = 2,
} DosGuiCtxItemType;

typedef struct {
    DosGuiCtxItemType type;
    char label[32];
    void (*action)(void);
    struct DosGuiContextMenu *submenu;  /* If type == SUBMENU */
    bool disabled;
    bool checked;
} DosGuiCtxItem;

typedef struct DosGuiContextMenu {
    DosGuiCtxItem items[DOSGUI_MAX_CTX_ITEMS];
    int item_count;
    int x, y;                /* Position */
    bool visible;
    int selected_item;       /* Highlighted item */
    struct DosGuiContextMenu *parent;
} DosGuiContextMenu;

/* Global context menu stack */
extern DosGuiContextMenu *g_ctx_stack;

int icon_add(const char *name, int gx, int gy,
                     void (*on_click)(void));

/* New enhanced API */
int icon_add_ex(const char *name, DeskIconType type,
                        const char *target, int gx, int gy,
                        uint32_t icon_color, void (*on_execute)(void));

void icon_remove(int grid_x, int grid_y);
int icon_find_at(int grid_x, int grid_y);
void icon_set_position(int grid_x, int grid_y, int new_gx, int new_gy);

/* Context Menu API */
DosGuiContextMenu *ctx_menu_create(int x, int y);
void ctx_menu_add_item(DosGuiContextMenu *menu, const char *label,
                               void (*action)(void));
void ctx_menu_add_separator(DosGuiContextMenu *menu);
DosGuiContextMenu *ctx_menu_add_submenu(DosGuiContextMenu *menu, const char *label);
void ctx_menu_show(DosGuiContextMenu *menu, int x, int y);
void ctx_menu_hide(DosGuiContextMenu *menu);
void ctx_menu_handle_mouse(int x, int y, int btn, int kind);
void ctx_menu_render(uint32_t *fb, int fb_w, int fb_h);

/* Shortcut Creation */
int shortcut_create(const char *name, const char *target,
                            const char *description, int grid_x, int grid_y);
int shortcut_create_url(const char *name, const char *url, int grid_x, int grid_y);

/* Default context menus */
void icon_show_context_menu(int icon_idx, int mx, int my);
void desktop_show_context_menu(int mx, int my);

/* -- Desktop Icons ----------------------------------------------- */

void icon_render(uint32_t *fb, int fb_w, int fb_h);
int  icon_hit_test(int mx, int my);

/* Tick */
void tick(void);

/* -- Query ------------------------------------------------------- */

int wm_screen_w(void);
int wm_screen_h(void);

/* Accessor for WM state fields */
int wm_get_icon_count(void);

/* -- Icon persistence & wallpaper reload (Desktop fixup Stream 2/4) - */
DosGuiIcon *icon_get(int idx);
/* Compact dead (alive==false) icons out of the array, keeping icon_count dense. */
void wm_compact_icons(void);
void wm_save_icon_layout(void);      /* Persist live icon grid into settings */
void wm_restore_icon_layout(void);   /* Restore live icon grid from settings */
void wm_reload_wallpaper(void);      /* Re-decode wallpaper from settings path */
bool wm_is_initialized(void);
int  wm_wallpaper_mode(void);
int  wm_wallpaper_w(void);
int  wm_wallpaper_h(void);
/* Desktop view options (ReactOS explorer/desktop.cpp lesson). */
void wm_refresh_desktop(void);       /* Re-scan ~/Desktop for live icons */
void wm_set_auto_arrange(bool on);   /* Toggle + persist auto-arrange */
bool wm_get_auto_arrange(void);
void wm_set_icons_visible(bool show);/* Live show/hide all desktop icons */

/* -- Invalidation tracking (legacy wm_invalidate compat) --------- */
void wm_invalidate(DosGuiWindow *win);
void wm_invalidate_all(void);
bool wm_poll_dirty(int *out_id);   /* drain; -1 = redraw all */
int  wm_dirty_count(void);         /* -1 if full-redraw pending */

/* -- Window Chrome (standardized window decorations) ------------- */

/* Content rect returned for app to draw within */
typedef struct {
    int x, y, w, h;
} ChromeContentRect;

/* Button hit-test result */
typedef enum {
    CHROME_BTN_NONE = 0,
    CHROME_BTN_CLOSE,
    CHROME_BTN_MAXIMIZE,
    CHROME_BTN_MINIMIZE,
} ChromeButton;

/* Draw complete window chrome (frame, title bar, buttons).
 * Returns content rect for app to draw within. */
ChromeContentRect chrome_draw_window(DosGuiWindow *win,
                                            uint32_t *fb, int fb_w, int fb_h);

/* Hit-test title bar buttons: returns which button (if any) contains point.
 * Coordinates are window-relative (0,0 = window top-left). */
int chrome_hit_test_button(DosGuiWindow *win, int mx, int my);

/* Handle title bar button click (close/maximize/minimize) */
void chrome_handle_button_click(DosGuiWindow *win, int button);

/* -- HolyC Terminal ---------------------------------------------- */

DosGuiWindow *wm_spawn_holyc_term(int x, int y, int w, int h);

/* -- Legacy "My Seed" WM aliases ---------------------------------
 *
 * Several apps still carry the old NanoShell `WmWindow` / `wm_*` API.  The
 * live window manager is this header; these aliases keep that code building
 * against the real implementation rather than a deleted one.
 */
typedef DosGuiWindow   WmWindow;
typedef DosGuiWinFlags WinFlags;

#define WM_MAX_WINDOWS     DOSGUI_MAX_WINDOWS
#define WM_TITLE_HEIGHT    20
#define WM_BORDER_WIDTH    3

#define WIN_UNUSED    DOSGUI_WIN_UNUSED
#define WIN_VISIBLE   DOSGUI_WIN_NORMAL
#define WIN_MINIMIZED DOSGUI_WIN_MINIMIZED
#define WIN_MAXIMIZED DOSGUI_WIN_MAXIMIZED
#define WIN_FOCUSED   DOSGUI_WIN_FOCUSED
#define WIN_NOCLOSE   DOSGUI_WIN_UNUSED

/* The old names for create/destroy differ from the live ones. */
#define wm_create_window   wm_create
#define wm_destroy_window  wm_destroy

#endif /* WUBU_WM_H */
