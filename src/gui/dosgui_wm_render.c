/*
 * dosgui_wm_render.c -- WuBuOS WM rendering (single module)
 *
 * ONE entry point: dosgui_wm_render(fb, w, h). It draws the desktop
 * background, icons, every live window (chrome + on_draw content), the
 * taskbar, the notification center and the cursor.
 *
 * The previous design had THREE render entry points (dosgui_wm_render,
 * dosgui_wm_render_desktop, dosgui_desktop_render) plus a dead
 * dirty-region tracking system that nothing consumed (the hosted loop
 * redraws every frame). All of that is collapsed here: the two
 * *_desktop_render names remain as 1-line wrappers for back-compat
 * callers (hosted.c, tests), and the dirty tracking is gone.
 *
 * Self-contained: depends only on the public WM API (dosgui_wm.h)
 * and the shared internal state/helpers (dosgui_wm_internal.h).
 */

#include "dosgui_wm.h"
#include "dosgui_wm_internal.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* -- Single window draw ------------------------------------------- */

static void draw_window(int idx, uint32_t *fb, int fb_w, int fb_h) {
    DosGuiWindow *w = &g_dwm.windows[idx];
    if (!w->alive) return;

    /* Use standardized chrome to draw window frame + title bar + buttons.
     * Returns content rect for app to draw within. */
    ChromeContentRect content = dosgui_chrome_draw_window(&g_dwm.windows[idx], fb, fb_w, fb_h);

    /* Content region: clip and call app's on_draw if present */
    if (w->on_draw) {
        vbe_set_clip(content.x, content.y, content.w, content.h);
        w->on_draw(w, fb, fb_w, fb_h);
        vbe_reset_clip();
    }
}

/* -- Full-screen render (the single entry point) ------------------- */

void dosgui_wm_render(uint32_t *fb, int fb_w, int fb_h) {
    draw_desktop_bg(fb_w, fb_h);

    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *icon = &g_dwm.icons[i];
        if (!icon->alive) continue;

        /* Selection highlight (XP active-selection: navy box + focus rect). */
        if (icon->selected)
            dosgui_wm_draw_icon_selection(icon->x, icon->y);

        /* Recognizable glyph instead of a flat colored box. Fall back to the
         * icon_color box only if the slot is somehow empty. */
        uint32_t accent = icon->icon_color ? icon->icon_color : tc()->win_title_active;
        dosgui_wm_draw_icon_glyph(icon->type, icon->x, icon->y,
                                  /* face */ tc()->btn_face,
                                  /* light */ tc()->border_light,
                                  /* dark */ tc()->border_dark,
                                  /* accent */ accent);

        int label_y = icon->y + DOSGUI_ICON_SIZE + 2;
        int max_label_w = DOSGUI_ICON_SIZE + 4;
        int text_w = vbe_text_width(icon->name, 1);

        if (label_y + 8 <= fb_h) {
            uint32_t txt_col = icon->selected ? tc()->win_title_text : tc()->icon_text;
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
                /* Shadow then text; on selection also draw a navy plate behind. */
                if (icon->selected) {
                    int sw = vbe_text_width(truncated, 1);
                    vbe_fill_rect(icon->x - 1, label_y - 1, sw + 2, 10, tc()->select_bg);
                }
                vbe_draw_text(icon->x + 1, label_y, truncated, tc()->icon_text_shadow, 1);
                vbe_draw_text(icon->x, label_y - 1, truncated, txt_col, 1);
            } else {
                if (icon->selected) {
                    int sw = vbe_text_width(icon->name, 1);
                    vbe_fill_rect(icon->x - 1, label_y - 1, sw + 2, 10, tc()->select_bg);
                }
                vbe_draw_text(icon->x + 1, label_y, icon->name, tc()->icon_text_shadow, 1);
                vbe_draw_text(icon->x, label_y - 1, icon->name, txt_col, 1);
            }
        }
    }

    for (int j = 0; j < g_dwm.nz; j++) {
        int idx = g_dwm.zorder[j];
        DosGuiWindow *w = &g_dwm.windows[idx];
        if (w->alive && w->desktop == g_dwm.current_desktop && !(w->flags & DOSGUI_WIN_MINIMIZED)) {
            /* Confine the entire window frame to the area ABOVE the taskbar.
             * Without this, a window whose bottom extends into the taskbar
             * band would paint its title bar / close "X" over the taskbar,
             * producing the "FILE MANAGER CLOSE" text-merge artifact. */
            int task_h = taskbar_height_dynamic();
            vbe_set_clip(0, 0, fb_w, fb_h - task_h);
            draw_window(idx, fb, fb_w, fb_h);
            vbe_reset_clip();
        }
    }

    dosgui_taskbar_render(fb, fb_w, fb_h);
    dosgui_notif_center_render(fb, fb_w, fb_h);
    vbe_draw_cursor(g_dwm.mouse_x, g_dwm.mouse_y);
}

/* -- Back-compat wrappers (kept for hosted.c / tests) ---------- */

void dosgui_wm_render_desktop(uint32_t *fb, int fb_w, int fb_h) {
    dosgui_wm_render(fb, fb_w, fb_h);
}

/* -- Invalidations (bookkeeping for a future dirty-region renderer) --
 * The current hosted loop redraws every frame, so these are not
 * consumed by the renderer yet, but the API is real and tested. */

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