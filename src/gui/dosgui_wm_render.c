/*
 * dosgui_wm_render.c -- WuBuOS WM full-screen render + dirty-tracking
 *
 * Extracted from dosgui_wm.c (monolith split, C11, opaque-safe).
 * Self-contained: depends only on the public WM API (dosgui_wm.h) and the
 * internal shared state/helpers (dosgui_wm_internal.h).  No god headers.
 */

#include "dosgui_wm.h"
#include "dosgui_wm_internal.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* -- Full Render ------------------------------------------------- */

void dosgui_wm_render(uint32_t *fb, int fb_w, int fb_h) {
    dosgui_wm_render_desktop(fb, fb_w, fb_h);
}

void dosgui_wm_render_desktop(uint32_t *fb, int fb_w, int fb_h) {
    draw_desktop_bg(fb_w, fb_h);

    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *icon = &g_dwm.icons[i];
        vbe_fill_rect(icon->x, icon->y, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE, tc()->icon_bg);
        vbe_rect(icon->x, icon->y, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE, tc()->icon_border);
        
        /* Draw icon label with bounds checking and truncation */
        int label_y = icon->y + DOSGUI_ICON_SIZE + 2;
        int max_label_w = DOSGUI_ICON_SIZE + 4; /* Slightly wider than icon */
        int text_w = vbe_text_width(icon->name, 1);
        
        /* Check if label would go off-screen */
        if (label_y + 8 <= fb_h) {
            /* Truncate text with ellipsis if too wide */
            if (text_w > max_label_w) {
                char truncated[32];
                int len = strlen(icon->name);
                strncpy(truncated, icon->name, len);
                truncated[len] = '\0';
                while (len > 0 && vbe_text_width(truncated, 1) > max_label_w) {
                    len--;
                    truncated[len] = '\0';
                }
                if (len > 0) {
                    strncpy(truncated + len, "...", 3);
                    truncated[len + 3] = '\0';
                } else {
                    strcpy(truncated, "...");
                }
                vbe_draw_text(icon->x + 1, label_y, truncated, tc()->icon_text_shadow, 1);
                vbe_draw_text(icon->x, label_y - 1, truncated, tc()->icon_text, 1);
            } else {
                vbe_draw_text(icon->x + 1, label_y, icon->name, tc()->icon_text_shadow, 1);
                vbe_draw_text(icon->x, label_y - 1, icon->name, tc()->icon_text, 1);
            }
        }
    }

    for (int j = 0; j < g_dwm.nz; j++) {
        int idx = g_dwm.zorder[j];
        DosGuiWindow *w = &g_dwm.windows[idx];
        if (w->alive && w->desktop == g_dwm.current_desktop && !(w->flags & DOSGUI_WIN_MINIMIZED))
            draw_window(idx, fb, fb_w, fb_h);
    }

    dosgui_taskbar_render(fb, fb_w, fb_h);

    /* Render notification center if open (on top of everything) */
    dosgui_notif_center_render(fb, fb_w, fb_h);

    vbe_draw_cursor(g_dwm.mouse_x, g_dwm.mouse_y);
}

/* ══════════════════════════════════════════════════════════════════
 * Invalidation tracking (legacy wm_invalidate / wm_invalidate_all)
 *
 * The hosted render loop (wubu_syscall.c) calls dosgui_wm_render() every
 * frame, so the whole screen is redrawn unconditionally.  These helpers
 * still do *real* bookkeeping: they record which windows were explicitly
 * invalidated so a smarter (dirty-region) renderer can later skip clean
 * windows.  dosgui_wm_poll_dirty() drains the queue; -1 means "redraw all".
 * ══════════════════════════════════════════════════════════════════ */

#define DOSGUI_DIRTY_MAX 64
static int    g_dirty_ids[DOSGUI_DIRTY_MAX];
static int    g_dirty_count = 0;
static bool   g_dirty_all   = false;

void dosgui_wm_invalidate(DosGuiWindow *win) {
    if (!win) { g_dirty_all = true; return; }
    if (g_dirty_count < DOSGUI_DIRTY_MAX)
        g_dirty_ids[g_dirty_count++] = win->id;
}

void dosgui_wm_invalidate_all(void) {
    g_dirty_all = true;
}

/* Drain one invalidated window id.  Returns true and writes *out_id:
 *   *out_id >= 0  -> that window was invalidated
 *   *out_id == -1 -> a full-redraw was requested (wm_invalidate_all)
 * Returns false when the queue is empty. */
bool dosgui_wm_poll_dirty(int *out_id) {
    if (g_dirty_all) {
        g_dirty_all = false;
        g_dirty_count = 0;
        if (out_id) *out_id = -1;
        return true;
    }
    if (g_dirty_count > 0) {
        if (out_id) *out_id = g_dirty_ids[--g_dirty_count];
        return true;
    }
    return false;
}

int dosgui_wm_dirty_count(void) {
    return g_dirty_all ? -1 : g_dirty_count;
}
