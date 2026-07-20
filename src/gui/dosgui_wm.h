/*
 * dosgui_wm.h  --  WuBuOS DosGui Window Manager
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
 *   WuBuOS hosted (Wayland) → VBE backbuffer → dosgui_wm render
 *   Desktop icons → launch apps → windows managed by this WM
 *   Fable apps (Notepad, Palette, Bounce, Welcome) run as window content
 */

#ifndef WUBU_DOSGUI_WM_H
#define WUBU_DOSGUI_WM_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define DOSGUI_MAX_WINDOWS    32
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
struct DosGuiWindow {
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
};

/* -- Lifecycle ---------------------------------------------------- */

int  dosgui_wm_init(int screen_w, int screen_h);
void dosgui_wm_shutdown(void);

/* -- Window Management ------------------------------------------- */

DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h,
                                const char *title);
void           dosgui_wm_destroy(DosGuiWindow *win);
void           dosgui_wm_set_focus(DosGuiWindow *win);
DosGuiWindow  *dosgui_wm_get_focused(void);
DosGuiWindow  *dosgui_wm_find_by_id(int id);
int            dosgui_wm_window_count(void);

/* Window resize and state management */
void           dosgui_wm_resize(DosGuiWindow *win, int w, int h);
void           dosgui_wm_move(DosGuiWindow *win, int x, int y);
void           dosgui_wm_maximize(DosGuiWindow *win);
void           dosgui_wm_minimize(DosGuiWindow *win);
void           dosgui_wm_restore(DosGuiWindow *win);
bool           dosgui_wm_is_maximized(DosGuiWindow *win);
bool           dosgui_wm_is_minimized(DosGuiWindow *win);

/* Modal dialog support */
DosGuiWindow *dosgui_wm_create_modal(int x, int y, int w, int h,
                                      const char *title,
                                      DosGuiWindow *parent);
bool           dosgui_wm_is_modal(DosGuiWindow *win);

/* -- Virtual Desktop Migration ------------------------------------- */

void dosgui_wm_move_window_to_desktop(DosGuiWindow *win, int desktop);
int  dosgui_wm_get_current_desktop(void);
void dosgui_wm_set_current_desktop(int desktop);
void dosgui_wm_move_focused_window(int delta);

/* -- Input ------------------------------------------------------- */

void dosgui_wm_handle_key(uint32_t key, uint32_t mods);
void dosgui_wm_handle_mouse(int x, int y, int btn, int kind);
/* Current rendered cursor position (set by the input path; used by the AGI
 * automation layer to press/release buttons at the live cursor). */
void dosgui_wm_get_mouse(int *x, int *y);
/* kind: 0=move, 1=down, 2=up */

/* -- Rendering --------------------------------------------------- */

void dosgui_wm_render(uint32_t *fb, int fb_w, int fb_h);
void dosgui_wm_render_desktop(uint32_t *fb, int fb_w, int fb_h);

/* -- Taskbar / Desktop ------------------------------------------- */

#define DOSGUI_TASKBAR_H        28
#define DOSGUI_SYSTRAY_SIZE     24
#define DOSGUI_CLOCK_W          80

int  dosgui_taskbar_height(void);
void dosgui_taskbar_render(uint32_t *fb, int fb_w, int fb_h);

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

int  dosgui_systray_add(const char *name, uint32_t color,
                         void (*on_click)(void),
                         void (*on_right_click)(void));
void dosgui_systray_remove(const char *name);
void dosgui_systray_set_notification_count(const char *name, int count);

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

int  dosgui_notif_center_add(const char *app_name, const char *summary,
                               const char *body, int urgency);
void dosgui_notif_center_mark_read(uint32_t id);
void dosgui_notif_center_clear(void);
void dosgui_notif_center_render(uint32_t *fb, int fb_w, int fb_h);
bool dosgui_notif_center_is_open(void);
void dosgui_notif_center_toggle(void);

/* -- Clock -------------------------------------------------------- */

void dosgui_taskbar_update_clock(time_t now);

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
extern DosGuiContextMenu *g_dosgui_ctx_stack;

