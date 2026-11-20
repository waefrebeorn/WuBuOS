/*
 * wubu_pkgmgr_sync_panel.h -- Control Panel widget for signed revocation sync.
 *
 * This is the GUI face of the bounded, fail-closed sync leaf in
 * wubu_pkgmgr_revoke_sync.c. It owns NO crypto and NO DB code: editing the
 * URL writes the existing GUI settings key `trusted_sync_url`, and every sync
 * (manual OR scheduled) goes through wubu_pkgmgr_key_feed_sync(), which
 * re-verifies the detached signature before its single atomic import.
 *
 * Scheduled sync is a tick-driven loop, never a daemon: it is OFF by default,
 * has a configurable interval (seconds), performs at most one bounded fetch
 * per tick, and fails closed (a refusal leaves the DB untouched). The host
 * clock is injectable so a gate can advance time without sleeping.
 */
#ifndef WUBU_PKGMGR_SYNC_PANEL_H
#define WUBU_PKGMGR_SYNC_PANEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "controlpanel.h"

#define WUBU_SYNC_PANEL_URL_MAX 512
#define WUBU_SYNC_MIN_INTERVAL  0
#define WUBU_SYNC_MAX_INTERVAL  86400

typedef int64_t (*PkSyncClockFn)(void);

typedef struct {
    char   url[WUBU_SYNC_PANEL_URL_MAX]; /* the edited (uncommitted) URL field */
    size_t url_len;
    bool   editing;        /* URL field has keyboard focus */
    bool   auto_enabled;   /* scheduled loop ON (persisted, default OFF) */
    int    interval_secs;  /* seconds between scheduled runs (persisted) */
    int64_t last_run;      /* clock reading of the last scheduled attempt */
    int    manual_runs;    /* Sync Now clicks (instrumentation for gates) */
    int    scheduled_runs; /* scheduled attempts (instrumentation) */
    int    applied_total;  /* entries applied by widget-driven syncs */
    bool   last_ok;
    char   status[192];    /* last human-readable result, safe for rendering */
} PkSyncPanelState;

/* Factory for the Control Panel applet registry. */
CpApplet cp_create_pkg_sync_applet(void);

/* Shared widget state (one engine instance, like the other applets). */
PkSyncPanelState *cp_pkg_sync_state(void);

/* Reset the widget from the persisted settings store (call after init/load). */
void cp_pkg_sync_reload_from_settings(void);

/* Inject the clock used by the scheduled loop (NULL restores time(NULL)). */
void cp_pkg_sync_set_clock(PkSyncClockFn fn);

/* Shared button geometry, in APPLET-LOCAL coordinates, ordered:
 * [0] Save, [1] Sync Now, [2] Auto toggle, [3] interval -, [4] interval +.
 * Renderer AND hit-test call this so the clickable area is the drawn area. */
void cp_pkg_sync_button_rects(int rects[5][4]);

/* One-line text view of the widget (test + accessibility surface). */
int cp_pkg_sync_test_line(char *out, size_t cap);

/* Widget actions; the key/mouse handlers call exactly these. */
bool cp_pkg_sync_apply_url(char *err, size_t errsz);  /* persist URL + loop config */
bool cp_pkg_sync_sync_now(char *err, size_t errsz);   /* one bounded signed sync */

/* Bounded scheduled-loop tick. Returns 1 if a sync ran and succeeded,
 * 0 if no run was due (or the loop is disabled), -1 if a run was refused. */
int cp_pkg_sync_tick(void);

#endif /* WUBU_PKGMGR_SYNC_PANEL_H */
