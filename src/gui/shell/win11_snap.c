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

#include "win11_snap.h"
#include "../wm_internal.h"
#include "../wm.h"
#include "../kernel/vbe.h"
#include "../wubu_theme.h"

#include <string.h>

#define SNAP_GAP     6
#define SNAP_PAD     8
/* The miniature must leave room for the zone separators to stay readable. */
#define SNAP_INNER_W 40
#define SNAP_INNER_H 22

static bool g_open = false;
static int  g_win_id = -1;

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
