/*
 * wubu_a11y.h  --  WuBuOS Accessibility Control Cluster
 *
 * The "GameCube face-button" psychology layout (color + size + shape):
 *
 *   ┌──────────────────────────────────────────────┐
 *   │  ▬▬▬▬  (Y) yellow pill  — click: MINIMIZE     │
 *   │        drag: ROTATE (aspect-ratio rotation)   │
 *   │    ●    (A) big GREEN orb  — grab+drag: MOVE  │
 *   │          (largest = most used; green = "go")  │
 *   │   ╳    (B) red orb with X — click: CLOSE      │
 *   │        drag: PURGE cache/temp, then close     │
 *   └──────────────────────────────────────────────┘
 *   ◆ purple corner grab (bottom-right) — easy RESIZE
 *
 * Accessibility tiers: new users / children / politicians acclimate on
 * this friendly cluster; power users live in the comfy Win98/XP chrome.
 * The rounded window corner itself is an accessibility affordance — the
 * cluster anchors onto it.
 *
 * Self-contained C11: draws via VBE primitives, drives the WM through
 * its public API (dosgui_wm.h). No god headers.
 */

#ifndef WUBU_A11Y_H
#define WUBU_A11Y_H

#include <stdbool.h>
#include <stdint.h>
#include "dosgui_wm.h"

/* Which control a screen point hits (0 = none). */
typedef enum {
    WUBU_A11Y_NONE   = 0,
    WUBU_A11Y_GRAB   = 1,  /* green orb   — move window */
    WUBU_A11Y_ROTATE = 2,  /* yellow pill — minimize on click, rotate on drag */
    WUBU_A11Y_CLOSE  = 3,  /* red orb X   — close on click, purge on drag */
    WUBU_A11Y_RESIZE = 4,  /* purple corner — resize (bottom-right) */
} WuBuA11yControl;

/* Cluster panel metrics (screen-space, anchored at the window's top-left
 * rounded corner, slightly overlapping it so the corner reads as the
 * "safe to touch" affordance). The panel BACKGROUND is gone (buttons float
 * on the window face); these bound the cluster for positioning/hit-test.
 * Layout: yellow crescent TL, green A LEFT of red B on the SAME row
 * (reference trace: green cx 465 < red cx 741, cy equal), 2.6:1 radii. */
#define WUBU_A11Y_PANEL_W    124
#define WUBU_A11Y_PANEL_H    80
#define WUBU_A11Y_PANEL_OFFX -8   /* overlap the rounded corner */
#define WUBU_A11Y_PANEL_OFFY -8

/* Master switch. When disabled the WM renders/consumes nothing extra. */
void  wubu_a11y_set_enabled(bool on);
bool  wubu_a11y_is_enabled(void);

/* Draw the cluster anchored on the given window (top-left corner). */
void  wubu_a11y_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);

/* Hit-test in screen coordinates; returns the control under (x,y). */
WuBuA11yControl wubu_a11y_hit(DosGuiWindow *win, int x, int y);

/* Mouse event routing for the cluster.
 * Returns true if the event was consumed by the cluster (the WM must NOT
 * continue normal drag/resize/title-bar handling for that event).
 * kind: 0=move, 1=down, 2=up   (same convention as dosgui_wm_handle_mouse) */
bool wubu_a11y_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);

/* Keyboard navigation for the cluster (P0 a11y — keyboard-only reach).
 * key/mods use the WM's key encoding. Returns true if the key was consumed.
 *   Arrow keys cycle focus (Y->A->B->purple); Enter/Space activates the
 *   focused control. A visible focus ring highlights the focused orb. */
bool wubu_a11y_key(DosGuiWindow *win, uint32_t key, uint32_t mods);

/* Direct actions (also used by tests). */
void wubu_a11y_rotate_window(DosGuiWindow *win);   /* 90°: swap w/h keep center */
void wubu_a11y_minimize_window(DosGuiWindow *win);
void wubu_a11y_close_window(DosGuiWindow *win);    /* end session */
void wubu_a11y_purge_and_close(DosGuiWindow *win); /* empty trash + close */

#endif /* WUBU_A11Y_H */
