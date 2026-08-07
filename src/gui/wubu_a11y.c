/*
 * wubu_a11y.c  --  WuBuOS Accessibility Control Cluster
 *
 * The "GameCube face-button" psychology layout, reversed for the desktop:
 *   A = big GREEN orb  -> grab & drag to MOVE the window (largest = most
 *       used; green = "go"; easy for elderly + children)
 *   Y = yellow pill    -> click MINIMIZES, drag ROTATES the window 90°
 *       (aspect-ratio rotation for mobile-site viewing + future
 *       smart-rotate apps)
 *   B = red orb with X -> click CLOSES (ends the session); DRAGGING it
 *       PURGES the cache/temp files (incognito-style wipe) then closes
 *   ◆ = purple corner  -> big easy-grab RESIZE handle at the bottom-right
 *       (the opposite corner from the green A, mirroring the image)
 *
 * Color/size/shape psychology: green=go, yellow=transform, red=stop;
 * biggest = most important; the rounded window corner itself is an
 * accessibility affordance the cluster anchors onto.
 *
 * Two-tier UX: newcomers/politicians/children acclimate on this cluster;
 * power users stay in the comfy Win98/XP chrome.
 */

#include "wubu_a11y.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../gui/wubu_trash.h"

#include <stdlib.h>
#include <string.h>

/* -- A11y semantic palette (deliberate fixed colors: accessibility
 * psychology overrides theme churn; the image's soft neumorphic feel is
 * approximated with flat fills + raised light/dark edges). */
#define A11Y_PANEL_FACE  0xFFE8E0F8  /* soft lavender squircle */
#define A11Y_PANEL_DARK  0xFFC9BFE8  /* panel lower edge */
#define A11Y_GREEN       0xFF4CAF50  /* A orb */
#define A11Y_GREEN_DARK  0xFF2E7D32
#define A11Y_YELLOW      0xFFFFB300  /* Y pill */
#define A11Y_YELLOW_DARK 0xFFB26A00
#define A11Y_RED         0xFFE53935  /* B orb */
#define A11Y_RED_DARK    0xFFB71C1C
#define A11Y_PURPLE      0xFF9C27B0  /* resize corner */
#define A11Y_PURPLE_DARK 0xFF6A1B9A
#define A11Y_GLYPH       0xFF212121  /* dark glyph on the orbs */

#define A11Y_ORB_A_R   16   /* green: biggest */
#define A11Y_ORB_B_R   12   /* red: medium */
#define A11Y_PILL_W    36
#define A11Y_PILL_H    14
#define A11Y_CORNER_S  22   /* purple corner square */

#define A11Y_CLICK_TOL 6    /* px of movement before a press is a drag */

static bool g_a11y_enabled = false;

/* Drag state for the active gesture. */
typedef struct {
    WuBuA11yControl ctrl;
    int  start_x, start_y;   /* press point (screen) */
    int  grab_ox, grab_oy;   /* window pos minus press point (for move) */
    int  orig_w, orig_h;     /* window size at press (for resize/rotate) */
    bool moved;              /* exceeded click tolerance */
} A11yDrag;

static A11yDrag g_drag = { WUBU_A11Y_NONE, 0, 0, 0, 0, 0, 0, false };

/* -- Public switch ------------------------------------------------ */

void wubu_a11y_set_enabled(bool on) {
    g_a11y_enabled = on;
    if (!on) g_drag.ctrl = WUBU_A11Y_NONE;
}
bool wubu_a11y_is_enabled(void) { return g_a11y_enabled; }

/* -- Panel geometry ------------------------------------------------ */

/* The cluster panel sits on the window's top-left rounded corner,
 * overlapping it slightly so the corner reads as the affordance. */
static void panel_rect(DosGuiWindow *w, int *px, int *py) {
    *px = w->x + WUBU_A11Y_PANEL_OFFX;
    *py = w->y + WUBU_A11Y_PANEL_OFFY;
}

static void orb_a_center(DosGuiWindow *w, int *cx, int *cy) {
    int px, py; panel_rect(w, &px, &py);
    *cx = px + 26;      /* below the pill, left side */
    *cy = py + 52;
}
static void orb_b_center(DosGuiWindow *w, int *cx, int *cy) {
    int px, py; panel_rect(w, &px, &py);
    *cx = px + 58;      /* right of the green orb */
    *cy = py + 52;
}
static void pill_center(DosGuiWindow *w, int *cx, int *cy) {
    int px, py; panel_rect(w, &px, &py);
    *cx = px + A11Y_PILL_W / 2 + 8;
    *cy = py + A11Y_PILL_H / 2 + 6;
}
/* Purple resize corner sits at the window's bottom-right (the opposite
 * corner from the green grab, mirroring the sizing grip). */
static void corner_rect(DosGuiWindow *w, int *cx, int *cy) {
    *cx = w->x + w->w - A11Y_CORNER_S;
    *cy = w->y + w->h - A11Y_CORNER_S;
}

