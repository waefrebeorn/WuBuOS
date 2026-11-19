/*
 * wm_systray.c  --  System Tray + Notification Center
 *
 * Extracted from wm.c for modularity.
 */

#include "../wm_internal.h"
#include "../wubu_tokens.h"
#include "wubu_notify.h"

/* ================================================================
 * SYSTEM TRAY / NOTIFICATION AREA
 * ================================================================ */

/* Tray DRAWING lives in wm_taskbar.c (both eras) — one owner. The static
 * renderer that used to sit here was dead code with a divergent layout
 * (fixed 50px margin vs the taskbar's clock-aware wells). What this module
 * owns: tray icon registry, notification center, and the tray HIT-TEST
 * (systray_icon_at) against the well the renderer publishes. */

int systray_add(const char *name, uint32_t color,
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

void systray_remove(const char *name) {
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

/* Update an existing icon's color (the world-state tray calls this
 * on every refresh). Returns 0 on success. */
int systray_update_color(const char *name, uint32_t color) {
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (strcmp(g_dwm.systray_icons[i].name, name) == 0) {
            g_dwm.systray_icons[i].icon_color = color;
            return 0;
        }
    }
    return -1;
}

/* The current color of an icon (for the tests). Returns 0 if missing. */
uint32_t systray_color(const char *name) {
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (strcmp(g_dwm.systray_icons[i].name, name) == 0)
            return g_dwm.systray_icons[i].icon_color;
    }
    return 0;
}

void systray_set_notification_count(const char *name, int count) {
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (strcmp(g_dwm.systray_icons[i].name, name) == 0) {
            g_dwm.systray_icons[i].notification_count = count;
            return;
        }
    }
}

/* ================================================================
 * NOTIFICATION CENTER
 * ================================================================ */

int notif_center_add(const char *app_name, const char *summary,
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
    systray_set_notification_count("Notifications", g_dwm.notif_count);

    /* Also send to wubu_notify daemon if available */
    (void)wubu_notify_simple(app_name, summary, body ? body : "",
                              NULL, urgency, urgency == 2 ? 0 : 5000);

    return n->id;
}

void notif_center_mark_read(uint32_t id) {
    for (int i = 0; i < g_dwm.notif_count; i++) {
        if (g_dwm.notifications[i].id == id) {
            g_dwm.notifications[i].read = true;
            return;
        }
    }
}

void notif_center_clear(void) {
    g_dwm.notif_count = 0;
    systray_set_notification_count("Notifications", 0);
}

bool notif_center_is_open(void) {
    return g_dwm.notif_center_open;
}

void notif_center_toggle(void) {
    g_dwm.notif_center_open = !g_dwm.notif_center_open;
    /* Mark all as read when opening */
    if (g_dwm.notif_center_open) {
        for (int i = 0; i < g_dwm.notif_count; i++) {
            g_dwm.notifications[i].read = true;
        }
        systray_set_notification_count("Notifications", 0);
    }
}

