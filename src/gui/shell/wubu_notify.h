/*
 * wubu_notify.h  --  WuBuOS Notification Daemon
 *
 * Phase 2: GNOME-standard desktop services.
 * libnotify-compatible notification server (Desktop Notifications Specification).
 * Supports: summary, body, icon, urgency, timeout, actions, hints.
 */

#ifndef WUBU_NOTIFY_H
#define WUBU_NOTIFY_H

#include <stdint.h>
#include <stdbool.h>

/* -- Notification Urgency ---------------------------------------- */

typedef enum {
    NOTIFY_URGENCY_LOW      = 0,
    NOTIFY_URGENCY_NORMAL   = 1,
    NOTIFY_URGENCY_CRITICAL = 2,
} NotifyUrgency;

/* -- Notification ------------------------------------------------- */

#define MAX_NOTIFICATION_ACTIONS 8
#define MAX_NOTIFICATION_HINTS   16

typedef struct {
    uint32_t id;                    /* Server-assigned ID (0 = new) */
    char app_name[64];              /* Calling application name */
    char summary[128];              /* Brief summary (title) */
    char body[512];                 /* Optional detailed body */
    char icon[128];                 /* Icon name or path */
    NotifyUrgency urgency;          /* Low/Normal/Critical */
    int timeout;                    /* Milliseconds (-1 = default, 0 = never expire) */
    
    /* Actions: (key, label) pairs */
    struct {
        char key[32];
        char label[64];
    } actions[MAX_NOTIFICATION_ACTIONS];
    int action_count;
    
    /* Hints: key-value pairs for extended data */
    struct {
        char key[32];
        char type;                  /* 's'=string, 'i'=int, 'd'=double, 'b'=bool */
        union {
            char str[128];
            int ival;
            double dval;
            bool bval;
        } value;
    } hints[MAX_NOTIFICATION_HINTS];
    int hint_count;
    
    /* Metadata */
    uint64_t timestamp;             /* Creation time (ms since boot) */
    bool transient;                 /* Bypass history */
    int replaces_id;                /* Replace existing notification */
} Notification;

/* -- Server Capabilities ------------------------------------------ */

typedef struct {
    char name[64];
    char vendor[64];
    char version[16];
    char spec_version[16];
    bool supports_actions;
    bool supports_body;
    bool supports_icon;
    bool supports_persistence;
    bool supports_sound;
} NotifyServerCaps;

/* -- Notification Daemon API -------------------------------------- */

/* Initialize notification server */
int  wubu_notify_init(void);
void wubu_notify_shutdown(void);

/* Get server capabilities */
const NotifyServerCaps *wubu_notify_get_caps(void);

/* Send notification (returns notification ID) */
uint32_t wubu_notify_send(const Notification *n);

/* Close notification by ID */
void wubu_notify_close(uint32_t id);

/* Get active notification count */
int wubu_notify_active_count(void);

/* Get notification by index (for history UI) */
const Notification *wubu_notify_get(int index);

/* Clear all notifications */
void wubu_notify_clear_all(void);

/* Callback for action invocation */
typedef void (*NotifyActionCallback)(uint32_t id, const char *action_key);
void wubu_notify_set_action_callback(NotifyActionCallback cb);

/* Callback for notification closed */
typedef void (*NotifyClosedCallback)(uint32_t id, int reason);
/* reason: 1=expired, 2=dismissed by user, 3=close called, 4=reserved */
void wubu_notify_set_closed_callback(NotifyClosedCallback cb);

/* Render notifications (call from main loop) */
void wubu_notify_render(uint32_t *fb, int fb_w, int fb_h);
void wubu_notify_tick(void);

/* Handle mouse click on notification */
bool wubu_notify_handle_click(int x, int y);

/* Handle keyboard (Escape to dismiss) */
bool wubu_notify_handle_key(uint32_t key);

/* -- Convenience Helpers ------------------------------------------ */

/* Simple notification with just summary and optional body */
uint32_t wubu_notify_simple(const char *app_name, const char *summary,
                             const char *body, const char *icon,
                             NotifyUrgency urgency, int timeout);

/* Progress notification (hint: "value" = 0-100, "value_max" = 100) */
uint32_t wubu_notify_progress(const char *app_name, const char *summary,
                               const char *body, int percent);

/* System notification (high urgency, persistent) */
uint32_t wubu_notify_system(const char *summary, const char *body);

#endif /* WUBU_NOTIFY_H */