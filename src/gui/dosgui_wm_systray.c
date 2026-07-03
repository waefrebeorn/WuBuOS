/*
 * dosgui_wm_systray.c  --  System Tray + Notification Center
 *
 * Extracted from dosgui_wm.c for modularity.
 */

#include "dosgui_wm_internal.h"

/* ================================================================
 * SYSTEM TRAY / NOTIFICATION AREA
 * ================================================================ */

static void draw_systray_icon(int idx, int ty, int th) {
    DosGuiSysTrayIcon *icon = &g_dwm.systray_icons[idx];
    if (!icon->visible) return;

    int x = g_dwm.screen_w - 50 - idx * (DOSGUI_SYSTRAY_SIZE + 4);
    int y = ty + (th - DOSGUI_SYSTRAY_SIZE) / 2;

    /* Draw icon background */
    vbe_fill_rect(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE, tc()->btn_face);
    vbe_3d_raised_colors(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE,
                         tc()->border_light, tc()->border_face,
                         tc()->border_dark, tc()->border_darkest);

    /* Draw simple colored square as icon */
    vbe_fill_rect(x + 4, y + 4, 16, 16, icon->icon_color);

    /* Draw notification badge if count > 0 */
    if (icon->notification_count > 0) {
        char badge[8];
        snprintf(badge, sizeof(badge), "%d", icon->notification_count > 9 ? 9 : icon->notification_count);
        int bx = x + DOSGUI_SYSTRAY_SIZE - 8;
        int by = y;
        vbe_fill_rect_rounded(bx, by, 12, 12, 6, 0xFF0000);
        vbe_draw_text(bx + 2, by + 1, badge, 0xFFFFFF, 1);
    }
}

int dosgui_systray_add(const char *name, uint32_t color,
                        void (*on_click)(void),
                        void (*on_right_click)(void)) {
    if (g_dwm.systray_count >= DOSGUI_MAX_SYSTRAY_ICONS) return -1;

    DosGuiSysTrayIcon *icon = &g_dwm.systray_icons[g_dwm.systray_count];
    memset(icon, 0, sizeof(*icon));
    strncpy(icon->name, name, sizeof(icon->name) - 1);
    icon->icon_color = color;
    icon->visible = true;
    icon->on_click = on_click;
    icon->on_right_click = on_right_click;
    icon->notification_count = 0;

    return g_dwm.systray_count++;
}

void dosgui_systray_remove(const char *name) {
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (strcmp(g_dwm.systray_icons[i].name, name) == 0) {
            for (int j = i; j < g_dwm.systray_count - 1; j++) {
                g_dwm.systray_icons[j] = g_dwm.systray_icons[j + 1];
            }
            g_dwm.systray_count--;
            return;
        }
    }
}

void dosgui_systray_set_notification_count(const char *name, int count) {
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (strcmp(g_dwm.systray_icons[i].name, name) == 0) {
            g_dwm.systray_icons[i].notification_count = count;
            return;
        }
    }
}

/* ================================================================

/* ================================================================
 * NOTIFICATION CENTER
 * ================================================================ */

/* ================================================================
 * NOTIFICATION CENTER
 * ================================================================ */

int dosgui_notif_center_add(const char *app_name, const char *summary,
                             const char *body, int urgency) {
    if (g_dwm.notif_count >= DOSGUI_NOTIF_CENTER_MAX) {
        /* Shift oldest out */
        for (int i = 1; i < g_dwm.notif_count; i++) {
            g_dwm.notifications[i - 1] = g_dwm.notifications[i];
        }
        g_dwm.notif_count--;
    }

    DosGuiNotification *n = &g_dwm.notifications[g_dwm.notif_count];
    memset(n, 0, sizeof(*n));
    n->id = g_dwm.next_notif_id++;
    strncpy(n->app_name, app_name, sizeof(n->app_name) - 1);
    strncpy(n->summary, summary, sizeof(n->summary) - 1);
    if (body) strncpy(n->body, body, sizeof(n->body) - 1);
    n->timestamp = (uint32_t)time(NULL);
    n->urgency = urgency;
    n->read = false;
    n->expanded = false;

    g_dwm.notif_count++;

    /* Update systray notification badge */
    dosgui_systray_set_notification_count("Notifications", g_dwm.notif_count);

    /* Also send to wubu_notify daemon if available */
    (void)wubu_notify_simple(app_name, summary, body ? body : "",
                              NULL, urgency, urgency == 2 ? 0 : 5000);

    return n->id;
}

