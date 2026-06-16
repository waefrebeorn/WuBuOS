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

#define DOSGUI_MAX_WINDOWS    32
#define DOSGUI_TITLE_H        20
#define DOSGUI_TASK_H         28
#define DOSGUI_BORDER          2

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
    char           title[64];
    bool           alive;
    /* Content render callback */
    void         (*on_draw)(DosGuiWindow *win, uint32_t *fb,
                            int fb_w, int fb_h);
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

/* -- Input ------------------------------------------------------- */

void dosgui_wm_handle_key(uint32_t key, uint32_t mods);
void dosgui_wm_handle_mouse(int x, int y, int btn, int kind);
/* kind: 0=move, 1=down, 2=up */

/* -- Rendering --------------------------------------------------- */

void dosgui_wm_render(uint32_t *fb, int fb_w, int fb_h);
void dosgui_wm_render_desktop(uint32_t *fb, int fb_w, int fb_h);

/* -- Taskbar / Desktop ------------------------------------------- */

int  dosgui_taskbar_height(void);
void dosgui_taskbar_render(uint32_t *fb, int fb_w, int fb_h);

/* -- Desktop Icons ----------------------------------------------- */

#define DOSGUI_MAX_ICONS 16
#define DOSGUI_ICON_SIZE 32
#define DOSGUI_ICON_GAP  8

typedef struct {
    char name[32];
    int  x, y;          /* Grid position in pixels */
    int  grid_x, grid_y;
    bool alive;
    void (*on_click)(void);
} DosGuiIcon;

int  dosgui_icon_add(const char *name, int gx, int gy,
                     void (*on_click)(void));
void dosgui_icon_render(uint32_t *fb, int fb_w, int fb_h);
int  dosgui_icon_hit_test(int mx, int my);

/* Tick */
void dosgui_tick(void);

/* -- Query ------------------------------------------------------- */

int dosgui_wm_screen_w(void);
int dosgui_wm_screen_h(void);

#endif /* WUBU_DOSGUI_WM_H */
