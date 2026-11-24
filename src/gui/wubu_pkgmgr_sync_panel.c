/*
 * wubu_pkgmgr_sync_panel.c -- Control Panel widget + bounded scheduled loop
 * for signed cross-device revocation-feed sync.
 *
 * The widget edits the EXISTING GUI settings key `trusted_sync_url` (via
 * wubu_settings_mut() + wubu_settings_save()). Every sync, manual or
 * scheduled, goes through wubu_pkgmgr_key_feed_sync() -- the same transport
 * and trust leaf the CLI uses -- which verifies the detached signature over
 * the exact feed bytes BEFORE the importer's single all-or-nothing DB
 * transaction. There is no crypto or DB code in this file.
 *
 * The scheduled loop is tick-driven (called from desktop_tick()), has no
 * thread and no daemon, is OFF by default, and runs at most one bounded
 * fetch per tick. Its clock is injectable so differential gates can advance
 * time deterministically. A refusal never touches the DB (fail-closed).
 */
#include "wubu_pkgmgr_sync_panel.h"
#include "wubu_settings.h"
#include "wubu_pkgmgr_revoke_sync.h"
#include "../kernel/vbe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* controlpanel_render() hands applets a content region starting at y=32. */
#define SYNC_CONTENT_Y 32

static PkSyncPanelState g_sync;
static PkSyncClockFn    g_clock;
static PkSyncJitterFn   g_jitter;
static bool             g_busy;
static unsigned         g_jitter_seed = 0x9E3779B9u;

static int64_t sync_now_time(void) { return g_clock ? g_clock() : (int64_t)time(NULL); }

void cp_pkg_sync_set_jitter_fn(PkSyncJitterFn fn) { g_jitter = fn; }

/* Built-in jitter source: a cheap deterministic PRNG. No cryptography is
 * involved -- this only de-synchronises retries, and the delay is still
 * bounded by cp_pkg_sync_backoff_delay_jittered(). */
static unsigned jitter_rand_bp(void) {
    g_jitter_seed = g_jitter_seed * 1103515245u + 12345u;
    return ((g_jitter_seed >> 8) % 10001u);
}

static void sync_seterr(char *err, size_t errsz, const char *msg) {
    if (err && errsz) {
        snprintf(err, errsz, "%s", msg ? msg : "error");
    }
}

PkSyncPanelState *cp_pkg_sync_state(void) { return &g_sync; }

void cp_pkg_sync_set_clock(PkSyncClockFn fn) { g_clock = fn; }

/* Exponential-with-cap retry schedule for the scheduled loop.  Pure: gates
 * assert this directly, and cp_pkg_sync_tick() calls the same function so the
 * asserted schedule is the one actually used.  No allocation, no clock. */
int cp_pkg_sync_backoff_delay(int initial_secs, int max_secs, int fail_count) {
    int64_t d;
    if (initial_secs < WUBU_SYNC_BACKOFF_HARD_MIN ||
        initial_secs > WUBU_SYNC_BACKOFF_HARD_MAX)
        initial_secs = WUBU_SYNC_BACKOFF_DEFAULT_INITIAL;
    if (max_secs < initial_secs) max_secs = initial_secs;
    if (max_secs > WUBU_SYNC_BACKOFF_HARD_MAX) max_secs = WUBU_SYNC_BACKOFF_HARD_MAX;
    if (fail_count <= 1) return initial_secs;
    d = initial_secs;
    for (int i = 1; i < fail_count; i++) {
        d *= 2;                       /* bounded by the loop count + cap below */
        if (d >= max_secs) return max_secs;
    }
    return (int)d;
}

