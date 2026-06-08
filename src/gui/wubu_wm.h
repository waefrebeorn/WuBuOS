/*
 * wubu_wm.h — WuBuOS Window Manager
 *
 * Cell 395: Full WM — drag, resize, GAAD snap, minimize/maximize,
 * virtual desktops, z-order, close, theme integration.
 *
 * Features from every OS:
 *   - Win98:  3D raised/sunken chrome, title bar close/min/max
 *   - XP:     Luna gradient title, rounded buttons
 *   - Linux:  Virtual desktops (workspaces), Alt+drag anywhere
 *   - macOS:  Double-click title = maximize (roll-up/unroll)
 *   - WuBu:   GAAD feng shui snap on drag release
 */
#ifndef WUBU_WM_H
#define WUBU_WM_H

#include <stdint.h>
#include <stdbool.h>
#include "wubu_theme.h"
#include "../kernel/wubu_gaad.h"

/* ── Constants ──────────────────────────────────────────────────── */

#define WUBU_WM_MAX_WINDOWS    64
#define WUBU_WM_MAX_DESKTOPS   9     /* 1-9 virtual desktops */
#define WUBU_WM_TITLE_H        20
#define WUBU_WM_BORDER_W       3
#define WUBU_WM_SNAP_DIST      30    /* GAAD snap threshold in pixels */

/* ── Window State ───────────────────────────────────────────────── */

typedef enum {
    WUBU_WIN_UNUSED    = 0,
    WUBU_WIN_NORMAL    = 1,    /* Visible, normal position */
    WUBU_WIN_MINIMIZED = 2,    /* Minimized to taskbar */
    WUBU_WIN_MAXIMIZED = 4,    /* Full screen */
    WUBU_WIN_FOCUSED   = 8,    /* Has keyboard focus */
    WUBU_WIN_STICKY    = 16,   /* Visible on all desktops */
    WUBU_WIN_NOCLOSE   = 32,   /* No close button */
} WubuWinFlags;

/* ── Drag State ─────────────────────────────────────────────────── */

typedef enum {
    DRAG_NONE      = 0,    /* Not dragging */
    DRAG_MOVE      = 1,    /* Moving window (title bar or Alt+click) */
    DRAG_RESIZE_E  = 2,    /* Resizing from east edge */
    DRAG_RESIZE_W  = 3,    /* Resizing from west edge */
    DRAG_RESIZE_S  = 4,    /* Resizing from south edge */
    DRAG_RESIZE_N  = 5,    /* Resizing from north edge */
    DRAG_RESIZE_SE = 6,    /* Resizing from SE corner */
    DRAG_RESIZE_SW = 7,    /* Resizing from SW corner */
    DRAG_RESIZE_NE = 8,    /* Resizing from NE corner */
    DRAG_RESIZE_NW = 9,    /* Resizing from NW corner */
} WubuDragKind;

/* ── Window Structure ───────────────────────────────────────────── */

typedef struct WubuWin WubuWin;
struct WubuWin {
    int          id;
    WubuWinFlags flags;
    int          desktop;     /* Which virtual desktop (0-based) */
    int          x, y, w, h;  /* Position and size */
    int          save_x, save_y, save_w, save_h;  /* Saved before maximize */
    int          min_w, min_h; /* Minimum size */
    int          z_order;     /* Higher = on top */
    char         title[64];

    /* GAAD snap state */
    bool         was_snapped; /* True if window is currently GAAD-snapped */
    int          snap_region; /* Which GAAD region it snapped to (-1 = none) */

    /* Callbacks */
    void (*on_draw)(WubuWin *win, void *fb, int fb_w, int fb_h);
    void (*on_key)(WubuWin *win, uint32_t key, uint32_t mods);
    void (*on_mouse)(WubuWin *win, int x, int y, int btn, int kind);
    void (*on_close)(WubuWin *win);
    void (*on_resize)(WubuWin *win, int w, int h);

    void *user_data;
};

/* ── Virtual Desktop ────────────────────────────────────────────── */

typedef struct {
    int  current;              /* Active desktop (0-based) */
    int  count;                /* Total desktops (1-9) */
    char names[WUBU_WM_MAX_DESKTOPS][16];  /* Desktop names */
} WubuDesktops;

/* ── WM State ───────────────────────────────────────────────────── */

typedef struct {
    int           screen_w, screen_h;
    WubuWin       windows[WUBU_WM_MAX_WINDOWS];
    int           next_id;
    int           focused_id;
    WubuDesktops  desktops;
    WubuGaadDecomp gaad;          /* GAAD decomposition of screen */
    WubuFengShui   feng_shui;     /* Feng shui snap layout */

    /* Drag state */
    WubuDragKind  drag_kind;
    int           drag_win_id;
    int           drag_start_x, drag_start_y;  /* Mouse start */
    int           drag_win_x, drag_win_y;      /* Window start pos */
    int           drag_win_w, drag_win_h;      /* Window start size */
    bool          gaad_snap_preview;            /* Show snap preview? */
} WubuWM;

/* ── WM Lifecycle ──────────────────────────────────────────────── */

int  wubu_wm_init(int screen_w, int screen_h);
void wubu_wm_shutdown(void);

/* ── Window Management ─────────────────────────────────────────── */

WubuWin *wubu_wm_create(int x, int y, int w, int h, const char *title);
void     wubu_wm_destroy(WubuWin *win);
void     wubu_wm_set_focus(WubuWin *win);
WubuWin *wubu_wm_get_focused(void);
WubuWin *wubu_wm_find(int id);
int      wubu_wm_count(void);

/* ── Window Operations ─────────────────────────────────────────── */

void wubu_wm_minimize(WubuWin *win);
void wubu_wm_maximize(WubuWin *win);
void wubu_wm_restore(WubuWin *win);
void wubu_wm_close(WubuWin *win);

/* Move window (during drag — free grid, no snap yet) */
void wubu_wm_move(WubuWin *win, int x, int y);

/* Resize window */
void wubu_wm_resize(WubuWin *win, int w, int h);

/* Snap window to nearest GAAD region (called on drag release) */
void wubu_wm_gaad_snap(WubuWin *win);

/* ── Virtual Desktops ──────────────────────────────────────────── */

void wubu_wm_desktop_switch(int desktop);  /* 0-based */
void wubu_wm_desktop_next(void);
void wubu_wm_desktop_prev(void);
int  wubu_wm_desktop_current(void);
int  wubu_wm_desktop_count(void);
void wubu_wm_desktop_set_count(int count);
void wubu_wm_desktop_move_win(WubuWin *win, int desktop);

/* ── Input Handling ────────────────────────────────────────────── */

void wubu_wm_handle_key(uint32_t key, uint32_t mods);
void wubu_wm_handle_mouse(int x, int y, int btn, int kind);
/* kind: 0=move, 1=down, 2=up, 3=scroll */

/* ── Rendering ─────────────────────────────────────────────────── */

void wubu_wm_render(uint32_t *fb, int fb_w, int fb_h);
void wubu_wm_invalidate(WubuWin *win);

/* ── GAAD / Resolution ─────────────────────────────────────────── */

/* Recompute GAAD decomposition (after resolution change) */
void wubu_wm_gaad_recompute(void);

/* Change screen resolution (re-decomposes GAAD, repositions windows) */
void wubu_wm_set_resolution(int w, int h);

/* Get WM state (for hosted binary main loop) */
WubuWM *wubu_wm_state(void);

#endif /* WUBU_WM_H */
