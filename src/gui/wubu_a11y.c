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
#include "dosgui_wm_internal.h"   /* title_bar_height() for the panel anchor */

#include <stdlib.h>
#include <string.h>

/* -- A11y semantic palette (deliberate fixed colors: accessibility
 * psychology overrides theme churn; the image's soft neumorphic feel is
 * approximated with flat fills + raised light/dark edges). */
#define A11Y_PANEL_FACE  0xFFE8E0F8  /* soft lavender squircle */
#define A11Y_PANEL_DARK  0xFFC9BFE8  /* panel lower edge */
#define A11Y_GREEN       0xFF42A85A  /* A orb (reference: ~66,170,93) */
#define A11Y_GREEN_DARK  0xFF1B5E20  /* dark glyph on the green orb */
#define A11Y_YELLOW      0xFFDE9E44  /* Y pill (reference: ~222,158,68) */
#define A11Y_YELLOW_DARK 0xFF7A4A10  /* pill handle slot */
#define A11Y_RED         0xFFE53935  /* B orb */
#define A11Y_RED_DARK    0xFF8E1A1A  /* dark X (reference: ~133,38,34) */
#define A11Y_PURPLE      0xFF9C27B0  /* resize corner */
#define A11Y_PURPLE_DARK 0xFF6A1B9A
#define A11Y_GLYPH       0xFF212121  /* dark glyph on the orbs */

#define A11Y_ORB_A_R   31   /* green A: BIGGEST (real GC 16.747mm A vs 6.449mm B
                               = 2.6:1; 62px diameter >= WCAG AAA 44px) */
#define A11Y_ORB_B_R   12   /* red B: smallest (24px = WCAG AA 2.5.8 min) */
#define A11Y_ORB_Y_R   14   /* yellow X: round-tip CRESCENT (28px) */
#define A11Y_ORB_P_R   14   /* purple Y: round-tip CRESCENT, resize (28px) */
#define A11Y_PURPLE_FADE 46 /* px: within this the purple resize fades in */
#define A11Y_PURGE_TOL  30  /* red: drag beyond this = full drag = purge */

/* GC-controller diamond layout (user's design philosophy, reference-traced):
 *      yellow (TL, minimize/rotate)   green A (TR, BIG, maximize/move)
 *      [purple Y: at the WINDOW's bottom-left AND bottom-right corners,
 *       resize, edge-detection reveal — NOT in the cluster anymore]
 *      red B (BR, smallest, close/purge) */

#define A11Y_CLICK_TOL 6    /* px of movement before a press is a drag */

static bool g_a11y_enabled = false;
/* STIM state: a partial drag on the red B button is feedback, not an
 * action — g_stim_frames counts down while the red orb pulses/buzzes. */
static int g_stim_frames = 0;

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

/* -- Cluster geometry ------------------------------------------------
 * Anchored to the window's CONTENT RECT (below the title bar, inside the
 * border), not floating at arbitrary ratios. The content rect is what
 * apps actually draw into; placing buttons at floating mid-window
 * coordinates creates "random" placement that ignores chrome.
 *
 * Rules (synthesized from Apple HIG, MS WinUI, Wikipedia Window_decoration,
 * Fitts's law, WCAG 2.5.5 / 2.5.8 — see docs/a11y-window-placement-research.md):
 *
 *   R1  Anchor = (win.x + border_w + 8, win.y + title_bar_height + 8)
 *                — content area top-left + 8px breathing room.
 *   R2  Cluster floats in the content area, NEVER over the title bar.
 *   R3  Buttons scale with the content rect (proportional to it).
 *   R4  Yellow X minimize:  ANCHOR + (0.18 cw, 0.16 ch) — up-left of green.
 *   R5  Green A  maximize:  ANCHOR + (0.50 cw, 0.32 ch) — LEFT of red, same row.
 *   R6  Red B    close:     ANCHOR + (0.66 cw, 0.32 ch) — RIGHT of green.
 *   R7  Purple resize: window bottom-left AND bottom-right corners
 *                       (asymmetric edge-detection — Fitts: corners are
 *                       "infinite" edges so the cursor can't overshoot).
 */
static void cluster_anchor(DosGuiWindow *w, int *ax, int *ay) {
    int bw  = border_width();
    int tbh = title_bar_height();
    *ax = w->x + bw + 8;
    *ay = w->y + tbh + 8;
}

