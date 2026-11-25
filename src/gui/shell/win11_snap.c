/*
 * win11_snap.c -- Win+Z snap-layout flyout (Windows 11 parity).
 *
 * WHY THIS FILE EXISTS SEPARATELY FROM wm_layout.c
 *   Geometry belongs to the WM core so snapping and rendering can never disagree;
 *   this file owns ONLY the picker: where the flyout sits, how each layout is drawn,
 *   and which zone a click landed in.  It reads the shared layout table instead of
 *   restating any rectangle.
 *
 * WHAT CHANGED FROM THE OLD PICKER
 *   The previous version offered six equal zones (halves + quarters) as if that were
 *   Windows 11's set.  It is not: Windows 11 offers six LAYOUTS of two to four
 *   UNEQUAL regions, you pick a layout and then which region to fill.  So each button
 *   now shows its actual arrangement -- thirds, a wide-left column with a split right
 *   side, quadrants -- and clicking a region snaps into THAT region rather than
 *   always taking the first one.
 *
 * C11, opaque API (win11_snap.h), no god headers.
 */

#include "wubu_gui_metrics.h"
#include "win11_snap.h"
#include "../wm_internal.h"
#include "../wm.h"
#include "../kernel/vbe.h"
#include "../wubu_theme.h"

#include <string.h>

#define SNAP_GAP     WUBU_METRIC_GAP_SM /* was 6: off the grid */
#define SNAP_PAD     8
/* The miniature must leave room for the zone separators to stay readable. */
#define SNAP_INNER_W 40
#define SNAP_INNER_H 22

static bool g_open = false;
static int  g_win_id = -1;

/* -- Snap assist ---------------------------------------------------- */

#define SNAP_ASSIST_MAX_CARDS 3

static int g_assist_active;
static int g_assist_zone;

bool win11_snap_assist_active(void) { return g_assist_active != 0; }
int  win11_snap_assist_zone(void)   { return g_assist_zone; }

void win11_snap_assist_close(void) { g_assist_active = 0; }

void win11_snap_assist_open(int free_zone, int exclude_id) {
    /* Nothing to offer when there is no other window: showing an empty picker would be a
     * modal dead end, which is worse than no assist at all. */
    int ids[SNAP_ASSIST_MAX_CARDS];
    if (win11_snap_assist_candidates(ids, SNAP_ASSIST_MAX_CARDS, exclude_id) <= 0) {
        g_assist_active = 0;
        return;
    }
    g_assist_zone = (free_zone == 0) ? 0 : 1;
    g_assist_active = 1;
}

/* Z-order is most-recently-focused-first, which is the order Windows shows these in. */
int win11_snap_assist_candidates(int *ids, int max, int exclude_id) {
    int n = 0;
    for (int z = 0; z < g_dwm.nz && n < max; z++) {
        int id = g_dwm.zorder[z];
        if (id < 0 || id >= DOSGUI_MAX_WINDOWS) continue;
        DosGuiWindow *w = &g_dwm.windows[id];
        if (!w->alive || id == exclude_id) continue;
        if (w->flags & DOSGUI_WIN_MINIMIZED) continue;
        if (w->desktop != g_dwm.current_desktop) continue;
        ids[n++] = id;
    }
    return n;
}

static void assist_cards(int fb_w, int fb_h, int *x0, int *y0, int *cw, int *ch, int *gap) {
    int task_h = taskbar_height_dynamic();
    int sh = fb_h - task_h;
    *gap = WUBU_SPACE_SM;
    *cw = (fb_w / 2 - 4 * WUBU_SPACE_SM) / SNAP_ASSIST_MAX_CARDS;
    *ch = *cw * 9 / 16;
    if (*cw > 220) *cw = 220;
    *ch = *cw * 9 / 16;
    int total = SNAP_ASSIST_MAX_CARDS * *cw + (SNAP_ASSIST_MAX_CARDS - 1) * *gap;
    *x0 = (g_assist_zone == 0) ? (fb_w / 4 - total / 2) : (fb_w * 3 / 4 - total / 2);
    *y0 = sh / 2 - *ch / 2;
    if (*y0 < 0) *y0 = 0;
}