int cp_pkg_sync_backoff_delay_jittered(int initial_secs, int max_secs,
                                       int fail_count, int jitter_pct,
                                       unsigned rand_bp) {
    int base = cp_pkg_sync_backoff_delay(initial_secs, max_secs, fail_count);
    int64_t mult, v;
    if (max_secs < base) max_secs = base;
    if (jitter_pct <= 0) return base;                 /* 0 == exact base */
    if (jitter_pct > WUBU_SYNC_BACKOFF_MAX_JITTER_PCT)
        jitter_pct = WUBU_SYNC_BACKOFF_MAX_JITTER_PCT;
    if (rand_bp > 10000u) rand_bp = 10000u;
    /* 1 +/- jitter_pct percent, expressed in basis points: rand_bp=0 -> (1-j),
     * rand_bp=10000 -> (1+j), rand_bp=5000 -> 1. */
    mult = 10000 + ((int64_t)rand_bp * 2 - 10000) * jitter_pct / 100;
    v = (int64_t)base * mult / 10000;
    if (v < 1) v = 1;
    if (v > max_secs) v = max_secs;
    return (int)v;
}

/* Build capability: a hosted build with an available mbedTLS dev root links
 * the HTTPS path by default (HOSTED_TLS auto-detected, no fetch); a no-TLS
 * build (HOSTED_TLS=0 or no dev root) refuses https:// up front instead of
 * opening a non-TLS socket. */
bool cp_pkg_sync_tls_capable(void) {
#ifdef WUBU_HAVE_MBEDTLS
    return true;
#else
    return false;
#endif
}

void cp_pkg_sync_button_rects(int rects[5][4]) {
    static const int xr[5] = { 10, 100, 10, 100, 134 };
    static const int yr[5] = { 76, 76, 110, 110, 110 };
    static const int wr[5] = { 84, 96, 84, 28, 28 };
    if (!rects) return;
    for (int i = 0; i < 5; i++) {
        rects[i][0] = xr[i];
        rects[i][1] = yr[i];
        rects[i][2] = wr[i];
        rects[i][3] = 24;
    }
}

void cp_pkg_sync_url_button_rects(int rects[2][4]) {
    static const int xr[2] = { 206, 260 };
    static const int yr[2] = { 76, 76 };
    static const int wr[2] = { 48, 48 };
    if (!rects) return;
    for (int i = 0; i < 2; i++) {
        rects[i][0] = xr[i];
        rects[i][1] = yr[i];
        rects[i][2] = wr[i];
        rects[i][3] = 24;
    }
}

void cp_pkg_sync_reload_from_settings(void) {
    const WubuSettings *set = wubu_settings_get();
    PkSyncPanelState *s = &g_sync;
    int n = wubu_settings_sync_url_count();
    const char *first = n > 0 ? wubu_settings_sync_url_at(0) : "";
    snprintf(s->url, sizeof(s->url), "%s", first);
    s->url_len = strlen(s->url);
    s->active_url = 0;
    s->auto_enabled = set ? set->sync_auto_enabled : false;
    s->interval_secs = set ? set->sync_interval_secs : 0;
    if (s->interval_secs < WUBU_SYNC_MIN_INTERVAL) s->interval_secs = WUBU_SYNC_MIN_INTERVAL;
    if (s->interval_secs > WUBU_SYNC_MAX_INTERVAL) s->interval_secs = WUBU_SYNC_MAX_INTERVAL;
    s->backoff_initial_secs = set ? set->sync_backoff_initial_secs
                                  : WUBU_SYNC_BACKOFF_DEFAULT_INITIAL;
    s->backoff_max_secs = set ? set->sync_backoff_max_secs
                              : WUBU_SYNC_BACKOFF_DEFAULT_MAX;
    if (s->backoff_initial_secs < WUBU_SYNC_BACKOFF_HARD_MIN ||
        s->backoff_initial_secs > WUBU_SYNC_BACKOFF_HARD_MAX)
        s->backoff_initial_secs = WUBU_SYNC_BACKOFF_DEFAULT_INITIAL;
    if (s->backoff_max_secs < s->backoff_initial_secs ||
        s->backoff_max_secs > WUBU_SYNC_BACKOFF_HARD_MAX)
        s->backoff_max_secs = (s->backoff_initial_secs > WUBU_SYNC_BACKOFF_DEFAULT_MAX)
                              ? s->backoff_initial_secs : WUBU_SYNC_BACKOFF_DEFAULT_MAX;
    s->jitter_pct = set ? set->sync_backoff_jitter_pct
                        : WUBU_SETTINGS_SYNC_BACKOFF_DEFAULT_JITTER;
    if (s->jitter_pct < 0) s->jitter_pct = 0;
    if (s->jitter_pct > WUBU_SYNC_BACKOFF_MAX_JITTER_PCT)
        s->jitter_pct = WUBU_SYNC_BACKOFF_MAX_JITTER_PCT;
    s->jitter_bp = 0;
    s->editing = false;
    s->fail_count = 0;               /* (re)load clears retry state */
    s->retry_secs = s->backoff_initial_secs;
    s->last_run = sync_now_time();   /* loading never fires an immediate run */
    snprintf(s->status, sizeof(s->status), "trusted URL loaded from settings");
    s->last_ok = true;
}