static void orb_y_center(DosGuiWindow *w, int *cx, int *cy) {
    int ax, ay; cluster_anchor(w, &ax, &ay);
    int bw  = border_width();
    int tbh = title_bar_height();
    int cw  = w->w - 2 * bw - 16;            /* content width  */
    int ch  = w->h - tbh - bw - 16;          /* content height */
    *cx = ax + (int)(cw * 0.18f);
    *cy = ay + (int)(ch * 0.16f);
}
static void orb_a_center(DosGuiWindow *w, int *cx, int *cy) {
    int ax, ay; cluster_anchor(w, &ax, &ay);
    int bw  = border_width();
    int tbh = title_bar_height();
    int cw  = w->w - 2 * bw - 16;
    int ch  = w->h - tbh - bw - 16;
    *cx = ax + (int)(cw * 0.50f);
    *cy = ay + (int)(ch * 0.32f);
}
static void orb_b_center(DosGuiWindow *w, int *cx, int *cy) {
    int ax, ay; cluster_anchor(w, &ax, &ay);
    int bw  = border_width();
    int tbh = title_bar_height();
    int cw  = w->w - 2 * bw - 16;
    int ch  = w->h - tbh - bw - 16;
    *cx = ax + (int)(cw * 0.66f);
    *cy = ay + (int)(ch * 0.32f);
}
static void orb_p_bl(DosGuiWindow *w, int *cx, int *cy) {
    int bw = border_width();
    *cx = w->x + bw + 22;
    *cy = w->y + w->h - bw - 22;
}
static void orb_p_br(DosGuiWindow *w, int *cx, int *cy) {
    int bw = border_width();
    *cx = w->x + w->w - bw - 22;
    *cy = w->y + w->h - bw - 22;
}

/* -- Hit testing --------------------------------------------------- */

WuBuA11yControl wubu_a11y_hit(DosGuiWindow *win, int x, int y) {
    if (!win || !win->alive || (win->flags & DOSGUI_WIN_MINIMIZED) || !g_a11y_enabled)
        return WUBU_A11Y_NONE;

    /* GC diamond, top-down (biggest first). */
    int ox, oy, dx, dy;

    orb_a_center(win, &ox, &oy);
    dx = x - ox; dy = y - oy;
    if (dx * dx + dy * dy <= A11Y_ORB_A_R * A11Y_ORB_A_R) return WUBU_A11Y_GRAB;

    orb_b_center(win, &ox, &oy);
    dx = x - ox; dy = y - oy;
    if (dx * dx + dy * dy <= A11Y_ORB_B_R * A11Y_ORB_B_R) return WUBU_A11Y_CLOSE;

    orb_y_center(win, &ox, &oy);
    dx = x - ox; dy = y - oy;
    if (dx * dx + dy * dy <= A11Y_ORB_Y_R * A11Y_ORB_Y_R) return WUBU_A11Y_ROTATE;

    /* Purple Y (resize): revealed only near the window's bottom-left /
     * bottom-right corners (edge detection — grab it once it appears). */
    int pxx, pyy;
    orb_p_bl(win, &pxx, &pyy);
    dx = x - pxx; dy = y - pyy;
    if (dx * dx + dy * dy <= A11Y_PURPLE_FADE * A11Y_PURPLE_FADE)
        return WUBU_A11Y_RESIZE;
    orb_p_br(win, &pxx, &pyy);
    dx = x - pxx; dy = y - pyy;
    if (dx * dx + dy * dy <= A11Y_PURPLE_FADE * A11Y_PURPLE_FADE)
        return WUBU_A11Y_RESIZE;

    return WUBU_A11Y_NONE;
}

/* -- Drawing ------------------------------------------------------- */

/* Blend a toward b by t (0..255). */
static uint32_t a11y_lerp(uint32_t a, uint32_t b, int t) {
    int ar = a & 0xFF, ag = (a >> 8) & 0xFF, ab = (a >> 16) & 0xFF;
    int br = b & 0xFF, bg = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF;
    int ir = (ar * (256 - t) + br * t) / 256;
    int ig = (ag * (256 - t) + bg * t) / 256;
    int ib = (ab * (256 - t) + bb * t) / 256;
    return (uint32_t)ir | ((uint32_t)ig << 8) | ((uint32_t)ib << 16);
}

/* Soft neumorphic 3D ball (REFERENCE sketch): gradient light top-left ->
 * darker bottom-right, a 1px darker rim, and a dark glyph on top. */