void win11_snap_assist_render(uint32_t *fb, int fb_w, int fb_h) {
    /* Draws through vbe_* like every other surface in this file: that is the buffer
     * vbe_get_pixel() reads and the one the hosted pipeline presents. `fb` is only the
     * SOURCE for the thumbnails, and it is NULL in a headless test -- cards then show their
     * frame and title, which is enough for a gate to see them. */
    if (!g_assist_active) return;

    /* Dim the free half so the offer reads as a choice, not as part of the desktop. */
    int half_x = (g_assist_zone == 0) ? 0 : fb_w / 2;
    int task_h = taskbar_height_dynamic();
    for (int y = 0; y < fb_h - task_h; y++) {
        for (int x = half_x; x < half_x + fb_w / 2 && x < fb_w; x++) {
            uint32_t c = vbe_get_pixel(x, y);
            uint32_t r = ((c >> 16) & 0xFF) / 3, gg = ((c >> 8) & 0xFF) / 3, b = (c & 0xFF) / 3;
            vbe_set_pixel(x, y, (r << 16) | (gg << 8) | b);
        }
    }

    int ids[SNAP_ASSIST_MAX_CARDS];
    int n = win11_snap_assist_candidates(ids, SNAP_ASSIST_MAX_CARDS, -1);
    int x0, y0, cw, ch, gap;
    assist_cards(fb_w, fb_h, &x0, &y0, &cw, &ch, &gap);
    for (int i = 0; i < n; i++) {
        int cx = x0 + i * (cw + gap);
        DosGuiWindow *w = &g_dwm.windows[ids[i]];
        static uint32_t thumb[SNAP_ASSIST_MAX_CARDS][96 * 54];
        int have_thumb = fb && wm_thumb_capture(fb, fb_w, fb_h, w->x, w->y, w->w, w->h,
                                                thumb[i], 96, 54);
        for (int y = 0; y < ch; y++) {
            int py = y0 + y;
            if (py < 0 || py >= fb_h) continue;
            for (int x = 0; x < cw; x++) {
                int px = cx + x;
                if (px < 0 || px >= fb_w) continue;
                int edge = (x == 0 || y == 0 || x == cw - 1 || y == ch - 1);
                if (edge) { vbe_set_pixel(px, py, wubu_theme_colors()->border_face); continue; }
                if (!have_thumb) { vbe_set_pixel(px, py, wubu_theme_colors()->win_face); continue; }
                int tx = (x * 96) / (cw > 0 ? cw : 1), ty = (y * 54) / (ch > 0 ? ch : 1);
                vbe_set_pixel(px, py, thumb[i][ty * 96 + tx]);
            }
        }
        vbe_draw_text(cx + WUBU_SPACE_XS, y0 + ch - vbe_text_baseline_off(),
                      wm_window_title_at(ids[i]) ? wm_window_title_at(ids[i]) : "",
                      wubu_theme_colors()->win_title_text, 1);
    }
}

bool win11_snap_assist_card_rect(int i, int *x, int *y, int *w, int *h) {
    int ids[SNAP_ASSIST_MAX_CARDS];
    int n = win11_snap_assist_candidates(ids, SNAP_ASSIST_MAX_CARDS, -1);
    if (i < 0 || i >= n) return false;
    int x0, y0, cw, ch, gap;
    assist_cards(g_dwm.screen_w, g_dwm.screen_h, &x0, &y0, &cw, &ch, &gap);
    if (x) *x = x0 + i * (cw + gap);
    if (y) *y = y0;
    if (w) *w = cw;
    if (h) *h = ch;
    return true;
}

/* Click a card: that window takes the free half. A click anywhere else dismisses the offer,
 * which is what Windows does -- an assist that traps the pointer is worse than none. */
