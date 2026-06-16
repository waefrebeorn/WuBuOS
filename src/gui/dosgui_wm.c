/*
 * dosgui_wm.c  --  WuBuOS DosGui Window Manager Implementation
 *
 * Cell 400: Fable Windowing Agent — THEMED EDITION.
 * Ports ZealOS/WuBuDos bare-metal window management into WuBuOS.
 * Based on Mythos Fable's wm.c (filipvabrousek/osdev).
 *
 * Features:
 *   - Draggable windows with title bars (XP gradient or Win98 flat)
 *   - Z-order + focus management
 *   - Taskbar with window buttons, clock, Start button (Luna orb on XP)
 *   - Close box (red X on Win98, themed on XP)
 *   - Software mouse cursor (18-row arrow)
 *   - Desktop icons with click handlers
 *   - Drop shadow under windows
 *   - FULL THEME ENGINE INTEGRATION: Win98 Classic, XP Luna Blue, XP Media Orange, WuBu Green
 *   - Rounded buttons on XP themes, square on Win98
 *   - Gradient title bars on XP themes
 */

#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* -- Global State ------------------------------------------------- */

typedef struct {
    int             screen_w, screen_h;
    DosGuiWindow    windows[DOSGUI_MAX_WINDOWS];
    int             next_id;
    int             focused_id;

    /* Z-order: indices into windows[], bottom..top */
    int             zorder[DOSGUI_MAX_WINDOWS];
    int             nz;

    /* Drag state */
    int             drag_id;
    int             drag_ox, drag_oy;

    /* Desktop icons */
    DosGuiIcon       icons[DOSGUI_MAX_ICONS];
    int             icon_count;

    /* Mouse state */
    int             mouse_x, mouse_y;
    int             ticks;
} DosGuiWM;

static DosGuiWM g_dwm = {0};

/* -- Forward declarations ----------------------------------------- */

static void raise_win(int i);
static void close_win(int i);
static int  hit_test(int x, int y);
static void draw_window(int idx);
static void draw_desktop_bg(int fb_w, int fb_h);
static const WubuThemeColors *tc(void);
static const WubuTheme *theme(void);
static int title_bar_height(void);
static int taskbar_height_dynamic(void);
static int border_width(void);
static int theme_radius(void);

/* -- Window List Management --------------------------------------- */

static void raise_win(int i) {
    int j = 0;
    while (j < g_dwm.nz && g_dwm.zorder[j] != i) j++;
    if (j == g_dwm.nz) return;
    for (; j < g_dwm.nz - 1; j++)
        g_dwm.zorder[j] = g_dwm.zorder[j + 1];
    g_dwm.zorder[g_dwm.nz - 1] = i;
}

static int spawn_window(int x, int y, int w, int h, const char *title) {
    DosGuiWindow *win = NULL;
    int i;
    for (i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (!g_dwm.windows[i].alive) { win = &g_dwm.windows[i]; break; }
    }
    if (!win) return -1;

    memset(win, 0, sizeof(*win));
    win->id = g_dwm.next_id++;
    win->flags = DOSGUI_WIN_NORMAL;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->alive = true;
    strncpy(win->title, title ? title : "Window", sizeof(win->title) - 1);

    g_dwm.zorder[g_dwm.nz++] = i;
    g_dwm.focused_id = i;
    return i;
}

static void close_win(int i) {
    if (i < 0 || i >= DOSGUI_MAX_WINDOWS) return;
    g_dwm.windows[i].alive = false;
    g_dwm.windows[i].flags = DOSGUI_WIN_UNUSED;
    int j = 0;
    while (j < g_dwm.nz && g_dwm.zorder[j] != i) j++;
    for (; j < g_dwm.nz - 1; j++)
        g_dwm.zorder[j] = g_dwm.zorder[j + 1];
    if (j < g_dwm.nz) g_dwm.nz--;
    if (g_dwm.drag_id == i) g_dwm.drag_id = -1;
    if (g_dwm.focused_id == i)
        g_dwm.focused_id = g_dwm.nz ? g_dwm.zorder[g_dwm.nz - 1] : -1;
}