/* Persist only the loop/jitter configuration, leaving the bounded URL list
 * intact (used by the Auto toggle and interval buttons). */
static void sync_write_config(PkSyncPanelState *s, WubuSettings *set) {
    set->sync_auto_enabled = s->auto_enabled;
    set->sync_interval_secs = s->interval_secs;
    set->sync_backoff_initial_secs = s->backoff_initial_secs;
    set->sync_backoff_max_secs = s->backoff_max_secs;
    set->sync_backoff_jitter_pct = s->jitter_pct;
}

static bool sync_persist_config(PkSyncPanelState *s, char *err, size_t errsz) {
    WubuSettings *set = wubu_settings_mut();
    if (!set) { sync_seterr(err, errsz, "settings unavailable"); return false; }
    sync_write_config(s, set);
    if (wubu_settings_save() != 0) {
        sync_seterr(err, errsz, "settings save failed");
        return false;
    }
    return true;
}

/* Persist an edit of the URL field. A typed URL REPLACES the bounded list
 * (the legacy single-field gesture); use cp_pkg_sync_url_add() to append. */
static bool sync_persist(PkSyncPanelState *s, char *err, size_t errsz) {
    WubuSettings *set = wubu_settings_mut();
    const char *u = s->url;
    if (!set) { sync_seterr(err, errsz, "settings unavailable"); return false; }
    sync_write_config(s, set);
    if (wubu_settings_sync_url_set(&u, u[0] ? 1 : 0) < 0) {
        sync_seterr(err, errsz, "settings save failed");
        return false;
    }
    return true;
}

/* -- Bounded multi-URL list --------------------------------------- */

int cp_pkg_sync_url_count(void) { return wubu_settings_sync_url_count(); }

const char *cp_pkg_sync_url_at(int idx) { return wubu_settings_sync_url_at(idx); }

bool cp_pkg_sync_url_add(const char *url, char *err, size_t errsz) {
    const char *urls[WUBU_SYNC_PANEL_MAX_URLS];
    PkSyncPanelState *s = &g_sync;
    int n = wubu_settings_sync_url_count();
    if (!url || !*url || strlen(url) >= WUBU_SYNC_PANEL_URL_MAX) {
        sync_seterr(err, errsz, "empty or oversize sync url"); return false;
    }
    if (n >= (int)WUBU_SYNC_PANEL_MAX_URLS) {
        sync_seterr(err, errsz, "sync url list is full"); return false;
    }
    for (int i = 0; i < n; i++) {
        urls[i] = wubu_settings_sync_url_at(i);
        if (strcmp(urls[i], url) == 0) {
            sync_seterr(err, errsz, "sync url already trusted"); return false;
        }
    }
    urls[n] = url;
    if (wubu_settings_sync_url_set(urls, n + 1) < 0) {
        sync_seterr(err, errsz, "settings save failed"); return false;
    }
    s->active_url = n;
    snprintf(s->url, sizeof(s->url), "%s", url);
    s->url_len = strlen(s->url);
    s->editing = false;
    snprintf(s->status, sizeof(s->status), "added trusted URL (%d total)", n + 1);
    s->last_ok = true;
    return true;
}