static void draw_orb(int cx, int cy, int r, uint32_t face, uint32_t dark) {
    vbe_fill_circle(cx, cy, r, face);
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy > r * r) continue;
            float nx = (float)dx / r, ny = (float)dy / r;
            uint32_t c = face;
            /* lighten toward top-left (glossy) */
            int tl = (int)((-nx - ny) * 0.5f * 255);
            if (tl > 0) c = a11y_lerp(c, 0xFFFFFF, tl > 110 ? 110 : tl);
            /* darken toward bottom-right (volume) */
            int br = (int)((nx + ny) * 0.5f * 255);
            if (br > 0) c = a11y_lerp(c, dark, br > 150 ? 150 : br);
            vbe_set_pixel(cx + dx, cy + dy, c);
        }
    /* 1px rim */
    for (int dy = -r - 1; dy <= r + 1; dy++)
        for (int dx = -r - 1; dx <= r + 1; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 > r * r && d2 <= (r + 1) * (r + 1))
                vbe_set_pixel(cx + dx, cy + dy, dark);
        }
}

/* TRUE BEAN CRESCENT (fixes the "two crescents" artifact — the old
 * offset-0.62r construction made the two circles barely intersect,
 * rendering a fat outer crescent + a small sliver crescent, looking
 * like two stacked crescents). This is a CONCENTRIC ring subtraction:
 * outer arc A minus inner arc B (same center, smaller radius), then
 * the inner arc is OFFSET by (dirx, diry) so the band is thicker on
 * one side and thin on the other — a bean / kidney / lune shape with
 * a SINGLE continuous band and naturally-rounded tips where the offset
 * inner arc meets the outer arc. */
static void draw_crescent(int cx, int cy, int r, uint32_t face, uint32_t dark,
                          float dirx, float diry) {
    /* Bean shape: outer circle radius r, inner circle radius r * 0.45
     * (band thickness = 55% of r on the THIN side), offset by 0.55 * r
     * in (dirx, diry). The tips where the inner circle meets the outer
     * are where r * 0.45 + offset_length = r, i.e. offset = 0.55 r. */
    float off = r * 0.55f;
    int   ox  = (int)(dirx * off);
    int   oy  = (int)(diry * off);
    int   ri  = (int)(r * 0.45f);  /* inner radius (same shape, smaller) */
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy > r*r) continue;            /* outside A */
            int qx = dx - ox, qy = dy - oy;
            if (qx*qx + qy*qy <= ri*ri) continue;          /* inside B (hollow) */
            /* we are on the bean band — single shape, no second crescent */
            float nx = (float)dx / r, ny = (float)dy / r;
            uint32_t c = face;
            int tl = (int)((-nx - ny) * 0.5f * 255);
            if (tl > 0) c = a11y_lerp(c, 0xFFFFFF, tl > 110 ? 110 : tl);
            int br = (int)((nx + ny) * 0.5f * 255);
            if (br > 0) c = a11y_lerp(c, dark, br > 150 ? 150 : br);
            vbe_set_pixel(cx + dx, cy + dy, c);
        }
    }
}

/* Crescent blended toward the framebuffer by alpha (edge-detection reveal).
 * Same bean construction as draw_crescent — NO separate shadow crescent,
 * just a single shape at the requested alpha. */
static void draw_crescent_fade(int cx, int cy, int r, uint32_t face,
                               uint32_t dark, float dirx, float diry,
                               int alpha) {
    float off = r * 0.55f;
    int   ox  = (int)(dirx * off);
    int   oy  = (int)(diry * off);
    int   ri  = (int)(r * 0.45f);
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy > r*r) continue;
            int qx = dx - ox, qy = dy - oy;
            if (qx*qx + qy*qy <= ri*ri) continue;
            float nx = (float)dx / r, ny = (float)dy / r;
            uint32_t c = face;
            int tl = (int)((-nx - ny) * 0.5f * 255);
            if (tl > 0) c = a11y_lerp(c, 0xFFFFFF, tl > 110 ? 110 : tl);
            int br = (int)((nx + ny) * 0.5f * 255);
            if (br > 0) c = a11y_lerp(c, dark, br > 150 ? 150 : br);
            vbe_set_pixel(cx + dx, cy + dy,
                          a11y_lerp(vbe_get_pixel(cx + dx, cy + dy), c, alpha));
        }
    }
}

