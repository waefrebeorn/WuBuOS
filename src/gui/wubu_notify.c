/*
 * wubu_notify.c  --  WuBuOS Notification Daemon Implementation
 * Phase 2: libnotify-compatible notification server
 */

#include "wubu_notify.h"
#include "wubu_settings.h"
#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* -- Constants ---------------------------------------------------- */

#define MAX_NOTIFICATIONS 64
#define MAX_HISTORY 100
#define NOTIFICATION_WIDTH 360
#define NOTIFICATION_HEIGHT 100
#define NOTIFICATION_MARGIN 16
#define NOTIFICATION_SPACING 8
#define DEFAULT_TIMEOUT_MS 5000
#define LOW_TIMEOUT_MS 7000
#define CRITICAL_TIMEOUT_MS 0  /* Never expire */

/* -- Notification State ------------------------------------------- */

typedef struct {
    Notification notifications[MAX_NOTIFICATIONS];
    int count;
    uint32_t next_id;
    
    Notification history[MAX_HISTORY];
    int history_count;
    
    NotifyActionCallback action_cb;
    NotifyClosedCallback closed_cb;
    
    int slide_offset;       /* For entrance/exit animation */
    bool animating;
    int animating_id;
    int anim_dir;           /* 1 = entering, -1 = exiting */
} NotifyState;

static NotifyState g_notify = {0};

/* -- Server Capabilities ------------------------------------------ */

static NotifyServerCaps g_caps = {
    .name = "WuBuOS Notification Daemon",
    .vendor = "WuBuOS Project",
    .version = "1.0",
    .spec_version = "1.2",
    .supports_actions = true,
    .supports_body = true,
    .supports_icon = true,
    .supports_persistence = true,
    .supports_sound = false,
};

/* -- Forward Declarations ----------------------------------------- */

static void notify_add_to_history(const Notification *n);
static void notify_start_animation(uint32_t id, int dir);
static void notify_update_animation(void);
static int notify_find_by_id(uint32_t id);
static void notify_remove_at(int index);
static void notify_layout(void);
static void render_notification(int idx, uint32_t *fb, int fb_w, int fb_h);
static int notification_x(int fb_w);
static int notification_y(int fb_h, int index);
static uint64_t get_time_ms(void);

/* -- Notification Daemon API -------------------------------------- */

int wubu_notify_init(void) {
    memset(&g_notify, 0, sizeof(g_notify));
    g_notify.next_id = 1;
    g_notify.slide_offset = 0;
    return 0;
}

void wubu_notify_shutdown(void) {
    wubu_notify_clear_all();
}

const NotifyServerCaps *wubu_notify_get_caps(void) {
    return &g_caps;
}

uint32_t wubu_notify_send(const Notification *n) {
    if (g_notify.count >= MAX_NOTIFICATIONS) return 0;
    
    /* Handle replaces_id */
    if (n->replaces_id > 0) {
        int idx = notify_find_by_id(n->replaces_id);
        if (idx >= 0) notify_remove_at(idx);
    }
    
    Notification new_n = *n;
    new_n.id = g_notify.next_id++;
    new_n.timestamp = get_time_ms();
    
    /* Set defaults */
    if (new_n.timeout < 0) {
        switch (new_n.urgency) {
            case NOTIFY_URGENCY_LOW:      new_n.timeout = LOW_TIMEOUT_MS; break;
            case NOTIFY_URGENCY_NORMAL:   new_n.timeout = DEFAULT_TIMEOUT_MS; break;
            case NOTIFY_URGENCY_CRITICAL: new_n.timeout = CRITICAL_TIMEOUT_MS; break;
        }
    }
    
    /* Add to active list */
    g_notify.notifications[g_notify.count++] = new_n;
    notify_add_to_history(&new_n);
    notify_layout();
    notify_start_animation(new_n.id, 1);
    
    return new_n.id;
}

void wubu_notify_close(uint32_t id) {
    int idx = notify_find_by_id(id);
    if (idx >= 0) {
        if (g_notify.closed_cb) g_notify.closed_cb(id, 3);  /* Close called */
        notify_start_animation(id, -1);
    }
}

int wubu_notify_active_count(void) {
    return g_notify.count;
}

const Notification *wubu_notify_get(int index) {
    if (index < 0 || index >= g_notify.count) return NULL;
    return &g_notify.notifications[index];
}

void wubu_notify_clear_all(void) {
    for (int i = g_notify.count - 1; i >= 0; i--) {
        if (g_notify.closed_cb) g_notify.closed_cb(g_notify.notifications[i].id, 3);
    }
    g_notify.count = 0;
    g_notify.slide_offset = 0;
}

void wubu_notify_set_action_callback(NotifyActionCallback cb) {
    g_notify.action_cb = cb;
}

void wubu_notify_set_closed_callback(NotifyClosedCallback cb) {
    g_notify.closed_cb = cb;
}

