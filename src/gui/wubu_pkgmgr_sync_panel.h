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
#include "wubu_settings.h"   /* backoff defaults live in the settings schema */

#define WUBU_SYNC_PANEL_URL_MAX 512
#define WUBU_SYNC_MIN_INTERVAL  0
#define WUBU_SYNC_MAX_INTERVAL  86400

/* Bounded retry backoff for the scheduled loop.  The first two are the
 * configurable settings (persisted); the last two bound them so a corrupt
 * settings file cannot turn a retry into a busy loop. */
#define WUBU_SYNC_BACKOFF_DEFAULT_INITIAL WUBU_SETTINGS_SYNC_BACKOFF_DEFAULT_INITIAL
#define WUBU_SYNC_BACKOFF_DEFAULT_MAX     WUBU_SETTINGS_SYNC_BACKOFF_DEFAULT_MAX
#define WUBU_SYNC_BACKOFF_HARD_MIN        1
#define WUBU_SYNC_BACKOFF_HARD_MAX        86400
#define WUBU_SYNC_BACKOFF_MAX_JITTER_PCT  100
#define WUBU_SYNC_PANEL_MAX_URLS          WUBU_SETTINGS_TRUSTED_SYNC_URLS_MAX

typedef int64_t (*PkSyncClockFn)(void);
/* Jitter source returns basis points in [0,10000]; injectable so gates can
 * assert exact bounds without sleeping or depending on a PRNG seed. */
typedef unsigned (*PkSyncJitterFn)(void);

typedef struct {
    char   url[WUBU_SYNC_PANEL_URL_MAX]; /* the edited (uncommitted) URL field */
    size_t url_len;
    int    active_url;     /* index of the currently selected list entry */
    bool   editing;        /* URL field has keyboard focus */
    bool   auto_enabled;   /* scheduled loop ON (persisted, default OFF) */
    int    interval_secs;  /* seconds between scheduled runs (persisted) */
    int    backoff_initial_secs; /* first retry delay after a refusal (persisted) */
    int    backoff_max_secs;     /* retry-delay cap (persisted) */
    int    jitter_pct;           /* symmetric +/- jitter percent (persisted) */
    unsigned jitter_bp;          /* last sampled jitter (basis points, observability) */
    int    fail_count;     /* consecutive scheduled refusals (backoff state) */
    int    retry_secs;     /* effective delay after the last refusal */
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

/* Exponential retry delay: min(initial * 2^(fail_count-1), max), with
 * fail_count <= 1 returning `initial`.  Pure so gates can assert the exact
 * schedule; the tick uses the same function. */
int cp_pkg_sync_backoff_delay(int initial_secs, int max_secs, int fail_count);

/* Jittered delay: base as above, multiplied by 1 +/- jitter_pct percent
 * using `rand_bp` in [0,10000], clamped to [1, max_secs]. Pure, no clock.
 * The tick calls this with the injected (or default) jitter source. */
int cp_pkg_sync_backoff_delay_jittered(int initial_secs, int max_secs,
                                       int fail_count, int jitter_pct,
                                       unsigned rand_bp);
/* Inject the jitter source (NULL restores the built-in PRNG). */
void cp_pkg_sync_set_jitter_fn(PkSyncJitterFn fn);

/* Bounded multi-URL list (the settings CSV is the persisted form). */
int         cp_pkg_sync_url_count(void);
const char *cp_pkg_sync_url_at(int idx);
/* Append one URL to the bounded list and persist; false if full/invalid. */
bool cp_pkg_sync_url_add(const char *url, char *err, size_t errsz);
/* Remove one URL (by index) and persist; false if the index is invalid. */
bool cp_pkg_sync_url_remove(int idx, char *err, size_t errsz);

/* True when this build can do TLS (WUBU_HAVE_MBEDTLS).  The widget refuses
 * https:// URLs when false instead of attempting a no-TLS connection. */
bool cp_pkg_sync_tls_capable(void);

/* Shared button geometry, in APPLET-LOCAL coordinates, ordered:
 * [0] Save, [1] Sync Now, [2] Auto toggle, [3] interval -, [4] interval +.
 * Renderer AND hit-test call this so the clickable area is the drawn area. */
void cp_pkg_sync_button_rects(int rects[5][4]);

/* Extra list-management buttons [0]=Add current URL, [1]=Remove active URL,
 * in APPLET-LOCAL coordinates. Kept separate so the original five-button
 * geometry and its gates stay unchanged. */
void cp_pkg_sync_url_button_rects(int rects[2][4]);

/* One-line text view of the widget (test + accessibility surface). */
int cp_pkg_sync_test_line(char *out, size_t cap);

/* Widget actions; the key/mouse handlers call exactly these. */
bool cp_pkg_sync_apply_url(char *err, size_t errsz);  /* persist URL + loop config */
bool cp_pkg_sync_sync_now(char *err, size_t errsz);   /* one bounded signed sync */

/* Bounded scheduled-loop tick. Returns 1 if a sync ran and succeeded,
 * 0 if no run was due (or the loop is disabled), -1 if a run was refused. */
int cp_pkg_sync_tick(void);

#endif /* WUBU_PKGMGR_SYNC_PANEL_H */
