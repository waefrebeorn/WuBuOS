/*
 * wubu_session.h  --  WuBuOS Session Manager
 *
 * Phase 2: GNOME-standard desktop services.
 * Handles auto-start apps, shutdown/logout/restart, session save/restore.
 * Coordinates with settings daemon for persistence.
 */

#ifndef WUBU_SESSION_H
#define WUBU_SESSION_H

#include <stdint.h>
#include <stdbool.h>

/* -- Session Action Types ----------------------------------------- */

typedef enum {
    SESSION_ACTION_NONE     = 0,
    SESSION_ACTION_LOGOUT   = 1,  /* End user session, return to login */
    SESSION_ACTION_SHUTDOWN = 2,  /* Power off system */
    SESSION_ACTION_RESTART  = 3,  /* Reboot system */
    SESSION_ACTION_SUSPEND  = 4,  /* Suspend to RAM */
    SESSION_ACTION_HIBERNATE = 5, /* Suspend to disk */
} SessionAction;

/* -- Auto-start Entry --------------------------------------------- */

#define MAX_AUTOSTART_ENTRIES 32

typedef struct {
    char name[64];           /* Display name */
    char exec[256];          /* Command to execute */
    char args[256];          /* Arguments */
    bool enabled;            /* Whether to auto-start */
    bool terminal;           /* Run in terminal */
    int order;               /* Start order (lower = earlier) */
    char only_show_in[64];   /* Desktop env filter (e.g., "WuBuOS") */
    char not_show_in[64];    /* Desktop env exclusion */
    bool hidden;             /* Hidden from UI */
} AutostartEntry;

/* -- Session State ------------------------------------------------ */

typedef struct {
    SessionAction pending_action;
    bool action_requested;
    bool save_session;           /* Save running apps on logout */
    char session_file[256];      /* Path to session file */
    
    /* Running app tracking for session restore */
    struct {
        char app_name[64];
        char window_title[128];
        int x, y, w, h;
        int desktop;             /* Virtual desktop */
        bool maximized;
        bool minimized;
    } running_apps[32];
    int running_app_count;
} SessionState;

/* -- Session Manager API ------------------------------------------ */

/* Initialize session manager */
int  wubu_session_init(void);
void wubu_session_shutdown(void);

/* Get current session state */
const SessionState *wubu_session_state(void);

/* Request session action (queued, processed on next tick) */
void wubu_session_request_action(SessionAction action);

/* Process pending action (call from main loop) */
void wubu_session_process(void);

/* Auto-start management */
int  wubu_autostart_add(const AutostartEntry *entry);
int  wubu_autostart_remove(const char *name);
int  wubu_autostart_count(void);
const AutostartEntry *wubu_autostart_get(int index);

/* Load auto-start entries from config */
int  wubu_autostart_load(void);

/* Save auto-start entries to config */
int  wubu_autostart_save(void);

/* Run all enabled auto-start entries */
void wubu_autostart_run(void);

/* Session save/restore */
int  wubu_session_save(void);      /* Save current running apps */
int  wubu_session_restore(void);   /* Restore apps from saved session */

/* Shutdown dialog helpers */
void wubu_session_show_shutdown_dialog(void);
bool wubu_session_shutdown_dialog_visible(void);
int  wubu_session_shutdown_dialog_handle_key(uint32_t key, uint32_t mods);
void wubu_session_shutdown_dialog_render(uint32_t *fb, int fb_w, int fb_h);

/* Inhibit idle/sleep (for media playback, etc.) */
void wubu_session_inhibit_idle(const char *reason);
void wubu_session_uninhibit_idle(void);
bool wubu_session_idle_inhibited(void);

/* Power management */
int  wubu_session_can_suspend(void);
int  wubu_session_can_hibernate(void);

#endif /* WUBU_SESSION_H */