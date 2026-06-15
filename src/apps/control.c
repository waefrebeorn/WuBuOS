/*
 * control.c  --  WuBuOS Control Panel (Win98-style Settings)
 *
 * Tabs:
 * - Display: resolution, wallpaper, refresh rate
 * - Theme: Win98 Classic, XP Luna Blue, XP Media Orange, WuBu Green
 * - Desktop: icon size, auto-arrange, grid snap
 * - Taskbar: auto-hide, clock format, tray icons
 * - Input: mouse speed, double-click, keyboard repeat
 * - Startup: auto-login, boot mode (RAM/DISK)
 * - Containers: default mounts, resource limits
 * - Network: WiFi, Ethernet, proxy
 * - About: WuBuOS version, ZealOS kernel hash, GAAD phi
 */

#include "control.h"
#include "../gui/wm.h"
#include "../kernel/vbe.h"
#include <string.h>
#include <stdio.h>

#define CTRL_WIN_W      520
#define CTRL_WIN_H      440
#define CTRL_TAB_H      24
#define CTRL_TAB_COUNT  9

typedef enum {
    CTRL_DISPLAY = 0,
    CTRL_THEME,
    CTRL_DESKTOP,
    CTRL_TASKBAR,
    CTRL_INPUT,
    CTRL_STARTUP,
    CTRL_CONTAINERS,
    CTRL_NETWORK,
    CTRL_ABOUT,
} CtrlTab;

typedef struct {
    int active_tab;
    int hover_tab;
    int control_hover;
    int control_active;
} ControlState;

static ControlState g_ctrl = {0};

static void ctrl_draw_tab_bar(WmWindow *win, void *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 4;
    int y = win->y + WM_TITLE_HEIGHT + 4;
    int w = win->w - 8;
    int tab_w = 56;

    for (int i = 0; i < CTRL_TAB_COUNT; i++) {
        int tx = x + i * tab_w;
        int active = (i == g_ctrl.active_tab);
        uint32_t bg = active ? tc->select_bg : tc->btn_face;
        vbe_fill_rect(tx, y, tab_w, CTRL_TAB_H, bg);
        if (active) vbe_3d_sunken(tx, y, tab_w, CTRL_TAB_H);
        else vbe_3d_raised(tx, y, tab_w, CTRL_TAB_H);

        if (i == g_ctrl.hover_tab && !active) {
            vbe_rect(tx, y, tab_w, CTRL_TAB_H, tc->btn_hover);
        }
    }

    /* Tab panel background */
    vbe_fill_rect(x, y + CTRL_TAB_H, w, win->h - WM_TITLE_HEIGHT - CTRL_TAB_H - 8, tc->win_face);
    vbe_3d_sunken(x, y + CTRL_TAB_H, w, win->h - WM_TITLE_HEIGHT - CTRL_TAB_H - 8);
}

static void ctrl_draw_display_tab(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;

    vbe_fill_rect(x, y, 200, 20, tc->win_title_active);
    /* Resolution dropdown, wallpaper picker, refresh rate */
    (void)win;
}

static void ctrl_draw_theme_tab(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;

    /* Theme options: Win98, XP Luna, XP Media, WuBu Green */
    int current = (int)wubu_theme_current();
    for (int i = 0; i < 4; i++) {
        int iy = y + i * 32;
        int active = (i == current);
        vbe_fill_rect(x, iy, 200, 28, active ? tc->select_bg : tc->btn_face);
        vbe_3d_raised(x, iy, 200, 28);
        /* Theme preview swatch */
        vbe_fill_rect(x + 4, iy + 4, 20, 20, tc->desktop_bg);
        vbe_rect(x + 4, iy + 4, 20, 20, tc->border_dark);
    }
    (void)win;
}

static void ctrl_draw_desktop_tab(WmWindow *win) {
    (void)win;
    /* Icon size, auto-arrange, grid snap */
}

static void ctrl_draw_taskbar_tab(WmWindow *win) {
    (void)win;
    /* Auto-hide, clock format, tray icons */
}

