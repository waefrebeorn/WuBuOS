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
        int tx = x + i * 56;
        int active = (i == g_ctrl.active_tab);
        uint32_t bg = active ? tc->select_bg : tc->btn_face;
        vbe_fill_rect(tx, y, 56, CTRL_TAB_H, bg);
        if (active) vbe_3d_sunken_colors(tx, y, 56, CTRL_TAB_H,
                                          tc->border_light, tc->border_face,
                                          tc->border_dark, tc->border_darkest);
        else vbe_3d_raised_colors(tx, y, 56, CTRL_TAB_H,
                                   tc->border_light, tc->border_face,
                                   tc->border_dark, tc->border_darkest);

        if (i == g_ctrl.hover_tab && !active) {
            vbe_rect(tx, y, 56, CTRL_TAB_H, tc->btn_hover);
        }
    }

    /* Tab panel background */
    vbe_fill_rect(x, y + CTRL_TAB_H, w, win->h - WM_TITLE_HEIGHT - CTRL_TAB_H - 8, tc->win_face);
    vbe_3d_sunken_colors(x, y + CTRL_TAB_H, w, win->h - WM_TITLE_HEIGHT - CTRL_TAB_H - 8,
                          tc->border_light, tc->border_face,
                          tc->border_dark, tc->border_darkest);
}