/* -- Hit testing --------------------------------------------------- */

WuBuA11yControl wubu_a11y_hit(DosGuiWindow *win, int x, int y) {
    if (!win || !win->alive || (win->flags & DOSGUI_WIN_MINIMIZED) || !g_a11y_enabled)
        return WUBU_A11Y_NONE;

    /* Resize corner first (it can overlap other windows' cluster zones). */
    int cx, cy; corner_rect(win, &cx, &cy);
    if (x >= cx && x < cx + A11Y_CORNER_S &&
        y >= cy && y < cy + A11Y_CORNER_S)
        return WUBU_A11Y_RESIZE;

    int ox, oy; orb_a_center(win, &ox, &oy);
    int dx = x - ox, dy = y - oy;
    if (dx * dx + dy * dy <= A11Y_ORB_A_R * A11Y_ORB_A_R) return WUBU_A11Y_GRAB;

    orb_b_center(win, &ox, &oy);
    dx = x - ox; dy = y - oy;
    if (dx * dx + dy * dy <= A11Y_ORB_B_R * A11Y_ORB_B_R) return WUBU_A11Y_CLOSE;

    pill_center(win, &ox, &oy);
    if (x >= ox - A11Y_PILL_W / 2 && x < ox + A11Y_PILL_W / 2 &&
        y >= oy - A11Y_PILL_H / 2 && y < oy + A11Y_PILL_H / 2)
        return WUBU_A11Y_ROTATE;

    return WUBU_A11Y_NONE;
}

/* -- Drawing ------------------------------------------------------- */

static void draw_orb(int cx, int cy, int r, uint32_t face, uint32_t dark) {
    /* Raised feel: dark crescent lower-right, face fill, light top. */
    vbe_fill_circle(cx + 1, cy + 1, r, dark);
    vbe_fill_circle(cx, cy, r, face);
}

static void draw_grab_glyph(int cx, int cy, int r) {
    /* Folded-corner mark: a diagonal line with a notch (the image's A). */
    int s = r - 5;
    vbe_fill_rect(cx - s, cy + s - 2, 2, 3, A11Y_GLYPH);
    for (int i = 0; i <= s; i++) {
        vbe_fill_rect(cx - s + i, cy + s - 2 - i, 2, 3, A11Y_GLYPH);
    }
    /* the notch: clear a small triangle near the top-right of the fold */
    vbe_fill_rect(cx + s - 3, cy - s + 2, 3, 2, A11Y_GREEN);
}

static void draw_close_glyph(int cx, int cy, int r) {
    int s = r - 4;
    vbe_fill_rect(cx - s, cy - 1, 2 * s, 2, A11Y_GLYPH);
    vbe_fill_rect(cx - 1, cy - s, 2, 2 * s, A11Y_GLYPH);
}

static void draw_rotate_glyph(int cx, int cy) {
    /* Rotation arc indicator: two small arrows (simplified: chevron pair). */
    int s = 5;
    vbe_fill_rect(cx - s, cy - 1, 2 * s, 2, A11Y_GLYPH);
    /* arrowhead right */
    vbe_fill_rect(cx + s - 2, cy - 3, 2, 2, A11Y_GLYPH);
    vbe_fill_rect(cx + s - 2, cy + 1, 2, 2, A11Y_GLYPH);
}

static void draw_purple_corner(int cx, int cy) {
    vbe_fill_rect(cx + 1, cy + 1, A11Y_CORNER_S, A11Y_CORNER_S, A11Y_PURPLE_DARK);
    vbe_fill_rect(cx, cy, A11Y_CORNER_S, A11Y_CORNER_S, A11Y_PURPLE);
    /* grip: diagonal notch */
    for (int i = 0; i < 6; i++) {
        vbe_fill_rect(cx + A11Y_CORNER_S - 6 - i, cy + 2 + i * 3, 2, 2, A11Y_PURPLE_DARK);
    }
}