static void ctrl_draw_input_tab(WmWindow *win) {
    (void)win;
    /* Mouse speed, double-click, keyboard repeat */
}

static void ctrl_draw_startup_tab(WmWindow *win) {
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;
    vbe_fill_rect(x, y, 200, 20, 0x00000080);
    /* Boot mode selector */
    (void)win;
}

static void ctrl_draw_containers_tab(WmWindow *win) {
    (void)win;
    /* Default mounts, resource limits */
}

static void ctrl_draw_network_tab(WmWindow *win) {
    (void)win;
    /* WiFi, Ethernet, proxy */
}

static void ctrl_draw_about_tab(WmWindow *win) {
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;

    vbe_fill_rect(x, y, 300, 20, 0x00000080);
    /* WuBuOS version, ZealOS kernel hash, GAAD phi */
    vbe_draw_text(x + 4, y + 4, "WuBuOS v0.1.0", 0x00FFFFFF, 1);
    vbe_draw_text(x + 4, y + 20, "ZealOS Kernel: wubu-custom", 0x00FFFFFF, 1);
    vbe_draw_text(x + 4, y + 36, "GAAD φ = 1.6180339887", 0x00FFFFFF, 1);
}

static void ctrl_draw_tab(WmWindow *win) {
    switch (g_ctrl.active_tab) {
        case CTRL_DISPLAY: ctrl_draw_display_tab(win); break;
        case CTRL_THEME: ctrl_draw_theme_tab(win); break;
        case CTRL_DESKTOP: ctrl_draw_desktop_tab(win); break;
        case CTRL_TASKBAR: ctrl_draw_taskbar_tab(win); break;
        case CTRL_INPUT: ctrl_draw_input_tab(win); break;
        case CTRL_STARTUP: ctrl_draw_startup_tab(win); break;
        case CTRL_CONTAINERS: ctrl_draw_containers_tab(win); break;
        case CTRL_NETWORK: ctrl_draw_network_tab(win); break;
        case CTRL_ABOUT: ctrl_draw_about_tab(win); break;
    }
}

static void control_draw(WmWindow *win, void *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    ctrl_draw_tab_bar(win, fb, fb_w, fb_h);
    ctrl_draw_tab(win);
}

static void control_handle_mouse(WmWindow *win, int x, int y, int btn, int kind) {
    if (kind != 1 || btn != 1) return;

    int tab_y = win->y + WM_TITLE_HEIGHT + 4;
    if (y >= tab_y && y < tab_y + CTRL_TAB_H) {
        int tx = (x - win->x - 4) / 56;
        if (tx >= 0 && tx < CTRL_TAB_COUNT) {
            g_ctrl.active_tab = tx;
            wm_invalidate(win);
        }
    }

    int panel_x = win->x + 8;
    int panel_y = tab_y + CTRL_TAB_H;

    if (g_ctrl.active_tab == CTRL_THEME) {
        for (int i = 0; i < 4; i++) {
            int iy = panel_y + 8 + i * 32;
            if (x >= panel_x && x < panel_x + 200 && y >= iy && y < iy + 28) {
                wubu_theme_set(i);
                wm_invalidate(win);
                break;
            }
        }
    }
    (void)panel_x; (void)kind; (void)btn;
}

static void control_handle_key(WmWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    if (key == 0xE04B) { /* Left */
        if (g_ctrl.active_tab > 0) g_ctrl.active_tab--;
    } else if (key == 0xE04D) { /* Right */
        if (g_ctrl.active_tab < CTRL_TAB_COUNT - 1) g_ctrl.active_tab++;
    }
}

void control_open(void) {
    WmWindow *win = wm_create_window(150, 150, CTRL_WIN_W, CTRL_WIN_H, "Control Panel");
    if (win) {
        win->on_draw = control_draw;
        win->on_mouse = control_handle_mouse;
        win->on_key = control_handle_key;
    }
}

void control_init(void) { memset(&g_ctrl, 0, sizeof(g_ctrl)); }
void control_shutdown(void) { }