/* -- Rendering ---------------------------------------------------- */

void wubu_notify_render(uint32_t *fb, int fb_w, int fb_h) {
    if (g_notify.count == 0) return;
    
    notify_update_animation();
    
    const WubuThemeColors *tc = wubu_theme_colors();
    const WubuTheme *th = wubu_theme_get();
    int rad = th->rounded_buttons ? 8 : 0;
    
    for (int i = 0; i < g_notify.count; i++) {
        Notification *n = &g_notify.notifications[i];
        int x = notification_x(fb_w);
        int y = notification_y(fb_h, i);
        
        /* Slide animation offset */
        x += g_notify.slide_offset;
        
        /* Shadow */
        vbe_shade_rect(x + 4, y + 4, NOTIFICATION_WIDTH, NOTIFICATION_HEIGHT);
        
        /* Background with urgency tint */
        uint32_t bg = tc->startmenu_bg;
        if (n->urgency == NOTIFY_URGENCY_CRITICAL) bg = 0x800000;
        else if (n->urgency == NOTIFY_URGENCY_LOW) bg = 0xC0C0C0;
        
        vbe_fill_rect_rounded(x, y, NOTIFICATION_WIDTH, NOTIFICATION_HEIGHT, rad, bg);
        vbe_rect_rounded(x, y, NOTIFICATION_WIDTH, NOTIFICATION_HEIGHT, rad, tc->border_dark);
        
        /* App name + summary */
        int tx = x + 12;
        int ty = y + 10;
        vbe_draw_text(tx, ty, n->app_name, tc->startmenu_text, 1);
        ty += 16;
        vbe_draw_text(tx, ty, n->summary, tc->startmenu_text, 1);
        
        /* Body */
        if (n->body[0]) {
            ty += 18;
            /* Simple word wrap */
            char line[128];
            const char *p = n->body;
            while (*p && ty < y + NOTIFICATION_HEIGHT - 20) {
                int len = 0;
                while (p[len] && p[len] != '\n' && len < 48) len++;
                if (len == 48) {
                    /* Find last space */
                    while (len > 0 && p[len] != ' ') len--;
                    if (len == 0) len = 48;
                }
                memcpy(line, p, len);
                line[len] = '\0';
                vbe_draw_text(tx, ty, line, tc->startmenu_text, 1);
                ty += 14;
                p += len;
                if (*p == ' ') p++;
                if (*p == '\n') p++;
            }
        }
        
        /* Urgency indicator */
        if (n->urgency == NOTIFY_URGENCY_CRITICAL) {
            vbe_fill_rect(x + NOTIFICATION_WIDTH - 20, y + 10, 8, 8, 0xFF0000);
        }
    }
}

void wubu_notify_tick(void) {
    uint64_t now = get_time_ms();
    
    for (int i = g_notify.count - 1; i >= 0; i--) {
        Notification *n = &g_notify.notifications[i];
        if (n->timeout > 0 && (now - n->timestamp) >= (uint64_t)n->timeout) {
            if (g_notify.closed_cb) g_notify.closed_cb(n->id, 1);  /* Expired */
            notify_start_animation(n->id, -1);
        }
    }
}

/* -- Input Handling ----------------------------------------------- */

bool wubu_notify_handle_click(int x, int y) {
    int fb_w = dosgui_wm_screen_w();
    int fb_h = dosgui_wm_screen_h();
    
    for (int i = 0; i < g_notify.count; i++) {
        int nx = notification_x(fb_w);
        int ny = notification_y(fb_h, i);
        
        if (x >= nx && x < nx + NOTIFICATION_WIDTH &&
            y >= ny && y < ny + NOTIFICATION_HEIGHT) {
            Notification *n = &g_notify.notifications[i];
            
            /* Check if clicked on action area (right side) */
            if (n->action_count > 0 && x > nx + NOTIFICATION_WIDTH - 80) {
                int action_idx = (y - ny) / (NOTIFICATION_HEIGHT / n->action_count);
                if (action_idx >= 0 && action_idx < n->action_count && g_notify.action_cb) {
                    g_notify.action_cb(n->id, n->actions[action_idx].key);
                }
            }
            
            /* Dismiss on click */
            if (g_notify.closed_cb) g_notify.closed_cb(n->id, 2);  /* Dismissed by user */
            notify_start_animation(n->id, -1);
            return true;
        }
    }
    return false;
}

bool wubu_notify_handle_key(uint32_t key) {
    if (key == 0x01 && g_notify.count > 0) {  /* Escape */
        /* Dismiss most recent */
        Notification *n = &g_notify.notifications[g_notify.count - 1];
        if (g_notify.closed_cb) g_notify.closed_cb(n->id, 2);
        notify_start_animation(n->id, -1);
        return true;
    }
    return false;
}

/* -- Convenience Helpers ------------------------------------------ */