bool cp_pkg_sync_url_remove(int idx, char *err, size_t errsz) {
    const char *urls[WUBU_SYNC_PANEL_MAX_URLS];
    PkSyncPanelState *s = &g_sync;
    int n = wubu_settings_sync_url_count();
    if (idx < 0 || idx >= n) {
        sync_seterr(err, errsz, "no such sync url"); return false;
    }
    for (int i = 0, j = 0; i < n; i++) {
        if (i == idx) continue;
        urls[j++] = wubu_settings_sync_url_at(i);
    }
    if (wubu_settings_sync_url_set(urls, n - 1) < 0) {
        sync_seterr(err, errsz, "settings save failed"); return false;
    }
    s->active_url = 0;
    snprintf(s->url, sizeof(s->url), "%s", n - 1 ? wubu_settings_sync_url_at(0) : "");
    s->url_len = strlen(s->url);
    snprintf(s->status, sizeof(s->status), "removed trusted URL (%d left)", n - 1);
    s->last_ok = true;
    return true;
}

bool cp_pkg_sync_apply_url(char *err, size_t errsz) {
    PkSyncPanelState *s = &g_sync;
    s->editing = false;
    if (!sync_persist(s, err, errsz)) {
        snprintf(s->status, sizeof(s->status), "REFUSED: settings save failed");
        s->last_ok = false;
        return false;
    }
    snprintf(s->status, sizeof(s->status), "saved trusted URL; auto=%d interval=%ds",
             s->auto_enabled ? 1 : 0, s->interval_secs);
    s->last_ok = true;
    return true;
}

bool cp_pkg_sync_sync_now(char *err, size_t errsz) {
    PkSyncPanelState *s = &g_sync;
    wubu_pkg_sync_config_t cfg;
    const char *targets[WUBU_SYNC_PANEL_MAX_URLS];
    char local_err[256], first_err[256];
    size_t applied = 0;
    int n, attempted = 0, refused = 0;

    if (g_busy) { sync_seterr(err, errsz, "sync already in progress"); return false; }
    g_busy = true;

    /* Rebuild trust from the settings store on EVERY attempt. An edited-but-
     * not-applied URL is not in the set and is refused; every applied URL in
     * the bounded list is attempted in one bounded pass. */
    wubu_pkg_sync_config_init(&cfg);
    wubu_pkg_sync_config_load(NULL, NULL, &cfg);
    n = wubu_settings_sync_url_count();
    if (n > 0) {
        for (int i = 0; i < n; i++) targets[i] = wubu_settings_sync_url_at(i);
    } else {
        targets[0] = s->url[0] ? s->url : wubu_settings_trusted_sync_url();
        n = 1;
    }
    if (n > (int)WUBU_SYNC_PANEL_MAX_URLS) n = (int)WUBU_SYNC_PANEL_MAX_URLS;

    local_err[0] = first_err[0] = '\0';
    /* An uncommitted edit must never silently sync a different URL. */
    if (s->url[0]) {
        bool committed = false;
        for (int i = 0; i < n; i++)
            if (strcmp(targets[i], s->url) == 0) { committed = true; break; }
        if (!committed) {
            snprintf(first_err, sizeof(first_err), "sync url not in trusted sync config");
        }
    }
    for (int i = 0; i < n && !first_err[0]; i++) {
        size_t got = 0;
        bool one;
        attempted++;
        local_err[0] = '\0';
        if (!cp_pkg_sync_tls_capable() && strncmp(targets[i], "https://", 8) == 0) {
            /* No-TLS build: refuse HTTPS before any socket is opened. */
            snprintf(local_err, sizeof(local_err),
                     "https:// requires TLS; build the hosted product with HOSTED_TLS=1");
            one = false;
        } else {
            one = wubu_pkgmgr_key_feed_sync(targets[i], &cfg, &got,
                                            local_err, sizeof(local_err));
        }
        if (one) applied += got;
        else {
            refused++;
            snprintf(first_err, sizeof(first_err), "%s",
                     local_err[0] ? local_err : "sync refused");
        }
    }
    g_busy = false;

    if (!first_err[0] && attempted > 0) {
        s->applied_total += (int)applied;
        snprintf(s->status, sizeof(s->status), "OK: applied %zu revocation(s) from %d URL(s)",
                 applied, attempted);
    } else {
        snprintf(s->status, sizeof(s->status), "REFUSED: %s",
                 first_err[0] ? first_err : "no trusted sync URL");
    }
    s->last_ok = (first_err[0] == '\0' && attempted > 0);
    if (err && errsz) snprintf(err, errsz, "%s", first_err);
    (void)refused;
    return s->last_ok;
}