void dosgui_notif_center_mark_read(uint32_t id) {
    for (int i = 0; i < g_dwm.notif_count; i++) {
        if (g_dwm.notifications[i].id == id) {
            g_dwm.notifications[i].read = true;
            return;
        }
    }
}

void dosgui_notif_center_clear(void) {
    g_dwm.notif_count = 0;
    dosgui_systray_set_notification_count("Notifications", 0);
}

bool dosgui_notif_center_is_open(void) {
    return g_dwm.notif_center_open;
}

void dosgui_notif_center_toggle(void) {
    g_dwm.notif_center_open = !g_dwm.notif_center_open;
    /* Mark all as read when opening */
    if (g_dwm.notif_center_open) {
        for (int i = 0; i < g_dwm.notif_count; i++) {
            g_dwm.notifications[i].read = true;
        }
        dosgui_systray_set_notification_count("Notifications", 0);
    }
}

void dosgui_notif_center_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_dwm.notif_center_open) return;

    int th = taskbar_height_dynamic();
    int ty = fb_h - th;

    /* Draw panel on right side, above taskbar */
    int panel_w = 350;
    int panel_h = fb_h - th;
    int panel_x = fb_w - panel_w;
    int panel_y = ty - panel_h;

    vbe_fill_rect_rounded(panel_x, panel_y, panel_w, panel_h, 8, tc()->win_face);
    vbe_3d_sunken_rounded_colors(panel_x, panel_y, panel_w, panel_h, 8,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);

    /* Header */
    vbe_fill_rect_rounded(panel_x + 4, panel_y + 4, panel_w - 8, 30, 4, tc()->select_bg);
    vbe_draw_text(panel_x + 10, panel_y + 10, "Notification Center", tc()->select_text, 1);

    /* Notifications list */
    int ny = panel_y + 40;
    for (int i = 0; i < g_dwm.notif_count; i++) {
        DosGuiNotification *n = &g_dwm.notifications[i];
        if (ny + 60 > panel_y + panel_h - 10) break;

        uint32_t bg = n->read ? 0xFF303030 : tc()->select_bg;
        vbe_fill_rect_rounded(panel_x + 4, ny, panel_w - 8, 56, 4, bg);
        vbe_3d_raised_rounded_colors(panel_x + 4, ny, panel_w - 8, 56, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);

        /* Urgency indicator */
        uint32_t urg_color = (n->urgency == 2) ? 0xFF0000 : (n->urgency == 1 ? 0xFFFF00 : 0x00FF00);
        vbe_fill_rect(panel_x + 6, ny + 6, 4, 44, urg_color);

        /* App name */
        vbe_draw_text(panel_x + 14, ny + 6, n->app_name, tc()->icon_text, 1);

        /* Summary */
        vbe_draw_text(panel_x + 14, ny + 18, n->summary, n->read ? tc()->icon_text_shadow : tc()->win_title_text, 1);

        /* Body */
        if (n->body[0]) {
            vbe_draw_text(panel_x + 14, ny + 30, n->body, tc()->icon_text_shadow, 1);
        }

        /* Time */
        char time_str[16];
        time_t t = n->timestamp;
        struct tm *tm = localtime(&t);
        snprintf(time_str, sizeof(time_str), "%02d:%02d", tm->tm_hour, tm->tm_min);
        vbe_draw_text(panel_x + panel_w - 60, ny + 6, time_str, tc()->icon_text_shadow, 1);

        ny += 60;
    }
}