static int hit_test(int x, int y) {
    for (int j = g_dwm.nz - 1; j >= 0; j--) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
        if (w->alive && x >= w->x && x < w->x + w->w &&
            y >= w->y && y < w->y + w->h)
            return g_dwm.zorder[j];
    }
    return -1;
}

/* -- Theme Helpers ------------------------------------------------ */

static const WubuThemeColors *tc(void) { return wubu_theme_colors(); }
static const WubuTheme *theme(void) { return wubu_theme_get(); }
static int title_bar_height(void) { return theme()->Luna_start_button ? 24 : DOSGUI_TITLE_H; }
static int taskbar_height_dynamic(void) { return theme()->Luna_start_button ? 30 : DOSGUI_TASK_H; }
static int border_width(void) { return theme()->rounded_buttons ? 3 : DOSGUI_BORDER; }
static int theme_radius(void) { return theme()->rounded_buttons ? 4 : 0; }

/* ================================================================
 * RENDERING — Themed Window Chrome
 * ================================================================ */

static void draw_desktop_bg(int fb_w, int fb_h) {
    int task_h = taskbar_height_dynamic();
    (void)vbe_state();
    vbe_fill_rect(0, 0, fb_w, fb_h - task_h, tc()->desktop_bg);
}

static void draw_window(int idx) {
    DosGuiWindow *w = &g_dwm.windows[idx];
    if (!w->alive) return;
    bool active = (idx == g_dwm.focused_id);

    const int rad = theme_radius();
    const int tbh = title_bar_height();
    const int bw = border_width();

    /* Drop shadow (Win98 style always, XP subtle) */
    if (!theme()->Luna_start_button || true) {
        vbe_shade_rect(w->x + 4, w->y + 4, w->w, w->h);
    }

    /* Window face */
    vbe_fill_rect_rounded(w->x, w->y, w->w, w->h, rad, tc()->win_face);
    if (rad > 0) vbe_rect_rounded(w->x, w->y, w->w, w->h, rad, tc()->border_dark);
    else vbe_rect(w->x, w->y, w->w, w->h, tc()->border_dark);

    /* Title bar */
    if (theme()->gradient_title) {
        if (active) {
            vbe_hgradient(w->x + rad, w->y + rad, w->w - 2*rad, tbh - rad,
                          theme()->title_gradient.color_start,
                          theme()->title_gradient.color_end);
        } else {
            vbe_hgradient(w->x + rad, w->y + rad, w->w - 2*rad, tbh - rad,
                          theme()->title_gradient_ina.color_start,
                          theme()->title_gradient_ina.color_end);
        }
    } else {
        /* Win98 flat titlebar using vbe_title_bar */
        vbe_title_bar(w->x + rad, w->y + rad, w->w - 2*rad, tbh - rad, active);
    }

    /* Title text */
    uint32_t title_color = active ? tc()->win_title_text : tc()->win_title_text_ina;
    int text_x = w->x + 8;
    int text_y = w->y + rad + (tbh - rad - 8) / 2;
    vbe_draw_text(text_x, text_y, w->title, title_color, 1);

    /* Close box - top right */
    int close_x = w->x + w->w - rad - 18;
    int close_y = w->y + rad + 2;
    vbe_fill_rect_rounded(close_x, close_y, 14, 12, 2, active ? tc()->border_darkest : tc()->btn_face);
    vbe_rect_rounded(close_x, close_y, 14, 12, 2, tc()->border_dark);
    vbe_draw_text(close_x + 5, close_y + 2, "X", active ? 0xFFFFFF : 0x808080, 1);

    /* Maximize box (XP) */
    if (theme()->Luna_start_button) {
        int max_x = close_x - 20;
        vbe_fill_rect_rounded(max_x, close_y, 14, 12, 2, active ? tc()->border_face : tc()->btn_face);
        vbe_rect_rounded(max_x, close_y, 14, 12, 2, tc()->border_dark);
        vbe_draw_text(max_x + 4, close_y + 2, "[ ]", active ? 0xFFFFFF : 0x808080, 1);
    }

    /* Minimize box */
    if (theme()->Luna_start_button) {
        int min_x = close_x - 40;
        vbe_fill_rect_rounded(min_x, close_y, 14, 12, 2, active ? tc()->border_face : tc()->btn_face);
        vbe_rect_rounded(min_x, close_y, 14, 12, 2, tc()->border_dark);
        vbe_draw_text(min_x + 5, close_y + 2, "_", active ? 0xFFFFFF : 0x808080, 1);
    }

    /* Content area */
    int cx = w->x + bw;
    int cy = w->y + tbh;
    int cw = w->w - 2 * bw;
    int ch = w->h - tbh - bw;

    /* Content background */
    vbe_fill_rect_rounded(cx, cy, cw, ch, rad, tc()->win_face);

    /* Sunken border around content */
    if (rad > 0) vbe_3d_sunken_rounded(cx - 1, cy - 1, cw + 2, ch + 2, rad + 1);
    else vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);

    /* Render window content via callback */
    if (w->on_draw) {
        w->on_draw(w, NULL, cw, ch);
    }
}