int cp_pkg_sync_tick(void) {
    PkSyncPanelState *s = &g_sync;
    int64_t now = sync_now_time();
    char err[256];
    int delay;
    bool ok;

    if (!s->auto_enabled || s->interval_secs <= 0) return 0;

    /* Normal cadence is the configured interval.  After a refusal the loop
     * switches to the exponential backoff (initial -> 2x -> 4x -> cap). */
    delay = s->fail_count > 0 ? s->retry_secs : s->interval_secs;
    if (delay <= 0) delay = WUBU_SYNC_BACKOFF_DEFAULT_INITIAL;
    if (s->last_run != 0 &&
        (now < s->last_run || now - s->last_run < delay)) return 0;

    /* One bounded attempt per tick.  A refusal advances the deadline and
     * raises the backoff, so a bad feed cannot be hammered; a success resets
     * the backoff to the configured initial delay.  No thread, no daemon. */
    s->last_run = now;
    s->scheduled_runs++;
    ok = cp_pkg_sync_sync_now(err, sizeof(err));
    if (ok) {
        s->fail_count = 0;
        s->retry_secs = s->backoff_initial_secs;
    } else {
        unsigned bp = g_jitter ? g_jitter() : jitter_rand_bp();
        if (bp > 10000u) bp = 10000u;
        s->jitter_bp = bp;
        s->fail_count++;
        s->retry_secs = cp_pkg_sync_backoff_delay_jittered(s->backoff_initial_secs,
                                                           s->backoff_max_secs,
                                                           s->fail_count,
                                                           s->jitter_pct, bp);
    }
    return ok ? 1 : -1;
}

int cp_pkg_sync_test_line(char *out, size_t cap) {
    PkSyncPanelState *s = &g_sync;
    if (!out || cap == 0) return -1;
    snprintf(out, cap,
             "url=%s urls=%d auto=%d interval=%d initial=%d max=%d jitter=%d%% bp=%u "
             "fails=%d retry=%d manual=%d scheduled=%d applied=%d last_ok=%d status=%s",
             s->url, wubu_settings_sync_url_count(), s->auto_enabled ? 1 : 0, s->interval_secs,
             s->backoff_initial_secs, s->backoff_max_secs, s->jitter_pct, s->jitter_bp,
             s->fail_count, s->retry_secs,
             s->manual_runs, s->scheduled_runs, s->applied_total,
             s->last_ok ? 1 : 0, s->status);
    return 0;
}

/* -- Applet callbacks ------------------------------------------------ */

static void sync_init(CpApplet *applet) {
    (void)applet;
    cp_pkg_sync_reload_from_settings();
}

