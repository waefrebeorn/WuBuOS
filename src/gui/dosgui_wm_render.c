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
#include "wubu_a11y.h"
#include "wubu_bonzi.h"
#include "wubu_bonzi_context.h"
#include "wubu_bonzi_activity.h"
#include "../kernel/vbe.h"

/* The /n/bonzi namespace mirror is an OPTIONAL meta-layer add-on: the GUI
 * must render whether or not the namespace bridge is linked. Declare weak
 * no-op defaults; the real wubu_ns_bonzi.o overrides them at link time when
 * the bridge is present. */
__attribute__((weak)) void wubu_ns_bonzi_set_enabled(int on){(void)on;}
__attribute__((weak)) void wubu_ns_bonzi_set_pos(int x,int y){(void)x;(void)y;}
__attribute__((weak)) void wubu_ns_bonzi_set_size(int w,int h){(void)w;(void)h;}
__attribute__((weak)) void wubu_ns_bonzi_set_agi(int o){(void)o;}
__attribute__((weak)) void wubu_ns_bonzi_set_bubble(const char*t){(void)t;}
__attribute__((weak)) int  wubu_ns_bonzi_sync(void){return -1;}
__attribute__((weak)) void wubu_bonzi_context_set_focus(const char*t,int x,int y,int w,int h){(void)t;(void)x;(void)y;(void)w;(void)h;}
__attribute__((weak)) void wubu_bonzi_context_set_window_count(int n){(void)n;}
__attribute__((weak)) void wubu_ns_bonzi_set_activity(const char*a){(void)a;}
__attribute__((weak)) wubu_bonzi_activity_t wubu_bonzi_get_activity(void){return WUBU_ACT_IDLE;}
__attribute__((weak)) const char *wubu_bonzi_activity_name(wubu_bonzi_activity_t a){return a==WUBU_ACT_THINKING?"thinking":a==WUBU_ACT_WORKING?"working":a==WUBU_ACT_ALERT?"alert":"idle";}

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

/* XP Luna push-button. (x,y,w,h) is the button rect; label is centered.
 * state: 0=normal, 1=hover, 2=pressed. is_default draws the blue
 * primary-action button (white text) used for OK/Apply on XP dialogs.
 * Luna buttons are flat + rounded with a soft gradient face and a 1px
 * outline — no 3D bevel. Win98 Classic keeps the raised silver button.
 * Returns nothing. */
void dosgui_draw_button(int x, int y, int w, int h, const char *label,
                        int state, bool is_default) {
    bool luna = theme()->Luna_start_button;
    int rad = theme()->rounded_buttons ? 3 : 0;

    if (luna) {
        uint32_t top, bot, edge, text;
        if (is_default) {
            top  = (state == 2) ? 0x2A5490 : (state == 1 ? 0x4A80C8 : 0x3A6EA5);
            bot  = (state == 2) ? 0x24486E : (state == 1 ? 0x3A6EA5 : 0x2E5A8A);
            edge = 0x1E3C5C;
            text = 0xFFFFFF;
        } else {
            top  = (state == 2) ? 0xD8D4CC : (state == 1 ? 0xFFFFFF : 0xF4F2EC);
            bot  = (state == 2) ? 0xC0BCB4 : (state == 1 ? 0xE8E4DC : 0xE0DCD4);
            edge = 0x9C9890;
            text = 0x000000;
        }
        int oy = (state == 2) ? 1 : 0;  /* pressed shifts content down 1px */
        vbe_fill_rect_rounded(x, y + oy, w, h, rad, bot);
        vbe_vgradient(x + 1, y + 1 + oy, w - 2, h - 2, top, bot);
        vbe_rect_rounded(x, y + oy, w, h, rad, edge);
    } else {
        uint32_t face = (state == 2) ? tc()->btn_face : tc()->btn_face;
        vbe_fill_rect(x, y, w, h, face);
        if (state == 2)
            vbe_3d_sunken_colors(x, y, w, h,
                                 tc()->border_light, tc()->border_face,
                                 tc()->border_dark, tc()->border_darkest);
        else
            vbe_3d_raised_colors(x, y, w, h,
                                 tc()->border_light, tc()->border_face,
                                 tc()->border_dark, tc()->border_darkest);
    }

    /* Centered label. */
    if (label && label[0]) {
        int tw = vbe_text_width(label, 1);
        int tx = x + (w - tw) / 2;
        int ty = y + (h - 8) / 2 + ((state == 2) ? 1 : 0);
        uint32_t text = 0x000000;
        if (luna && is_default) text = 0xFFFFFF;
        else if (luna) text = 0x000000;
        else text = tc()->btn_text;
        vbe_draw_text(tx, ty, label, text, 1);
    }
}