/* ================================================================
 * PUBLIC API
 * ================================================================ */

int dosgui_wm_init(int screen_w, int screen_h) {
    memset(&g_dwm, 0, sizeof(g_dwm));
    g_dwm.screen_w = screen_w;
    g_dwm.screen_h = screen_h;
    g_dwm.focused_id = -1;
    g_dwm.drag_id = -1;
    return 0;
}

void dosgui_wm_shutdown(void) {
    memset(&g_dwm, 0, sizeof(g_dwm));
}

DosGuiWindow *dosgui_wm_create(int x, int y, int w, int h,
                                const char *title) {
    int i = spawn_window(x, y, w, h, title);
    if (i < 0) return NULL;
    return &g_dwm.windows[i];
}

void dosgui_wm_destroy(DosGuiWindow *win) {
    if (!win) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) { close_win(i); return; }
    }
}

void dosgui_wm_set_focus(DosGuiWindow *win) {
    if (!win) return;
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++) {
        if (&g_dwm.windows[i] == win) {
            raise_win(i);
            g_dwm.focused_id = i;
            return;
        }
    }
}

DosGuiWindow *dosgui_wm_get_focused(void) {
    if (g_dwm.focused_id < 0) return NULL;
    return &g_dwm.windows[g_dwm.focused_id];
}

DosGuiWindow *dosgui_wm_find_by_id(int id) {
    for (int i = 0; i < DOSGUI_MAX_WINDOWS; i++)
        if (g_dwm.windows[i].alive && g_dwm.windows[i].id == id)
            return &g_dwm.windows[i];
    return NULL;
}

int dosgui_wm_window_count(void) {
    return g_dwm.nz;
}

DosGuiWindow *dosgui_wm_spawn(int x, int y, int w, int h,
                               const char *title,
                               void (*on_draw)(DosGuiWindow *, uint32_t *, int, int)) {
    int i = spawn_window(x, y, w, h, title);
    if (i < 0) return NULL;
    g_dwm.windows[i].on_draw = on_draw;
    return &g_dwm.windows[i];
}

/* -- Input ------------------------------------------------------- */

void dosgui_wm_handle_key(uint32_t key, uint32_t mods) {
    (void)mods;
    if (key == 111 /* Escape */ && g_dwm.focused_id >= 0) {
        close_win(g_dwm.focused_id);
    }
    /* Ctrl+T = cycle theme */
    if ((mods & 0x4) && key == 0x14) { /* Ctrl+T */
        wubu_theme_cycle();
        fprintf(stderr, "Theme cycled to: %s\n", wubu_theme_name(wubu_theme_current()));
    }
}