static void ctrl_draw_display_tab(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;
    int line_h = 24;

    /* Resolution */
    vbe_draw_text(x, y, "Display Resolution:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Width:", tc->btn_text, 1);
    vbe_draw_text(x + 160, y, "Height:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Refresh Rate:", tc->btn_text, 1);
    y += line_h * 2;

    /* Wallpaper */
    vbe_draw_text(x, y, "Wallpaper:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "None (solid color)", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Browse...", tc->btn_text, 1);
    y += line_h * 2;

    /* DPI / Scaling */
    vbe_draw_text(x, y, "Scaling:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "100% (native)", tc->btn_text, 1);
}

static void ctrl_draw_theme_tab(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;
    int item_h = 36;

    /* Theme options: Win98, XP Luna, XP Media, WuBu Green */
    int current = (int)wubu_theme_current();
    const char *theme_names[4] = {
        "Win98 Classic",
        "XP Luna Blue", 
        "XP Media Orange",
        "WuBu Green"
    };

    for (int i = 0; i < 4; i++) {
        int iy = y + i * item_h;
        int active = (i == current);
        uint32_t bg = active ? tc->select_bg : tc->btn_face;
        vbe_fill_rect(x, iy, 300, item_h - 4, bg);
        vbe_3d_raised_colors(x, iy, 300, item_h - 4,
                              tc->border_light, tc->border_face,
                              tc->border_dark, tc->border_darkest);

        /* Theme preview swatch - left side */
        vbe_fill_rect(x + 8, iy + 8, 20, 20, tc->desktop_bg);
        vbe_rect(x + 8, iy + 8, 20, 20, tc->border_dark);

        /* Theme name */
        vbe_draw_text(x + 36, iy + 12, theme_names[i], tc->btn_text, 1);

        if (active) {
            vbe_draw_text(x + 200, iy + 12, "(current)", tc->btn_text, 1);
        }
    }
}

static void ctrl_draw_desktop_tab(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;
    int line_h = 24;

    /* Icon settings */
    vbe_draw_text(x, y, "Desktop Icons:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Show Icons: [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Auto Arrange: [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Align to Grid: [ ]", tc->btn_text, 1);
    y += line_h * 2;

    /* Screen saver */
    vbe_draw_text(x, y, "Screen Saver:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "None", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Blank Screen", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "WuBu Logo", tc->btn_text, 1);
    y += line_h * 2;

    /* Desktop background */
    vbe_draw_text(x, y, "Background Color:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Current: ", tc->btn_text, 1);
    vbe_fill_rect(x + 80, y, 20, 16, tc->desktop_bg);
    vbe_rect(x + 80, y, 20, 16, tc->border_dark);
}

static void ctrl_draw_taskbar_tab(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;
    int line_h = 24;

    /* Taskbar behavior */
    vbe_draw_text(x, y, "Taskbar:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Auto-hide: [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Always on Top: [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Show Window Buttons: [ ]", tc->btn_text, 1);
    y += line_h * 2;

    /* Clock */
    vbe_draw_text(x, y, "Clock:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Format: 12-hour / 24-hour", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Show Seconds: [ ]", tc->btn_text, 1);
    y += line_h * 2;

    /* System Tray */
    vbe_draw_text(x, y, "System Tray:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Hide Inactive Icons: [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Show Volume: [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Show Network: [ ]", tc->btn_text, 1);
}

static void ctrl_draw_input_tab(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;
    int line_h = 24;

    /* Mouse */
    vbe_draw_text(x, y, "Mouse:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Speed: [======     ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Double-click Speed: [======     ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Left-handed: [ ]", tc->btn_text, 1);
    y += line_h * 2;

    /* Keyboard */
    vbe_draw_text(x, y, "Keyboard:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Repeat Delay: [======     ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Repeat Rate: [======     ]", tc->btn_text, 1);
    y += line_h * 2;

    /* Cursor */
    vbe_draw_text(x, y, "Cursor:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Blink Rate: [======     ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Show Pointer Trails: [ ]", tc->btn_text, 1);
}

static void ctrl_draw_startup_tab(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;
    int line_h = 24;

    /* Boot mode */
    vbe_draw_text(x, y, "Boot Mode:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "RAM Disk (Fast, Volatile): [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Disk Image (Persistent): [ ]", tc->btn_text, 1);
    y += line_h * 2;

    /* Login */
    vbe_draw_text(x, y, "Login:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Auto-login: [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Username: [___________]", tc->btn_text, 1);
    y += line_h * 2;

    /* Startup items */
    vbe_draw_text(x, y, "Startup Items:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "WuBuOS Shell", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Network Manager", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Container Daemon", tc->btn_text, 1);
}

static void ctrl_draw_containers_tab(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;
    int line_h = 24;

    /* Default mounts */
    vbe_draw_text(x, y, "Default Mounts:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "/wubu -> /var/wubu [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "/apps -> /var/apps [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "/home -> /home/wubu [ ]", tc->btn_text, 1);
    y += line_h * 2;

    /* Resource limits */
    vbe_draw_text(x, y, "Resource Limits:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Max Memory per Container: [512 MB]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Max CPU %: [100%]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Max Containers: [16]", tc->btn_text, 1);
    y += line_h * 2;

    /* Network isolation */
    vbe_draw_text(x, y, "Network:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Isolate Container Network: [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Allow Host Access: [ ]", tc->btn_text, 1);
}

static void ctrl_draw_network_tab(WmWindow *win) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = win->x + 8;
    int y = win->y + WM_TITLE_HEIGHT + CTRL_TAB_H + 8;
    int line_h = 24;

    /* Network interfaces */
    vbe_draw_text(x, y, "Network Interfaces:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "lo (Loopback) - UP", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "eth0 (Ethernet) - UP", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "wlan0 (WiFi) - DOWN", tc->btn_text, 1);
    y += line_h * 2;

    /* IP Configuration */
    vbe_draw_text(x, y, "IP Configuration:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "DHCP: [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Static IP: [___________]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Netmask: [___________]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Gateway: [___________]", tc->btn_text, 1);
    y += line_h * 2;

    /* DNS */
    vbe_draw_text(x, y, "DNS Servers:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Primary: [___________]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Secondary: [___________]", tc->btn_text, 1);
    y += line_h * 2;

    /* Proxy */
    vbe_draw_text(x, y, "Proxy:", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Use Proxy: [ ]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Address: [___________]", tc->btn_text, 1);
    y += line_h;
    vbe_draw_text(x + 16, y, "Port: [____]", tc->btn_text, 1);
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