static void sync_render(CpApplet *applet, uint32_t *fb, int fb_w, int fb_h,
                        int x, int y, int w, int h) {
    PkSyncPanelState *s = &g_sync;
    int rects[5][4];
    (void)applet;
    (void)fb; (void)fb_w; (void)fb_h;  /* draw through vbe_* like the desktop */

    cp_pkg_sync_button_rects(rects);
    vbe_fill_rect(x, y, w, h > 0 ? h : 1, 0xFF101820u);
    vbe_draw_text(x + 10, y + 8, "Revocation Sync (signed feed)", 0xFFFFFFFFu, 1);
    vbe_draw_text(x + 10, y + 28, "Trusted URL:", 0xFF9AD0FFu, 1);
    vbe_fill_rect(x + rects[0][0] - 2, y + 46, w > 200 ? w - 20 : 180, 26, 0xFF1F2A37u);
    vbe_draw_text(x + 10, y + 52, s->url[0] ? s->url : "(none - type a URL, press Enter)",
                  0xFFFFFFFFu, 1);

    vbe_fill_rect(x + rects[0][0], y + rects[0][1], rects[0][2], rects[0][3], 0xFF2F6F3Fu);
    vbe_draw_text(x + rects[0][0] + 8, y + rects[0][1] + 6, "Save", 0xFFFFFFFFu, 1);
    vbe_fill_rect(x + rects[1][0], y + rects[1][1], rects[1][2], rects[1][3], 0xFF2F4F8Fu);
    vbe_draw_text(x + rects[1][0] + 6, y + rects[1][1] + 6, "Sync Now", 0xFFFFFFFFu, 1);

    char list_lbl[64];
    snprintf(list_lbl, sizeof(list_lbl), "URLs:%d", wubu_settings_sync_url_count());
    vbe_draw_text(x + 206, y + 40, list_lbl, 0xFF9AD0FFu, 1);
    int ur[2][4];
    cp_pkg_sync_url_button_rects(ur);
    vbe_fill_rect(x + ur[0][0], y + ur[0][1], ur[0][2], ur[0][3], 0xFF2F6F3Fu);
    vbe_draw_text(x + ur[0][0] + 12, y + ur[0][1] + 6, "Add", 0xFFFFFFFFu, 1);
    vbe_fill_rect(x + ur[1][0], y + ur[1][1], ur[1][2], ur[1][3], 0xFF4A4A4Au);
    vbe_draw_text(x + ur[1][0] + 14, y + ur[1][1] + 6, "Del", 0xFFFFFFFFu, 1);

    vbe_fill_rect(x + rects[2][0], y + rects[2][1], rects[2][2], rects[2][3],
                  s->auto_enabled ? 0xFF2F6F3Fu : 0xFF4A4A4Au);
    char auto_lbl[48];
    snprintf(auto_lbl, sizeof(auto_lbl), "Auto:%s", s->auto_enabled ? "ON" : "OFF");
    vbe_draw_text(x + rects[2][0] + 6, y + rects[2][1] + 6, auto_lbl, 0xFFFFFFFFu, 1);

    char int_lbl[48];
    snprintf(int_lbl, sizeof(int_lbl), "Every %ds", s->interval_secs);
    vbe_draw_text(x + rects[3][0] + 36, y + rects[3][1] + 6, int_lbl, 0xFFFFFFFFu, 1);
    vbe_fill_rect(x + rects[3][0], y + rects[3][1], rects[3][2], rects[3][3], 0xFF4A4A4Au);
    vbe_fill_rect(x + rects[4][0], y + rects[4][1], rects[4][2], rects[4][3], 0xFF4A4A4Au);
    vbe_draw_text(x + rects[3][0] + 9, y + rects[3][1] + 6, "-", 0xFFFFFFFFu, 1);
    vbe_draw_text(x + rects[4][0] + 9, y + rects[4][1] + 6, "+", 0xFFFFFFFFu, 1);

    char status[192];
    snprintf(status, sizeof(status), "%.160s", s->status);
    vbe_draw_text(x + 10, y + (h > 148 ? h - 24 : 146), status,
                  s->last_ok ? 0xFF9BE89Bu : 0xFFFF9B9Bu, 1);
}