void dosgui_wm_handle_mouse(int x, int y, int btn, int kind) {
    (void)btn;
    g_dwm.mouse_x = x;
    g_dwm.mouse_y = y;

    int task_h = taskbar_height_dynamic();
    int tbh = title_bar_height();
    (void)border_width();

    if (kind == 1) { /* down */
        if (y >= g_dwm.screen_h - task_h) {
            /* Taskbar click — handled in taskbar render/input */
            return;
        }
        int i = hit_test(x, y);
        if (i >= 0) {
            raise_win(i);
            g_dwm.focused_id = i;
            DosGuiWindow *w = &g_dwm.windows[i];

            /* Check close button */
            int close_x = w->x + w->w - theme_radius() - 18;
            int close_y = w->y + theme_radius() + 2;
            if (x >= close_x && x < close_x + 14 &&
                y >= close_y && y < close_y + 12) {
                close_win(i);
                return;
            }

            /* Check maximize button (XP) */
            if (theme()->Luna_start_button) {
                int max_x = close_x - 20;
                if (x >= max_x && x < max_x + 14 &&
                    y >= close_y && y < close_y + 12) {
                    /* Toggle maximize - not implemented yet */
                    return;
                }
                int min_x = close_x - 40;
                if (x >= min_x && x < min_x + 14 &&
                    y >= close_y && y < close_y + 12) {
                    /* Minimize - not implemented yet */
                    return;
                }
            }

            /* Start drag if in title bar */
            if (y < w->y + tbh) {
                g_dwm.drag_id = i;
                g_dwm.drag_ox = x - w->x;
                g_dwm.drag_oy = y - w->y;
            }
        } else {
            g_dwm.focused_id = -1;
            /* Check desktop icons */
            int icon_idx = dosgui_icon_hit_test(x, y);
            if (icon_idx >= 0 && g_dwm.icons[icon_idx].on_click) {
                g_dwm.icons[icon_idx].on_click();
            }
        }
    } else if (kind == 2) { /* up */
        g_dwm.drag_id = -1;
    } else if (kind == 0) { /* move */
        if (g_dwm.drag_id >= 0 && g_dwm.windows[g_dwm.drag_id].alive) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.drag_id];
            w->x = x - g_dwm.drag_ox;
            w->y = y - g_dwm.drag_oy;
            /* Clamp */
            if (w->x < -w->w + 60) w->x = -w->w + 60;
            if (w->x > g_dwm.screen_w - 60) w->x = g_dwm.screen_w - 60;
            if (w->y < 0) w->y = 0;
            if (w->y > g_dwm.screen_h - task_h - tbh)
                w->y = g_dwm.screen_h - task_h - tbh;
        }
    }
}

/* -- Desktop Icons ------------------------------------------------------ */

int dosgui_icon_add(const char *name, int gx, int gy,
                        void (*on_click)(void)) {
    if (g_dwm.icon_count >= DOSGUI_MAX_ICONS) return -1;
    DosGuiIcon *icon = &g_dwm.icons[g_dwm.icon_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->grid_x = gx; icon->grid_y = gy;
    icon->x = 20 + gx * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP);
    icon->y = 20 + gy * (DOSGUI_ICON_SIZE + DOSGUI_ICON_GAP + 8);
    icon->on_click = on_click;
    return g_dwm.icon_count++;
}

int dosgui_icon_hit_test(int mx, int my) {
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *ic = &g_dwm.icons[i];
        if (mx >= ic->x && mx < ic->x + DOSGUI_ICON_SIZE &&
            my >= ic->y && my < ic->y + DOSGUI_ICON_SIZE)
            return i;
    }
    return -1;
}

/* -- Taskbar ----------------------------------------------------- */

int dosgui_taskbar_height(void) { return taskbar_height_dynamic(); }