void notif_center_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!g_dwm.notif_center_open) return;

    int th = taskbar_height_dynamic();
    int ty = fb_h - th;

    /* Draw panel on right side, above taskbar */
    int panel_w = WUBU_NOTIF_PANEL_W;
    int panel_h = fb_h - th;
    int panel_x = fb_w - panel_w;
    int panel_y = ty - panel_h;

    vbe_fill_rect_rounded(panel_x, panel_y, panel_w, panel_h,
                          WUBU_METRIC_RADIUS_WINDOW, tc()->win_face);
    vbe_3d_sunken_rounded_colors(panel_x, panel_y, panel_w, panel_h,
                                  WUBU_METRIC_RADIUS_WINDOW,
                                  tc()->border_light, tc()->border_face,
                                  tc()->border_dark, tc()->border_darkest);

    /* Header */
    vbe_fill_rect_rounded(panel_x + WUBU_SPACE_XS, panel_y + WUBU_SPACE_XS,
                          panel_w - 2 * WUBU_SPACE_XS, WUBU_NOTIF_HEADER_H,
                          WUBU_METRIC_RADIUS_CONTROL, tc()->select_bg);
    vbe_draw_text(panel_x + WUBU_NOTIF_HEADER_PAD, panel_y + WUBU_NOTIF_HEADER_PAD,
                  "Notification Center", tc()->select_text, 1);

    /* Notifications list */
    int ny = panel_y + WUBU_NOTIF_LIST_TOP;
    for (int i = 0; i < g_dwm.notif_count; i++) {
        DosGuiNotification *n = &g_dwm.notifications[i];
        if (ny + WUBU_NOTIF_ROW_PITCH > panel_y + panel_h - WUBU_NOTIF_BOTTOM_PAD) break;

        uint32_t bg = n->read ? tc()->notif_read_bg : tc()->select_bg;
        vbe_fill_rect_rounded(panel_x + WUBU_SPACE_XS, ny,
                          panel_w - 2 * WUBU_SPACE_XS, WUBU_NOTIF_CARD_H,
                          WUBU_METRIC_RADIUS_CONTROL, bg);
        vbe_3d_raised_rounded_colors(panel_x + WUBU_SPACE_XS, ny,
                                      panel_w - 2 * WUBU_SPACE_XS,
                                      WUBU_NOTIF_CARD_H, WUBU_METRIC_RADIUS_CONTROL,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);

        /* Urgency indicator */
        uint32_t urg_color = (n->urgency == 2) ? WUBU_STATUS_CRIT
                       : (n->urgency == 1 ? WUBU_STATUS_WARN : WUBU_STATUS_OK);
        vbe_fill_rect(panel_x + WUBU_NOTIF_URG_INSET, ny + WUBU_NOTIF_URG_INSET,
                  WUBU_NOTIF_URG_W, WUBU_NOTIF_URG_H, urg_color);

        /* App name */
        vbe_draw_text(panel_x + WUBU_NOTIF_TEXT_DX, ny + WUBU_NOTIF_URG_INSET, n->app_name, tc()->icon_text, 1);

        /* Summary */
        vbe_draw_text(panel_x + WUBU_NOTIF_TEXT_DX, ny + WUBU_NOTIF_SUMMARY_DY, n->summary, n->read ? tc()->icon_text_shadow : tc()->win_title_text, 1);

        /* Body */
        if (n->body[0]) {
            vbe_draw_text(panel_x + WUBU_NOTIF_TEXT_DX, ny + WUBU_NOTIF_BODY_DY, n->body, tc()->icon_text_shadow, 1);
        }

        /* Time */
        char time_str[16];
        time_t t = n->timestamp;
        struct tm *tm = localtime(&t);
        snprintf(time_str, sizeof(time_str), "%02d:%02d", tm->tm_hour, tm->tm_min);
        vbe_draw_text(panel_x + panel_w - WUBU_NOTIF_TIME_RESERVE,
                  ny + WUBU_NOTIF_URG_INSET, time_str, tc()->icon_text_shadow, 1);

        ny += WUBU_NOTIF_ROW_PITCH;
    }
}

/* Tray hit-test: ONE owner, against the well the taskbar renderer
 * publishes (g_dwm.tray_well_*). Both eras compact visible icons at
 * pitch SIZE+XS from the well's left edge, so the i-th visible icon
 * lives at well_x + i*pitch. Returns the icon's ARRAY index, or -1.
 * The old input path re-derived a right-aligned walk from the clock
 * and missed the modern centred bar entirely. */
int systray_icon_at(int px, int py) {
    if (g_dwm.tray_well_w <= 0) return -1;
    if (px < g_dwm.tray_well_x || px >= g_dwm.tray_well_x + g_dwm.tray_well_w ||
        py < g_dwm.tray_well_y || py >= g_dwm.tray_well_y + g_dwm.tray_well_h)
        return -1;
    int pitch = DOSGUI_SYSTRAY_SIZE + WUBU_SPACE_XS;
    int slot = (px - g_dwm.tray_well_x) / pitch;
    int seen = 0;
    for (int i = 0; i < g_dwm.systray_count; i++) {
        if (!g_dwm.systray_icons[i].visible) continue;
        if (seen == slot) return i;
        seen++;
    }
    return -1;
}