static bool sync_in_rect(int px, int py, const int r[4]) {
    return px >= r[0] && px < r[0] + r[2] && py >= r[1] && py < r[1] + r[3];
}

static void sync_mouse(CpApplet *applet, int x, int y, int btn, int kind) {
    PkSyncPanelState *s = &g_sync;
    int rects[5][4], ur[2][4];
    char err[256];
    (void)applet;
    if (btn != 1 || kind != 1) return;   /* press only */
    cp_pkg_sync_button_rects(rects);
    cp_pkg_sync_url_button_rects(ur);
    int ly = y - SYNC_CONTENT_Y;         /* window -> applet content coords */
    if (sync_in_rect(x, ly, rects[0])) {
        cp_pkg_sync_apply_url(err, sizeof(err));
    } else if (sync_in_rect(x, ly, rects[1])) {
        s->manual_runs++;
        cp_pkg_sync_sync_now(err, sizeof(err));
    } else if (sync_in_rect(x, ly, ur[0])) {
        cp_pkg_sync_url_add(s->url, err, sizeof(err));
    } else if (sync_in_rect(x, ly, ur[1])) {
        cp_pkg_sync_url_remove(s->active_url, err, sizeof(err));
    } else if (sync_in_rect(x, ly, rects[2])) {
        s->auto_enabled = !s->auto_enabled;
        if (s->auto_enabled) s->last_run = sync_now_time();
        sync_persist_config(s, err, sizeof(err));
    } else if (sync_in_rect(x, ly, rects[3]) || sync_in_rect(x, ly, rects[4])) {
        int step = sync_in_rect(x, ly, rects[4]) ? 1 : -1;
        s->interval_secs += step;
        if (s->interval_secs < WUBU_SYNC_MIN_INTERVAL) s->interval_secs = WUBU_SYNC_MIN_INTERVAL;
        if (s->interval_secs > WUBU_SYNC_MAX_INTERVAL) s->interval_secs = WUBU_SYNC_MAX_INTERVAL;
        sync_persist_config(s, err, sizeof(err));
    } else if (ly >= 46 && ly < 72) {
        s->editing = true;               /* focus the URL field */
    }
}

static void sync_key(CpApplet *applet, uint32_t key, uint32_t mods) {
    PkSyncPanelState *s = &g_sync;
    char err[256];
    (void)applet; (void)mods;

    if (key == '\r' || key == '\n') {
        cp_pkg_sync_apply_url(err, sizeof(err));
        return;
    }
    if (key == 27) { cp_pkg_sync_reload_from_settings(); return; }  /* cancel edit */
    if (key == 8 || key == 0x7f) {
        if (s->url_len > 0) s->url[--s->url_len] = '\0';
        s->editing = true;
        return;
    }
    if (key >= 32 && key < 127) {
        if (s->url_len + 1 < sizeof(s->url)) {
            s->url[s->url_len++] = (char)key;
            s->url[s->url_len] = '\0';
        }
        s->editing = true;
    }
}

static void sync_cleanup(CpApplet *applet) {
    (void)applet;
    g_busy = false;
}

CpApplet cp_create_pkg_sync_applet(void) {
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_PKG_SYNC;
    snprintf(a.name, sizeof(a.name), "Revocation Sync");
    snprintf(a.desc, sizeof(a.desc), "View/edit trusted_sync_url and run signed revocation sync");
    a.icon_color = 0xFF2F6F8Fu;
    a.init = sync_init;
    a.render = sync_render;
    a.mouse = sync_mouse;
    a.key = sync_key;
    a.cleanup = sync_cleanup;
    a.user_data = NULL;
    a.initialized = false;
    return a;
}