void dosgui_taskbar_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    int th = taskbar_height_dynamic();
    int ty = fb_h - th;

    /* Taskbar background */
    vbe_fill_rect(0, ty, fb_w, th, tc()->taskbar_bg);
    vbe_hline(0, fb_w - 1, ty, tc()->taskbar_border);

    /* Start button */
    int by = ty + (th - 24) / 2;
    int start_w = theme()->Luna_start_button ? 54 : 60;
    
    if (theme()->Luna_start_button) {
        /* XP Luna Start Orb - rounded green button with "Start" text */
        vbe_fill_rect_rounded(4, by, start_w + 20, 24, 4, tc()->start_btn_face);
        vbe_3d_raised_rounded(4, by, start_w + 20, 24, 4);
        vbe_draw_text(8, by + 8, "Start", tc()->start_btn_text, 1);
    } else {
        /* Win98 "+ NEW" button */
        vbe_fill_rect(4, by, 60, 22, tc()->start_btn_face);
        vbe_3d_raised(4, by, 60, 22);
        vbe_draw_text(8, by + 6, "+ NEW", tc()->start_btn_text, 1);
    }

    /* Window buttons */
    int bx = theme()->Luna_start_button ? 82 : 72;
    for (int j = 0; j < g_dwm.nz; j++) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
        if (!w->alive) continue;
        int bw = (int)strlen(w->title) * 6 + 16;
        if (bw > 160) bw = 160;
        bool focused = (g_dwm.zorder[j] == g_dwm.focused_id);
        
        if (theme()->rounded_buttons) {
            if (focused) {
                vbe_fill_rect_rounded(bx, by, bw, 22, 3, tc()->select_bg);
                vbe_3d_sunken_rounded(bx, by, bw, 22, 3);
                vbe_draw_text(bx + 8, by + 7, w->title, tc()->select_text, 1);
            } else {
                vbe_fill_rect_rounded(bx, by, bw, 22, 3, tc()->btn_face);
                vbe_3d_raised_rounded(bx, by, bw, 22, 3);
                vbe_draw_text(bx + 8, by + 7, w->title, tc()->btn_text, 1);
            }
        } else {
            /* Win98 style */
            if (focused) {
                vbe_fill_rect(bx, by, bw, 22, 0x000080);
                vbe_3d_sunken(bx, by, bw, 22);
                vbe_draw_text(bx + 8, by + 6, w->title, 0xFFFFFF, 1);
            } else {
                vbe_fill_rect(bx, by, bw, 22, tc()->btn_face);
                vbe_3d_raised(bx, by, bw, 22);
                vbe_draw_text(bx + 8, by + 6, w->title, tc()->btn_text, 1);
            }
        }
        bx += bw + 2;
        if (bx > fb_w - 160) break;
    }

    /* Clock */
    int secs = g_dwm.ticks / 10;
    char clk[16];
    snprintf(clk, sizeof(clk), "UP %02d:%02d", (secs / 60) % 100, secs % 60);
    int clk_w = vbe_text_width(clk, 1);
    vbe_draw_text(fb_w - clk_w - 10, ty + (th - 8) / 2, clk, 
                  theme()->Luna_start_button ? 0xFFFFFF : tc()->icon_text, 1);
}

/* -- Full Render ------------------------------------------------- */

void dosgui_wm_render_desktop(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    draw_desktop_bg(fb_w, fb_h);

    /* Desktop icons */
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *icon = &g_dwm.icons[i];
        /* Icon background */
        vbe_fill_rect(icon->x, icon->y, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE, 0x008080);
        vbe_rect(icon->x, icon->y, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE, 0x000000);
        /* Icon text with shadow */
        vbe_draw_text(icon->x + 1, icon->y + DOSGUI_ICON_SIZE + 3, icon->name,
                      tc()->icon_text_shadow, 1);
        vbe_draw_text(icon->x, icon->y + DOSGUI_ICON_SIZE + 2, icon->name,
                      tc()->icon_text, 1);
    }

    /* Windows in z-order */
    for (int j = 0; j < g_dwm.nz; j++)
        draw_window(g_dwm.zorder[j]);

    /* Taskbar */
    dosgui_taskbar_render(fb, fb_w, fb_h);

    /* Cursor */
    vbe_draw_cursor(g_dwm.mouse_x, g_dwm.mouse_y);
}

/* -- Tick ------------------------------------------------------- */

void dosgui_tick(void) {
    g_dwm.ticks++;
}

/* -- Query ------------------------------------------------------- */

int dosgui_wm_screen_w(void) { return g_dwm.screen_w; }
int dosgui_wm_screen_h(void) { return g_dwm.screen_h; }