void wubu_a11y_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    if (!win || !win->alive || !g_a11y_enabled) return;

    int px, py; panel_rect(win, &px, &py);

    /* Panel: raised lavender squircle. */
    vbe_fill_rect_rounded(px + 2, py + 2, WUBU_A11Y_PANEL_W, WUBU_A11Y_PANEL_H, 14, A11Y_PANEL_DARK);
    vbe_fill_rect_rounded(px, py, WUBU_A11Y_PANEL_W, WUBU_A11Y_PANEL_H, 14, A11Y_PANEL_FACE);

    /* Y pill (top). */
    int ycx, ycy; pill_center(win, &ycx, &ycy);
    vbe_fill_rect_rounded(ycx - A11Y_PILL_W / 2 + 1, ycy - A11Y_PILL_H / 2 + 1,
                          A11Y_PILL_W, A11Y_PILL_H, 7, A11Y_YELLOW_DARK);
    vbe_fill_rect_rounded(ycx - A11Y_PILL_W / 2, ycy - A11Y_PILL_H / 2,
                          A11Y_PILL_W, A11Y_PILL_H, 7, A11Y_YELLOW);
    draw_rotate_glyph(ycx, ycy);

    /* A orb (big green, the primary action). */
    int ax, ay; orb_a_center(win, &ax, &ay);
    draw_orb(ax, ay, A11Y_ORB_A_R, A11Y_GREEN, A11Y_GREEN_DARK);
    draw_grab_glyph(ax, ay, A11Y_ORB_A_R);

    /* B orb (red X). */
    int bx, by; orb_b_center(win, &bx, &by);
    draw_orb(bx, by, A11Y_ORB_B_R, A11Y_RED, A11Y_RED_DARK);
    draw_close_glyph(bx, by, A11Y_ORB_B_R);

    /* Purple resize corner at the window's bottom-right. */
    int cx, cy; corner_rect(win, &cx, &cy);
    draw_purple_corner(cx, cy);
}

/* -- Direct actions ------------------------------------------------ */

void wubu_a11y_rotate_window(DosGuiWindow *win) {
    if (!win || !win->alive || (win->flags & DOSGUI_WIN_MAXIMIZED)) return;
    /* 90° rotation: swap w/h, keep the center fixed (aspect-ratio flip). */
    int cx = win->x + win->w / 2;
    int cy = win->y + win->h / 2;
    int nw = win->h, nh = win->w;
    dosgui_wm_resize(win, nw, nh);
    dosgui_wm_move(win, cx - nw / 2, cy - nh / 2);
}

void wubu_a11y_minimize_window(DosGuiWindow *win) {
    if (win && win->alive) dosgui_wm_minimize(win);
}

void wubu_a11y_close_window(DosGuiWindow *win) {
    if (win && win->alive) dosgui_wm_destroy(win);
}

void wubu_a11y_purge_and_close(DosGuiWindow *win) {
    if (!win) return;
    /* Incognito-style wipe: empty the Recycle Bin (cache/temp files are
     * moved there by trash_move; this is the "browser incognito delete"). */
    wubu_trash_empty();
    dosgui_wm_destroy(win);
}

/* -- Mouse routing ------------------------------------------------- */

bool wubu_a11y_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    if (!g_a11y_enabled || !win || !win->alive) return false;

    if (kind == 1) {  /* press */
        WuBuA11yControl c = wubu_a11y_hit(win, x, y);
        if (c == WUBU_A11Y_NONE) return false;
        g_drag.ctrl    = c;
        g_drag.start_x = x;
        g_drag.start_y = y;
        g_drag.grab_ox = win->x - x;
        g_drag.grab_oy = win->y - y;
        g_drag.orig_w  = win->w;
        g_drag.orig_h  = win->h;
        g_drag.moved   = false;
        if (c == WUBU_A11Y_GRAB || c == WUBU_A11Y_ROTATE || c == WUBU_A11Y_CLOSE)
            dosgui_wm_set_focus(win);
        return true;
    }

    if (kind == 0) {  /* move */
        if (g_drag.ctrl == WUBU_A11Y_NONE) return false;
        int dx = x - g_drag.start_x, dy = y - g_drag.start_y;
        if (!g_drag.moved && (dx * dx + dy * dy) > A11Y_CLICK_TOL * A11Y_CLICK_TOL)
            g_drag.moved = true;

        switch (g_drag.ctrl) {
        case WUBU_A11Y_GRAB:
            /* Live window move while dragging the green orb. */
            dosgui_wm_move(win, x + g_drag.grab_ox, y + g_drag.grab_oy);
            break;
        case WUBU_A11Y_RESIZE:
            if (g_drag.moved) {
                int nw = g_drag.orig_w + (x - g_drag.start_x);
                int nh = g_drag.orig_h + (y - g_drag.start_y);
                if (nw < 80)  nw = 80;
                if (nh < 40)  nh = 40;
                dosgui_wm_resize(win, nw, nh);
            }
            break;
        default:
            break;  /* rotate/close decide on release */
        }
        return true;
    }

    if (kind == 2) {  /* release */
        WuBuA11yControl c = g_drag.ctrl;
        if (c == WUBU_A11Y_NONE) return false;
        g_drag.ctrl = WUBU_A11Y_NONE;

        switch (c) {
        case WUBU_A11Y_GRAB:
            break;  /* already moved live */
        case WUBU_A11Y_ROTATE:
            if (g_drag.moved) wubu_a11y_rotate_window(win);
            else              wubu_a11y_minimize_window(win);
            break;
        case WUBU_A11Y_CLOSE:
            if (g_drag.moved) wubu_a11y_purge_and_close(win);
            else              wubu_a11y_close_window(win);
            break;
        case WUBU_A11Y_RESIZE:
            break;
        default:
            break;
        }
        return true;
    }

    return false;
}