uint32_t wubu_notify_simple(const char *app_name, const char *summary,
                             const char *body, const char *icon,
                             NotifyUrgency urgency, int timeout) {
    Notification n = {0};
    strncpy(n.app_name, app_name ? app_name : "WuBuOS", sizeof(n.app_name) - 1);
    strncpy(n.summary, summary ? summary : "Notification", sizeof(n.summary) - 1);
    if (body) strncpy(n.body, body, sizeof(n.body) - 1);
    if (icon) strncpy(n.icon, icon, sizeof(n.icon) - 1);
    n.urgency = urgency;
    n.timeout = timeout;
    return wubu_notify_send(&n);
}

uint32_t wubu_notify_progress(const char *app_name, const char *summary,
                               const char *body, int percent) {
    Notification n = {0};
    strncpy(n.app_name, app_name ? app_name : "WuBuOS", sizeof(n.app_name) - 1);
    strncpy(n.summary, summary ? summary : "Progress", sizeof(n.summary) - 1);
    if (body) strncpy(n.body, body, sizeof(n.body) - 1);
    n.urgency = NOTIFY_URGENCY_LOW;
    n.timeout = 2000;
    n.transient = true;
    
    if (n.hint_count < MAX_NOTIFICATION_HINTS) {
        strcpy(n.hints[n.hint_count].key, "value");
        n.hints[n.hint_count].type = 'i';
        n.hints[n.hint_count].value.ival = percent;
        n.hint_count++;
    }
    if (n.hint_count < MAX_NOTIFICATION_HINTS) {
        strcpy(n.hints[n.hint_count].key, "value_max");
        n.hints[n.hint_count].type = 'i';
        n.hints[n.hint_count].value.ival = 100;
        n.hint_count++;
    }
    return wubu_notify_send(&n);
}

uint32_t wubu_notify_system(const char *summary, const char *body) {
    return wubu_notify_simple("System", summary, body, NULL, NOTIFY_URGENCY_CRITICAL, 0);
}

/* -- Internal Helpers --------------------------------------------- */

static void notify_add_to_history(const Notification *n) {
    if (g_notify.history_count >= MAX_HISTORY) {
        /* Shift down */
        for (int i = 1; i < MAX_HISTORY; i++)
            g_notify.history[i-1] = g_notify.history[i];
        g_notify.history_count = MAX_HISTORY - 1;
    }
    g_notify.history[g_notify.history_count++] = *n;
}

static int notify_find_by_id(uint32_t id) {
    for (int i = 0; i < g_notify.count; i++)
        if (g_notify.notifications[i].id == id)
            return i;
    return -1;
}

static void notify_remove_at(int index) {
    if (index < 0 || index >= g_notify.count) return;
    
    if (g_notify.closed_cb && g_notify.animating_id != g_notify.notifications[index].id) {
        g_notify.closed_cb(g_notify.notifications[index].id, 3);
    }
    
    for (int i = index; i < g_notify.count - 1; i++)
        g_notify.notifications[i] = g_notify.notifications[i+1];
    g_notify.count--;
    notify_layout();
}

static void notify_layout(void) {
    /* Layout is computed on-demand in notification_x/y */
}

static void notify_start_animation(uint32_t id, int dir) {
    int idx = notify_find_by_id(id);
    if (idx < 0) return;
    
    g_notify.animating = true;
    g_notify.animating_id = id;
    g_notify.anim_dir = dir;
    g_notify.slide_offset = (dir > 0) ? -NOTIFICATION_WIDTH : 0;
}

static void notify_update_animation(void) {
    if (!g_notify.animating) return;
    
    int speed = 40;  /* pixels per frame */
    
    if (g_notify.anim_dir > 0) {
        g_notify.slide_offset += speed;
        if (g_notify.slide_offset >= 0) {
            g_notify.slide_offset = 0;
            g_notify.animating = false;
            g_notify.animating_id = 0;
        }
    } else {
        g_notify.slide_offset -= speed;
        if (g_notify.slide_offset <= -NOTIFICATION_WIDTH) {
            /* Animation complete, actually remove */
            int idx = notify_find_by_id(g_notify.animating_id);
            if (idx >= 0) {
                for (int i = idx; i < g_notify.count - 1; i++)
                    g_notify.notifications[i] = g_notify.notifications[i+1];
                g_notify.count--;
            }
            g_notify.animating = false;
            g_notify.animating_id = 0;
            g_notify.slide_offset = 0;
            notify_layout();
        }
    }
}

static int notification_x(int fb_w) {
    return fb_w - NOTIFICATION_WIDTH - NOTIFICATION_MARGIN;
}

static int notification_y(int fb_h, int index) {
    int task_h = dosgui_taskbar_height();
    int start_y = fb_h - task_h - NOTIFICATION_MARGIN - NOTIFICATION_HEIGHT;
    return start_y - index * (NOTIFICATION_HEIGHT + NOTIFICATION_SPACING);
}

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}