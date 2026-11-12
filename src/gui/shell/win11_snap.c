/*
 * win11_snap.c -- Win+Z snap-layout flyout (Windows 11 parity).
 *
 * One concern: the layout picker.  Geometry lives in win11_snap_layout()
 * so the renderer and the pointer path cannot disagree about a button.
 * The zone ids are the ones snap_window_to_zone() already understands.
 *
 * C11, opaque API (win11_snap.h), no god headers.
 */

#include "win11_snap.h"
#include "../wm_internal.h"
#include "../wm.h"
#include "../kernel/vbe.h"
#include "../wubu_theme.h"

#include <string.h>

#define SNAP_BUTTONS 6
#define SNAP_BTN_W   52
#define SNAP_BTN_H   34
#define SNAP_GAP     6
#define SNAP_PAD     8

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

/* Flyout geometry: centred over the focused window's title bar, flipped
 * below it when there is no room above. */
static void win11_snap_layout(int fb_w, int fb_h, int *fx, int *fy,
                              int *fw, int *fh) {
    (void)fb_w; (void)fb_h;
    *fw = SNAP_BUTTONS * SNAP_BTN_W + (SNAP_BUTTONS - 1) * SNAP_GAP + 2 * SNAP_PAD;
    *fh = SNAP_BTN_H + 2 * SNAP_PAD;

    DosGuiWindow *w = (g_win_id >= 0) ? &g_dwm.windows[g_win_id] : NULL;
    if (!w) { *fx = 0; *fy = 0; return; }

    *fx = w->x + (w->w - *fw) / 2;
    *fy = w->y - *fh - 8;
    if (*fy < 4) *fy = w->y + title_bar_height() + 8;
}

static void draw_zone_glyph(int x, int y, int w, int h, int zone,
                            uint32_t face, uint32_t accent) {
    /* A miniature of the window with the chosen zone filled in. */
    vbe_rect(x, y, w, h, accent);
    int hx = x + w / 2, hy = y + h / 2;
    switch (zone) {
        case 0: vbe_fill_rect(x, y, w / 2, h, accent); break;
        case 1: vbe_fill_rect(hx, y, w - w / 2, h, accent); break;
        case 2: vbe_fill_rect(x, y, w / 2, h / 2, accent); break;
        case 3: vbe_fill_rect(hx, y, w - w / 2, h / 2, accent); break;
        case 4: vbe_fill_rect(x, hy, w / 2, h - h / 2, accent); break;
        case 5: vbe_fill_rect(hx, hy, w - w / 2, h - h / 2, accent); break;
        default: break;
    }
    (void)face;
}

void win11_snap_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_open) return;
    const WubuThemeColors *c = tc();

    int fx, fy, fw, fh;
    win11_snap_layout(fb_w, fb_h, &fx, &fy, &fw, &fh);

    vbe_fill_rect_rounded(fx, fy, fw, fh, 8, c->startmenu_bg);
    vbe_rect_rounded(fx, fy, fw, fh, 8, c->border_light);

    for (int i = 0; i < SNAP_BUTTONS; i++) {
        int bx = fx + SNAP_PAD + i * (SNAP_BTN_W + SNAP_GAP);
        int by = fy + SNAP_PAD;
        vbe_fill_rect_rounded(bx, by, SNAP_BTN_W, SNAP_BTN_H, 6, c->btn_face);
        vbe_rect_rounded(bx, by, SNAP_BTN_W, SNAP_BTN_H, 6, c->border_light);
        draw_zone_glyph(bx + 8, by + 7, SNAP_BTN_W - 16, SNAP_BTN_H - 14, i,
                        c->btn_face, c->btn_hover);
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
    for (int i = 0; i < SNAP_BUTTONS; i++) {
        int bx = fx + SNAP_PAD + i * (SNAP_BTN_W + SNAP_GAP);
        int by = fy + SNAP_PAD;
        if (x >= bx && x < bx + SNAP_BTN_W && y >= by && y < by + SNAP_BTN_H) {
            if (g_win_id >= 0 && g_dwm.windows[g_win_id].alive)
                snap_window_to_zone(&g_dwm.windows[g_win_id], i);
            g_open = false;
            return true;
        }
    }
    g_open = false;
    return true;
}