/* -- Full-screen render (the single entry point) ------------------- */

/* Frame-based millisecond clock (~60 Hz: 16ms per frame). Monotonic —
 * all double-click detection needs. Returns total elapsed ms. */
uint64_t dosgui_wm_ticks_ms(void) {
    extern DosGuiWM g_dwm;
    return g_dwm.render_frame_count * 16u;
}

/* XP Luna vertical scrollbar. (x,y,w,h) is the full track rect.
 * scroll = first visible row, visible = rows shown, total = total rows.
 * Under Luna the thumb is a rounded blue-gradient pill with a lighter
 * top edge and up/down arrow buttons; Win98 keeps the classic raised
 * silver track + 3D thumb. */
void dosgui_draw_vscrollbar(int x, int y, int w, int h,
                            int scroll, int visible, int total) {
    bool luna = theme()->Luna_start_button;
    int btn = w;  /* square arrow buttons */

    /* Track */
    if (luna) {
        vbe_fill_rect(x, y, w, h, 0xF1EFE8);
        vbe_vline(x, y, y + h - 1, 0xB8B4A8);
        vbe_vline(x + w - 1, y, y + h - 1, 0xFFFFFF);
    } else {
        vbe_fill_rect(x, y, w, h, tc()->btn_face);
        vbe_3d_sunken_colors(x, y, w, h,
                             tc()->border_light, tc()->border_face,
                             tc()->border_dark, tc()->border_darkest);
    }

    /* Up arrow button */
    if (luna) {
        vbe_fill_rect(x + 1, y + 1, w - 2, btn - 2, 0xE8F0FA);
        vbe_rect(x + 1, y + 1, w - 2, btn - 2, 0x7A9CC6);
    } else {
        vbe_fill_rect(x + 1, y + 1, w - 2, btn - 2, tc()->btn_face);
        vbe_3d_raised_colors(x + 1, y + 1, w - 2, btn - 2,
                             tc()->border_light, tc()->border_face,
                             tc()->border_dark, tc()->border_darkest);
    }
    {   /* up triangle */
        int cx = x + w / 2, by = y + btn / 2;
        for (int i = 0; i < 4; i++)
            vbe_hline(cx - i, cx + i, by - 2 + i, 0x000000);
    }

    /* Down arrow button */
    int dy = y + h - btn;
    if (luna) {
        vbe_fill_rect(x + 1, dy + 1, w - 2, btn - 2, 0xE8F0FA);
        vbe_rect(x + 1, dy + 1, w - 2, btn - 2, 0x7A9CC6);
    } else {
        vbe_fill_rect(x + 1, dy + 1, w - 2, btn - 2, tc()->btn_face);
        vbe_3d_raised_colors(x + 1, dy + 1, w - 2, btn - 2,
                             tc()->border_light, tc()->border_face,
                             tc()->border_dark, tc()->border_darkest);
    }
    {   /* down triangle */
        int cx = x + w / 2, by = dy + btn / 2;
        for (int i = 0; i < 4; i++)
            vbe_hline(cx - i, cx + i, by + 2 - i, 0x000000);
    }

    /* Thumb */
    int track_top = y + btn;
    int track_h = h - 2 * btn;
    if (track_h <= 0) return;
    int thumb_h = track_h * visible / (total > 0 ? total : 1);
    if (thumb_h < 8) thumb_h = 8;
    if (thumb_h > track_h) thumb_h = track_h;
    int thumb_y = track_top;
    if (total > visible)
        thumb_y = track_top + (track_h - thumb_h) * scroll / (total - visible);

    if (luna) {
        /* XP blue-gradient thumb: lighter top -> darker bottom. */
        vbe_fill_rect_rounded(x + 1, thumb_y, w - 2, thumb_h, 3, 0x3A6EA5);
        vbe_vgradient(x + 2, thumb_y + 2, w - 4, thumb_h - 4,
                      0x8FB8E8, 0x3A6EA5);
        vbe_rect_rounded(x + 1, thumb_y, w - 2, thumb_h, 3, 0x24486E);
    } else {
        vbe_fill_rect(x + 1, thumb_y, w - 2, thumb_h, tc()->btn_face);
        vbe_3d_raised_colors(x + 1, thumb_y, w - 2, thumb_h,
                             tc()->border_light, tc()->border_face,
                             tc()->border_dark, tc()->border_darkest);
    }
}

