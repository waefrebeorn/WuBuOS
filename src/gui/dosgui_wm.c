/*
 * dosgui_wm.c  --  WuBuOS DosGui Window Manager Implementation
 *
 * Cell 400: Fable Windowing Agent.
 * Ports ZealOS/WuBuDos bare-metal window management into WuBuOS.
 * Based on Mythos Fable's wm.c (filipvabrousek/osdev).
 *
 * Features from Fable:
 *   - Draggable windows with title bars
 *   - Z-order + focus management
 *   - Taskbar with window buttons, clock, "+ NEW" button
 *   - Close box (red X) on each window
 *   - Software mouse cursor (18-row arrow)
 *   - Desktop icons with click handlers
 *   - Drop shadow under windows
 */

#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

/* ================================================================
 * RENDERING — Fable sauce adapted for WuBuOS VBE
 * ================================================================ */

static void draw_desktop_bg(int fb_w, int fb_h) {
    int task_h = dosgui_taskbar_height();
    vbe_fill_rect(0, 0, fb_w, fb_h - task_h, C_WIN_DESKTOP);
}

static void draw_window(int idx) {
    DosGuiWindow *w = &g_dwm.windows[idx];
    if (!w->alive) return;
    bool active = (idx == g_dwm.focused_id);

    /* Drop shadow */
    vbe_shade_rect(w->x + 4, w->y + 4, w->w, w->h);

    /* Window face — silver Win98 */
    vbe_fill_rect(w->x, w->y, w->w, w->h, 0xC8C4BC);
    vbe_rect(w->x, w->y, w->w, w->h, 0x1A1A1A);

    /* Title bar */
    vbe_title_bar(w->x + 1, w->y + 1, w->w - 2, DOSGUI_TITLE_H - 2, active);

    /* Title text */
    vbe_draw_text(w->x + 8, w->y + 6, w->title,
                  active ? 0xFFFFFF : 0xD8D8D8, 1);

    /* Close box */
    vbe_close_box(w->x + w->w - 18, w->y + 4, active);

    /* Content area */
    int cx = w->x + DOSGUI_BORDER;
    int cy = w->y + DOSGUI_TITLE_H;
    int cw = w->w - 2 * DOSGUI_BORDER;
    int ch = w->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    /* Content background + sunken border */
    vbe_fill_rect(cx, cy, cw, ch, 0xC0C0C0);
    vbe_3d_sunken(cx - 1, cy - 1, cw + 2, ch + 2);

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
}

void dosgui_wm_handle_mouse(int x, int y, int btn, int kind) {
    (void)btn;
    g_dwm.mouse_x = x;
    g_dwm.mouse_y = y;

    if (kind == 1) { /* down */
        int task_h = dosgui_taskbar_height();
        if (y >= g_dwm.screen_h - task_h) {
            /* Taskbar click — handled elsewhere */
            return;
        }
        int i = hit_test(x, y);
        if (i >= 0) {
            raise_win(i);
            g_dwm.focused_id = i;
            DosGuiWindow *w = &g_dwm.windows[i];
            /* Check close button */
            if (x >= w->x + w->w - 18 && x < w->x + w->w - 4 &&
                y >= w->y + 4 && y < w->y + 16) {
                close_win(i);
                return;
            }
            /* Start drag if in title bar */
            if (y < w->y + DOSGUI_TITLE_H) {
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
            if (w->y > g_dwm.screen_h - DOSGUI_TASK_H - DOSGUI_TITLE_H)
                w->y = g_dwm.screen_h - DOSGUI_TASK_H - DOSGUI_TITLE_H;
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

int dosgui_taskbar_height(void) { return DOSGUI_TASK_H; }

void dosgui_taskbar_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    int ty = fb_h - DOSGUI_TASK_H;

    /* Taskbar background */
    vbe_fill_rect(0, ty, fb_w, DOSGUI_TASK_H, 0x1F242E);
    vbe_hline(0, fb_w - 1, ty, 0x4A5564);

    /* Start button */
    int by = ty + (DOSGUI_TASK_H - 22) / 2;
    vbe_fill_rect(4, by, 60, 22, 0x2ECC71);
    vbe_rect(4, by, 60, 22, 0x0E5E33);
    vbe_draw_text(8, by + 6, "+ NEW", 0x06301A, 1);

    /* Window buttons */
    int bx = 72;
    for (int j = 0; j < g_dwm.nz; j++) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
        if (!w->alive) continue;
        int bw = (int)strlen(w->title) * 6 + 16;
        if (bw > 140) bw = 140;
        bool focused = (g_dwm.zorder[j] == g_dwm.focused_id);
        vbe_fill_rect(bx, by, bw, 22, focused ? 0x000080 : 0xC0C0C0);
        if (focused) {
            vbe_3d_sunken(bx, by, bw, 22);
            vbe_draw_text(bx + 8, by + 6, w->title, 0xFFFFFF, 1);
        } else {
            vbe_3d_raised(bx, by, bw, 22);
            vbe_draw_text(bx + 8, by + 6, w->title, 0x000000, 1);
        }
        bx += bw + 2;
        if (bx > fb_w - 160) break;
    }

    /* Clock */
    int secs = g_dwm.ticks / 10;
    char clk[16];
    snprintf(clk, sizeof(clk), "UP %02d:%02d", (secs / 60) % 100, secs % 60);
    vbe_draw_text(fb_w - vbe_text_width(clk, 1) - 10, ty + 8, clk, 0xC8D2E0, 1);
}

/* -- Full Render ------------------------------------------------- */

void dosgui_wm_render_desktop(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    draw_desktop_bg(fb_w, fb_h);

    /* Desktop icons */
    for (int i = 0; i < g_dwm.icon_count; i++) {
        DosGuiIcon *icon = &g_dwm.icons[i];
        vbe_fill_rect(icon->x, icon->y, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE,
                      0x008080);
        vbe_rect(icon->x, icon->y, DOSGUI_ICON_SIZE, DOSGUI_ICON_SIZE,
                 0x000000);
        vbe_draw_text(icon->x, icon->y + DOSGUI_ICON_SIZE + 2,
                      icon->name, 0xFFFFFF, 1);
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