bool win11_snap_assist_click(int x, int y) {
    if (!g_assist_active) return false;
    int ids[SNAP_ASSIST_MAX_CARDS];
    int n = win11_snap_assist_candidates(ids, SNAP_ASSIST_MAX_CARDS, -1);
    int x0, y0, cw, ch, gap;
    assist_cards(g_dwm.screen_w, g_dwm.screen_h, &x0, &y0, &cw, &ch, &gap);
    for (int i = 0; i < n; i++) {
        int cx = x0 + i * (cw + gap);
        if (x >= cx && x < cx + cw && y >= y0 && y < y0 + ch) {
            DosGuiWindow *w = &g_dwm.windows[ids[i]];
            wm_snap_half(w, g_assist_zone == 0 ? -1 : +1);
            wm_set_focus(w);
            g_assist_active = 0;
            return true;
        }
    }
    g_assist_active = 0;
    return true;
}

void win11_snap_toggle(void) {
    if (!g_open) {
        if (g_dwm.focused_id < 0) return;
        g_win_id = g_dwm.focused_id;
        g_open = true;
        return;
    }
    g_open = false;
}

bool win11_snap_is_open(void) { return g_open; }
void win11_snap_close(void)   { g_open = false; }

int win11_snap_button_count(void) { return WUBU_SNAP_LAYOUTS; }

int win11_snap_button_size(int *w, int *h)
{
    if (w) *w = SNAP_INNER_W + 2 * SNAP_PAD;
    if (h) *h = SNAP_INNER_H + 2 * SNAP_PAD;
    return 0;
}

/* Flyout geometry: centred over the focused window's title bar, flipped
 * below it when there is no room above. */
static void win11_snap_layout(int fb_w, int fb_h, int *fx, int *fy,
                              int *fw, int *fh) {
    (void)fb_w; (void)fb_h;

    int bw, bh;
    win11_snap_button_size(&bw, &bh);
    *fw = WUBU_SNAP_LAYOUTS * bw + (WUBU_SNAP_LAYOUTS - 1) * SNAP_GAP;
    *fh = bh;

    DosGuiWindow *w = (g_win_id >= 0) ? &g_dwm.windows[g_win_id] : NULL;
    if (!w) { *fx = 0; *fy = 0; return; }

    *fx = w->x + (w->w - *fw) / 2;
    *fy = w->y - *fh - 8;
    if (*fy < 4) *fy = w->y + title_bar_height() + 8;
}

/* Origin of one button's miniature, inside the flyout. */
static void button_box(int index, int fx, int fy, int *bx, int *by) {
    int bw, bh;
    win11_snap_button_size(&bw, &bh);
    *bx = fx + index * (bw + SNAP_GAP);
    *by = fy;
}

/* Draw one layout's miniature: the real arrangement of its zones, not a generic
 * icon.  This is what makes the picker recognisably Windows 11. */
static void draw_layout_mini(int bx, int by, int layout,
                             uint32_t face, uint32_t accent) {
    int ix = bx + SNAP_PAD, iy = by + SNAP_PAD;

    vbe_fill_rect_rounded(bx, by, SNAP_INNER_W + 2 * SNAP_PAD,
                          SNAP_INNER_H + 2 * SNAP_PAD, 6, face);
    vbe_rect_rounded(bx, by, SNAP_INNER_W + 2 * SNAP_PAD,
                     SNAP_INNER_H + 2 * SNAP_PAD, 6, accent);

    int zones = wubu_snap_layout_zones(layout);
    for (int z = 0; z < zones; z++) {
        float nx, ny, nw, nh;
        wubu_snap_layout_rect(layout, z, &nx, &ny, &nw, &nh);

        int zx = ix + (int)(nx * SNAP_INNER_W);
        int zy = iy + (int)(ny * SNAP_INNER_H);
        int zw = (int)(nw * SNAP_INNER_W);
        int zh = (int)(nh * SNAP_INNER_H);

        /* Fill the region, then separate it from its neighbours so a three-column
         * layout reads as three columns rather than one block. */
        vbe_fill_rect(zx, zy, zw, zh, accent);
        vbe_rect(zx, zy, zw, zh, face);
    }
}