void dosgui_wm_render(uint32_t *fb, int fb_w, int fb_h) {
    /* Advance the desktop companion animation clock (~60 Hz). */
    g_dwm.render_frame_count++;
    wubu_bonzi_tick(16);
    /* Meta-layer: mirror the live companion state into /n/bonzi so the shell
     * (and any agent) can read what the AGI face IS -- `cat /n/bonzi/*`.
     * sync() is a safe no-op (-1) when the namespace root isn't set. */
    wubu_ns_bonzi_set_enabled(wubu_bonzi_is_enabled());
    wubu_ns_bonzi_set_pos(wubu_bonzi_x(), wubu_bonzi_y());
    wubu_ns_bonzi_set_size(wubu_bonzi_w(), wubu_bonzi_h());
    wubu_ns_bonzi_set_activity(wubu_bonzi_activity_name(wubu_bonzi_get_activity()));
    wubu_ns_bonzi_sync();
    /* Ambient context (Track B1): feed the live focused window so the
     * companion sees what the user is attending to ("context is the
     * prompt"). Weak-linked like the ns sync. */
    {
        DosGuiWindow *fw = dosgui_wm_get_focused();
        wubu_bonzi_context_set_window_count(dosgui_wm_window_count());
        if (fw)
            wubu_bonzi_context_set_focus(fw->title, fw->x, fw->y, fw->w, fw->h);
    }
    /* Advance any active human-typing session (keystrokes land with
     * human rhythm, not all at once). */
    dosgui_wm_typer_tick(16);

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

    /* Rubber-band drag-select lasso (Classic Mac / Win98 lesson). */
    if (g_dwm.lasso_active) {
        int x0 = g_dwm.lasso_x0, y0 = g_dwm.lasso_y0;
        int x1 = g_dwm.lasso_x1, y1 = g_dwm.lasso_y1;
        if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
        int w = x1 - x0, h = y1 - y0;
        /* Translucent navy fill + 1px solid border (Win98 rubber band). */
        vbe_blend_rect(x0, y0, w, h, tc()->select_bg, 60);
        vbe_rect(x0, y0, w, h, tc()->select_bg);
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

    /* Accessibility cluster: drawn over the focused window (top of z-order),
     * before the taskbar so it never paints under it. Skipped for minimized
     * windows (nothing to grab). */
    if (wubu_a11y_is_enabled() && g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->alive && !(w->flags & DOSGUI_WIN_MINIMIZED))
            wubu_a11y_draw(w, fb, fb_w, fb_h);
    }

    /* Desktop mascot (Bonzi Buddy): drawn above windows, below taskbar.
     * Balloon Help (System 7 lesson): when the cursor hovers a desktop
     * icon, the buddy's bubble becomes a tip about that icon; otherwise
     * it returns to the greeting. */
    if (wubu_bonzi_is_enabled()) {
        int hov = dosgui_icon_hit_test(g_dwm.mouse_x, g_dwm.mouse_y);
        /* Only tip about an icon when NO window covers it (the cursor must
         * actually be on the desktop, not on a window above the icon). */
        if (hov >= 0 && hit_test(g_dwm.mouse_x, g_dwm.mouse_y) < 0) {
            DosGuiIcon *hic = &g_dwm.icons[hov];
            wubu_bonzi_set_bubble(hic->name, "Click to open");
        } else {
            wubu_bonzi_set_bubble("Hi! I'm WuBu Buddy!",
                                  "Click me for the AGI!");
        }
        wubu_bonzi_draw(fb, fb_w, fb_h);
    }

    dosgui_taskbar_render(fb, fb_w, fb_h);
    dosgui_notif_center_render(fb, fb_w, fb_h);

    /* Clock popup menu (Win98 parity): the clock's own calendar panel. */
    if (g_dwm.clock_menu_open)
        dosgui_clock_menu_render(fb, fb_w, fb_h);

    /* Human-lag cursor: the drawn cursor eases toward the input target
     * (fast exponential approach — ~7 frames to converge, so motion reads
     * as a quick human swipe, not an instant teleport).  Input LOGIC still
     * uses g_dwm.mouse_x/y so clicks land exactly where intended. */
    g_dwm.cursor_x += (int)((g_dwm.mouse_x - g_dwm.cursor_x) * 0.30f);
    g_dwm.cursor_y += (int)((g_dwm.mouse_y - g_dwm.cursor_y) * 0.30f);
    if (g_dwm.cursor_x == 0 && g_dwm.cursor_y == 0) {
        g_dwm.cursor_x = g_dwm.mouse_x;
        g_dwm.cursor_y = g_dwm.mouse_y;
    }
    vbe_draw_cursor(g_dwm.cursor_x, g_dwm.cursor_y);
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