int dosgui_icon_add(const char *name, int gx, int gy,
                     void (*on_click)(void));

/* New enhanced API */
int dosgui_icon_add_ex(const char *name, DeskIconType type,
                        const char *target, int gx, int gy,
                        uint32_t icon_color, void (*on_execute)(void));

void dosgui_icon_remove(int grid_x, int grid_y);
int dosgui_icon_find_at(int grid_x, int grid_y);
void dosgui_icon_set_position(int grid_x, int grid_y, int new_gx, int new_gy);

/* Context Menu API */
DosGuiContextMenu *dosgui_ctx_menu_create(int x, int y);
void dosgui_ctx_menu_add_item(DosGuiContextMenu *menu, const char *label,
                               void (*action)(void));
void dosgui_ctx_menu_add_separator(DosGuiContextMenu *menu);
DosGuiContextMenu *dosgui_ctx_menu_add_submenu(DosGuiContextMenu *menu, const char *label);
void dosgui_ctx_menu_show(DosGuiContextMenu *menu, int x, int y);
void dosgui_ctx_menu_hide(DosGuiContextMenu *menu);
void dosgui_ctx_menu_handle_mouse(int x, int y, int btn, int kind);
void dosgui_ctx_menu_render(uint32_t *fb, int fb_w, int fb_h);

/* Shortcut Creation */
int dosgui_shortcut_create(const char *name, const char *target,
                            const char *description, int grid_x, int grid_y);
int dosgui_shortcut_create_url(const char *name, const char *url, int grid_x, int grid_y);

/* Default context menus */
void dosgui_icon_show_context_menu(int icon_idx, int mx, int my);
void dosgui_desktop_show_context_menu(int mx, int my);

/* -- Desktop Icons ----------------------------------------------- */

void dosgui_icon_render(uint32_t *fb, int fb_w, int fb_h);
int  dosgui_icon_hit_test(int mx, int my);

/* Tick */
void dosgui_tick(void);

/* -- Query ------------------------------------------------------- */

int dosgui_wm_screen_w(void);
int dosgui_wm_screen_h(void);

/* Accessor for WM state fields */
int dosgui_wm_get_icon_count(void);

/* -- Icon persistence & wallpaper reload (Desktop fixup Stream 2/4) - */
DosGuiIcon *dosgui_icon_get(int idx);
/* Compact dead (alive==false) icons out of the array, keeping icon_count dense. */
void dosgui_wm_compact_icons(void);
void dosgui_wm_save_icon_layout(void);      /* Persist live icon grid into settings */
void dosgui_wm_restore_icon_layout(void);   /* Restore live icon grid from settings */
void dosgui_wm_reload_wallpaper(void);      /* Re-decode wallpaper from settings path */
bool dosgui_wm_is_initialized(void);
int  dosgui_wm_wallpaper_mode(void);
int  dosgui_wm_wallpaper_w(void);
int  dosgui_wm_wallpaper_h(void);
/* Desktop view options (ReactOS explorer/desktop.cpp lesson). */
void dosgui_wm_refresh_desktop(void);       /* Re-scan ~/Desktop for live icons */
void dosgui_wm_set_auto_arrange(bool on);   /* Toggle + persist auto-arrange */
bool dosgui_wm_get_auto_arrange(void);
void dosgui_wm_set_icons_visible(bool show);/* Live show/hide all desktop icons */

/* -- Invalidation tracking (legacy wm_invalidate compat) --------- */
void dosgui_wm_invalidate(DosGuiWindow *win);
void dosgui_wm_invalidate_all(void);
bool dosgui_wm_poll_dirty(int *out_id);   /* drain; -1 = redraw all */
int  dosgui_wm_dirty_count(void);         /* -1 if full-redraw pending */

/* -- HolyC Terminal ---------------------------------------------- */

DosGuiWindow *dosgui_wm_spawn_holyc_term(int x, int y, int w, int h);

#endif /* WUBU_DOSGUI_WM_H */