void win11_snap_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_open) return;
    const WubuThemeColors *c = tc();

    int fx, fy, fw, fh;
    win11_snap_layout(fb_w, fb_h, &fx, &fy, &fw, &fh);

    vbe_fill_rect_rounded(fx, fy, fw, fh, 8, c->startmenu_bg);
    vbe_rect_rounded(fx, fy, fw, fh, 8, c->border_light);

    for (int i = 0; i < WUBU_SNAP_LAYOUTS; i++) {
        int bx, by;
        button_box(i, fx, fy, &bx, &by);
        draw_layout_mini(bx, by, i, c->btn_face, c->btn_hover);
    }
}

/* Which region of which layout was clicked?  Returns false when the press landed
 * outside every button (the caller closes the flyout in that case). */
static bool snap_hit_test(int x, int y, int fx, int fy, int *layout_out, int *zone_out) {
    int bw, bh;
    win11_snap_button_size(&bw, &bh);

    for (int i = 0; i < WUBU_SNAP_LAYOUTS; i++) {
        int bx = fx + i * (bw + SNAP_GAP);
        if (x < bx || x >= bx + bw || y < fy || y >= fy + bh) continue;

        *layout_out = i;

        /* Local position inside the miniature, as a fraction. */
        float px = (float)(x - (bx + SNAP_PAD)) / (float)SNAP_INNER_W;
        float py = (float)(y - (fy + SNAP_PAD)) / (float)SNAP_INNER_H;

        int zones = wubu_snap_layout_zones(i);
        for (int z = 0; z < zones; z++) {
            float nx, ny, nw, nh;
            wubu_snap_layout_rect(i, z, &nx, &ny, &nw, &nh);
            if (px >= nx && px < nx + nw && py >= ny && py < ny + nh) {
                *zone_out = z;
                return true;
            }
        }
        /* Inside the button but in a gap between regions: take the nearest zone by
         * clamping, so a click on a separator still does something sensible. */
        *zone_out = 0;
        return true;
    }
    return false;
}

/* ---- keyboard selection ------------------------------------------------------ */
static int g_sel_layout = 0;
static int g_sel_zone   = 0;

int win11_snap_selected_zone(void)   { return g_open ? g_sel_zone : -1; }
int win11_snap_selected_layout(void) { return g_open ? g_sel_layout : -1; }

bool win11_snap_key(uint32_t key)
{
    if (!g_open) return false;

    /* 1-6 choose the LAYOUT. Choosing a layout that has fewer zones than the current
     * selection keeps the selection in range -- clamping is a decision, and silently
     * pointing at a zone that no longer exists is how a keypress snaps into nothing. */
    switch (wubu_snap_key_select(&g_sel_layout, &g_sel_zone, key)) {
    case WUBU_SNAP_KEY_MOVED:
        return true;
    case WUBU_SNAP_KEY_APPLY:
        if (g_win_id >= 0 && g_dwm.windows[g_win_id].alive)
            wubu_snap_apply_layout_zone(&g_dwm.windows[g_win_id], g_sel_layout, g_sel_zone);
        g_open = false;
        return true;
    case WUBU_SNAP_KEY_CLOSE:
        g_open = false;
        return true;
    case WUBU_SNAP_KEY_IGNORED:
    default:
        return true;   /* the flyout owns the keyboard while it is open */
    }
}

bool win11_snap_click(int x, int y) {
    if (!g_open) return false;

    int fx, fy, fw, fh;
    win11_snap_layout(wm_screen_w(), wm_screen_h(), &fx, &fy, &fw, &fh);

    if (x < fx || x >= fx + fw || y < fy || y >= fy + fh) {
        g_open = false;
        return false;      /* the press closes the flyout; the WM still gets it */
    }

    int layout = 0, zone = 0;
    if (snap_hit_test(x, y, fx, fy, &layout, &zone)) {
        if (g_win_id >= 0 && g_dwm.windows[g_win_id].alive)
            wubu_snap_apply_layout_zone(&g_dwm.windows[g_win_id], layout, zone);
        g_open = false;
        return true;
    }

    g_open = false;
    return true;
}