/* Grab glyph: DARK folded-corner diagonal (the reference's dark-green
 * "Zotero-like" mark) — the primary easy-drag affordance. */
static void draw_grab_glyph(int cx, int cy, int r, uint32_t col) {
    int s = r - 5;
    for (int i = 0; i <= s; i++)
        vbe_fill_rect(cx - s + i, cy + s - 2 - i, 2, 3, col);
}

/* Close glyph: bold DARK X (the reference's dark-red X). */
static void draw_close_glyph(int cx, int cy, int r, uint32_t col) {
    int s = r - 4;
    for (int i = -s; i <= s; i++) {
        vbe_set_pixel(cx + i, cy + i, col);
        vbe_set_pixel(cx + i, cy - i, col);
        vbe_set_pixel(cx + i + 1, cy + i, col);
        vbe_set_pixel(cx + i + 1, cy - i, col);
    }
}

/* Resize glyph for the PURPLE round button: three diagonal grip lines
 * (the classic resize-handle pattern, now inside a round button). */
static void draw_resize_glyph(int cx, int cy, int r, uint32_t col) {
    int s = r - 5;
    for (int i = 0; i < 3; i++) {
        int gx = cx + s - 2 - i * 3;
        int gy = cy - s + 2 + i * 3;
        for (int k = 0; k < 5; k++)
            vbe_set_pixel(gx + k, gy + k, col);
    }
}

/* Soft circular drop shadow cast down-right (the reference's "floating"
 * neumorphic buttons — lighting from upper-left).  Blends the dark color
 * over whatever is already on the framebuffer (window face / content). */
static void draw_orb_shadow(int cx, int cy, int r, uint32_t color, int alpha) {
    int sr = r + 2;
    for (int dy = -sr; dy <= sr; dy++)
        for (int dx = -sr; dx <= sr; dx++) {
            if (dx * dx + dy * dy > sr * sr) continue;
            int px = cx + dx + 3, py = cy + dy + 4;   /* cast down-right */
            vbe_set_pixel(px, py, a11y_lerp(vbe_get_pixel(px, py), color, alpha));
        }
}

/* Soft rounded-rect shadow for the pill (same cast, rounded corners so it
 * hugs the pill silhouette instead of peeking as a rect). */

