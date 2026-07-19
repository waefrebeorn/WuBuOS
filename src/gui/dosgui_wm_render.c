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
    bool active = (idx == g_dwm.focused_id);

    const int rad = theme_radius();
    const int tbh = title_bar_height();
    const int bw  = border_width();

    /* XP Luna soft drop-shadow: a blended offset rect drawn BEFORE the window
     * body so the window paints over it. Win98 (win_shadow == 0) gets none. */
    if (theme()->Luna_start_button && tc()->win_shadow != 0) {
        int off = 4;
        vbe_blend_rect(w->x + off + 1, w->y + off + 1, w->w, w->h,
                       tc()->win_shadow, 40);   /* soft outer */
        vbe_blend_rect(w->x + off, w->y + off, w->w, w->h,
                       tc()->win_shadow, 90);   /* inner */
    }

    vbe_fill_rect_rounded(w->x, w->y, w->w, w->h, rad, tc()->win_face);
    if (rad > 0) vbe_rect_rounded(w->x, w->y, w->w, w->h, rad, tc()->border_dark);
    else            vbe_rect(w->x, w->y, w->w, w->h, tc()->border_dark);

    /* Title bar */
    if (theme()->gradient_title) {
        int gx = w->x, gy = w->y, gw = w->w, gh = tbh;
        if (active) vbe_hgradient(gx, gy, gw, gh,
                              theme()->title_gradient.color_start, theme()->title_gradient.color_end);
        else       vbe_hgradient(gx, gy, gw, gh,
                              theme()->title_gradient_ina.color_start, theme()->title_gradient_ina.color_end);
        /* Restore the rounded top corners: the gradient filled the whole
         * title band, so paint the corner wedges back to the window face. */
        if (rad > 0) {
            vbe_fill_rect_rounded(w->x, w->y, w->w, w->h, rad, tc()->win_face);
            if (active) vbe_hgradient(w->x, w->y, w->w, tbh - rad,
                                  theme()->title_gradient.color_start, theme()->title_gradient.color_end);
            else       vbe_hgradient(w->x, w->y, w->w, tbh - rad,
                                  theme()->title_gradient_ina.color_start, theme()->title_gradient_ina.color_end);
            vbe_rect_rounded(w->x, w->y, w->w, w->h, rad, tc()->border_dark);
        }
    } else {
        vbe_title_bar(w->x + rad, w->y + rad, w->w - 2*rad, tbh - rad, active);
    }

    /* Title text */
    uint32_t title_color = active ? tc()->win_title_text : tc()->win_title_text_ina;
    int text_x = w->x + 8;
    int text_y = w->y + rad + (tbh - rad - 8) / 2;
    vbe_draw_text(text_x, text_y, w->title, title_color, 1);

    /* Close button */
    int close_x = w->x + w->w - rad - 18;
    int close_y = w->y + rad + 2;
    vbe_fill_rect_rounded(close_x, close_y, 14, 12, 2, active ? tc()->border_darkest : tc()->btn_face);
    vbe_rect_rounded(close_x, close_y, 14, 12, 2, tc()->border_dark);
    vbe_draw_text(close_x + 5, close_y + 2, "X", active ? 0x00FFFFFF : 0x00808080, 1);

    /* Maximize / minimize buttons (Luna style) */
    if (theme()->Luna_start_button) {
        int max_x = close_x - 20;
        vbe_fill_rect_rounded(max_x, close_y, 14, 12, 2, active ? tc()->border_face : tc()->btn_face);
        vbe_rect_rounded(max_x, close_y, 14, 12, 2, tc()->border_dark);
        vbe_draw_text(max_x + 4, close_y + 2, "[", active ? 0x00FFFFFF : 0x00808080, 1);

        int min_x = close_x - 40;
        vbe_fill_rect_rounded(min_x, close_y, 14, 12, 2, active ? tc()->border_face : tc()->btn_face);
        vbe_rect_rounded(min_x, close_y, 14, 12, 2, tc()->border_dark);
        vbe_draw_text(min_x + 5, close_y + 2, "_", active ? 0x00FFFFFF : 0x00808080, 1);
    }

    /* Content region */
    int cx = w->x + bw;
    int cy = w->y + tbh;
    int cw = w->w - 2 * bw;
    int ch = w->h - tbh - bw;

    vbe_fill_rect_rounded(cx, cy, cw, ch, rad, tc()->win_face);
    if (rad > 0) vbe_3d_sunken_rounded_colors(cx - 1, cy - 1, cw + 2, ch + 2, rad + 1,
                                              tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);
    else            vbe_3d_sunken_colors(cx - 1, cy - 1, cw + 2, ch + 2,
                                        tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);

    if (w->on_draw) {
        /* Real framebuffer + dims so apps render into their content region.
         * Apps using the global vbe_* backbuffer ignore these and still work. */
        w->on_draw(w, fb, fb_w, fb_h);
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
        if (w->alive && w->desktop == g_dwm.current_desktop && !(w->flags & DOSGUI_WIN_MINIMIZED))
            draw_window(idx, fb, fb_w, fb_h);
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