void wubu_a11y_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    if (!win || !win->alive || !g_a11y_enabled) return;

    /* NOTE: no panel background. The buttons float DIRECTLY on the window
     * (the user: "these buttons need to properly be ON OUR WINDOW NO
     * BACKGROUND" — the lavender panel box + its translucent shadow read as
     * a "bad JPEG transparency" cloud on the silver window face).
     * Vibe = GameCube: yellow bean TL, green A, red B, ghost purple beans
     * at the bottom corners. The buttons are the whole UI — no card. */

    /* YELLOW X (top-left): round-tip CRESCENT = a BEAN (the real GC X/Y
     * buttons are bean-shaped — Space World prototype). The reference shows
     * a band from 9 o'clock to 12 o'clock: hollow opens DOWN-RIGHT, the
     * band hugs the upper-left arc. Click = minimize, drag = ROTATE. */
    int yx, yy; orb_y_center(win, &yx, &yy);
    draw_crescent_fade(yx, yy + 2, A11Y_ORB_Y_R, A11Y_PANEL_DARK,
                       A11Y_PANEL_DARK, 0.707f, 0.707f, 95);  /* shadow */
    draw_crescent(yx, yy, A11Y_ORB_Y_R, A11Y_YELLOW, A11Y_YELLOW_DARK,
                  0.707f, 0.707f);
    /* handle slot along the band's belly (upper-left arc) */
    vbe_fill_rect_rounded(yx - 10, yy - 7, 10, 4, 2, A11Y_YELLOW_DARK);

    /* GREEN A (top-right): BIGGEST circle (reference r-ratio 2:1 vs red).
     * Click = MAXIMIZE, drag = move. Dark drag-arrows glyph. */
    int ax, ay; orb_a_center(win, &ax, &ay);
    draw_orb_shadow(ax, ay, A11Y_ORB_A_R, A11Y_PANEL_DARK, 100);
    draw_orb(ax, ay, A11Y_ORB_A_R, A11Y_GREEN, A11Y_GREEN_DARK);
    draw_grab_glyph(ax, ay, A11Y_ORB_A_R, A11Y_GREEN_DARK);

    /* RED B (bottom-right): smallest circle. Click = end session, FULL drag
     * = purge+close, PARTIAL drag = STIM. */
    int bx, by; orb_b_center(win, &bx, &by);
    if (g_stim_frames > 0) {
        int ring = A11Y_ORB_B_R + 2 + (14 - g_stim_frames) / 3;
        draw_orb_shadow(bx, by, ring, 0xFFFFE080, 150);
        g_stim_frames--;
    }
    draw_orb_shadow(bx, by, A11Y_ORB_B_R, A11Y_PANEL_DARK, 100);
    draw_orb(bx, by, A11Y_ORB_B_R, A11Y_RED, A11Y_RED_DARK);
    draw_close_glyph(bx, by, A11Y_ORB_B_R, A11Y_RED_DARK);

    /* PURPLE Y: round-tip crescents at the WINDOW's bottom-left AND
     * bottom-right corners. Windows-style edge detection: invisible until
     * the cursor nears either corner; fully revealed while resizing. */
    int pxx, pyy, pxx2, pyy2;
    orb_p_bl(win, &pxx, &pyy);
    orb_p_br(win, &pxx2, &pyy2);
    int mx = g_dwm.mouse_x, my = g_dwm.mouse_y;
    int dbl = (mx - pxx) * (mx - pxx) + (my - pyy) * (my - pyy);
    int dbr = (mx - pxx2) * (mx - pxx2) + (my - pyy2) * (my - pyy2);
    int d2 = dbl < dbr ? dbl : dbr;   /* nearest corner controls the fade */
    int alpha = 255;
    if (g_drag.ctrl != WUBU_A11Y_RESIZE) {
        int f0 = A11Y_ORB_P_R + 5, f1 = A11Y_PURPLE_FADE;
        if (d2 > f1 * f1)      alpha = 0;
        else if (d2 > f0 * f0) alpha = 255 - (d2 - f0 * f0) * 255 /
                                            (f1 * f1 - f0 * f0);
    }
    if (alpha > 6) {
        /* both crescents share the same fade; 9->12 bean orientation
         * (hollow opens down-right, band along the upper-left arc) */
        draw_crescent_fade(pxx, pyy + 2, A11Y_ORB_P_R, A11Y_PANEL_DARK,
                           A11Y_PANEL_DARK, 0.707f, 0.707f, alpha / 2);
        draw_crescent_fade(pxx, pyy, A11Y_ORB_P_R, A11Y_PURPLE, A11Y_PURPLE_DARK,
                           0.707f, 0.707f, alpha);
        draw_crescent_fade(pxx2, pyy2 + 2, A11Y_ORB_P_R, A11Y_PANEL_DARK,
                           A11Y_PANEL_DARK, 0.707f, 0.707f, alpha / 2);
        draw_crescent_fade(pxx2, pyy2, A11Y_ORB_P_R, A11Y_PURPLE, A11Y_PURPLE_DARK,
                           0.707f, 0.707f, alpha);
        /* diagonal grip in each crescent's belly (upper-left arc) */
        uint32_t gcol = a11y_lerp(A11Y_PURPLE_DARK, A11Y_PURPLE, 255 - alpha);
        draw_resize_glyph(pxx  - 3, pyy  - 3, A11Y_ORB_P_R, gcol);
        draw_resize_glyph(pxx2 - 3, pyy2 - 3, A11Y_ORB_P_R, gcol);
    }
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
            if (!g_drag.moved) dosgui_wm_maximize(win);  /* click A = maximize */
            break;  /* drag already moved live */
        case WUBU_A11Y_ROTATE:
            if (g_drag.moved) wubu_a11y_rotate_window(win);
            else              wubu_a11y_minimize_window(win);
            break;
        case WUBU_A11Y_CLOSE:
            if (!g_drag.moved) {
                wubu_a11y_close_window(win);            /* click = end session */
            } else {
                int dx = x - g_drag.start_x, dy = y - g_drag.start_y;
                int dist2 = dx * dx + dy * dy;
                if (dist2 >= A11Y_PURGE_TOL * A11Y_PURGE_TOL) {
                    wubu_a11y_purge_and_close(win);     /* FULL drag = purge */
                } else {
                    /* PARTIAL drag: NOT a close — it's a STIM (feedback
                     * buzz/pulse; the window stays alive). */
                    g_stim_frames = 14;                 /* ~14 frames of pulse */
                }
            }
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
