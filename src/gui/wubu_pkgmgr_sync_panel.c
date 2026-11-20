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
static bool             g_busy;

static int64_t sync_now_time(void) { return g_clock ? g_clock() : (int64_t)time(NULL); }

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

/* Compile-time build capability.  The default hosted build has no mbedTLS, so
 * the widget refuses https:// up front instead of opening a non-TLS socket. */
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

void cp_pkg_sync_reload_from_settings(void) {
    const WubuSettings *set = wubu_settings_get();
    PkSyncPanelState *s = &g_sync;
    snprintf(s->url, sizeof(s->url), "%s", set ? set->trusted_sync_url : "");
    s->url_len = strlen(s->url);
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
    s->editing = false;
    s->fail_count = 0;               /* (re)load clears retry state */
    s->retry_secs = s->backoff_initial_secs;
    s->last_run = sync_now_time();   /* loading never fires an immediate run */
    snprintf(s->status, sizeof(s->status), "trusted URL loaded from settings");
    s->last_ok = true;
}

/* Persist the edited URL AND the loop configuration through the one settings
 * object, then save. Writing a value without saving/applied through the
 * settings object would be decorative (two sources of truth). */
static bool sync_persist(PkSyncPanelState *s, char *err, size_t errsz) {
    WubuSettings *set = wubu_settings_mut();
    if (!set) { sync_seterr(err, errsz, "settings unavailable"); return false; }
    snprintf(set->trusted_sync_url, sizeof(set->trusted_sync_url), "%s", s->url);
    set->sync_auto_enabled = s->auto_enabled;
    set->sync_interval_secs = s->interval_secs;
    set->sync_backoff_initial_secs = s->backoff_initial_secs;
    set->sync_backoff_max_secs = s->backoff_max_secs;
    if (wubu_settings_save() != 0) {
        sync_seterr(err, errsz, "settings save failed");
        return false;
    }
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
    char local_err[256];
    size_t applied = 0;
    const char *target;
    bool ok;

    if (g_busy) { sync_seterr(err, errsz, "sync already in progress"); return false; }
    g_busy = true;

    /* Rebuild trust from the settings store on EVERY attempt. A URL edited
     * but not applied is not in the set and is refused; a rotated feed URL
     * applied later is picked up by the next scheduled tick. */
    wubu_pkg_sync_config_init(&cfg);
    wubu_pkg_sync_config_load(NULL, NULL, &cfg);
    target = s->url[0] ? s->url : wubu_settings_trusted_sync_url();

    local_err[0] = '\0';
    if (!cp_pkg_sync_tls_capable() && strncmp(target, "https://", 8) == 0) {
        /* Config gate: a no-TLS build refuses HTTPS before any network I/O. */
        snprintf(local_err, sizeof(local_err),
                 "https:// requires TLS; build the hosted product with HOSTED_TLS=1");
        ok = false;
    } else {
        ok = wubu_pkgmgr_key_feed_sync(target, &cfg, &applied,
                                       local_err, sizeof(local_err));
    }
    g_busy = false;

    if (ok) {
        s->applied_total += (int)applied;
        snprintf(s->status, sizeof(s->status), "OK: applied %zu revocation(s)", applied);
    } else {
        snprintf(s->status, sizeof(s->status), "REFUSED: %s",
                 local_err[0] ? local_err : "sync refused");
    }
    s->last_ok = ok;
    if (err && errsz) snprintf(err, errsz, "%s", local_err);
    return ok;
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
        s->fail_count++;
        s->retry_secs = cp_pkg_sync_backoff_delay(s->backoff_initial_secs,
                                                  s->backoff_max_secs,
                                                  s->fail_count);
    }
    return ok ? 1 : -1;
}

int cp_pkg_sync_test_line(char *out, size_t cap) {
    PkSyncPanelState *s = &g_sync;
    if (!out || cap == 0) return -1;
    snprintf(out, cap,
             "url=%s auto=%d interval=%d initial=%d max=%d fails=%d retry=%d "
             "manual=%d scheduled=%d applied=%d last_ok=%d status=%s",
             s->url, s->auto_enabled ? 1 : 0, s->interval_secs,
             s->backoff_initial_secs, s->backoff_max_secs,
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

static void sync_render(CpApplet *applet, uint32_t *fb, int x, int y, int w, int h) {
    PkSyncPanelState *s = &g_sync;
    int rects[5][4];
    (void)applet;
    (void)fb;  /* draw through vbe_* like the rest of the desktop */

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
    int rects[5][4];
    char err[256];
    (void)applet;
    if (btn != 1 || kind != 1) return;   /* press only */
    cp_pkg_sync_button_rects(rects);
    int ly = y - SYNC_CONTENT_Y;         /* window -> applet content coords */
    if (sync_in_rect(x, ly, rects[0])) {
        cp_pkg_sync_apply_url(err, sizeof(err));
    } else if (sync_in_rect(x, ly, rects[1])) {
        s->manual_runs++;
        cp_pkg_sync_sync_now(err, sizeof(err));
    } else if (sync_in_rect(x, ly, rects[2])) {
        s->auto_enabled = !s->auto_enabled;
        if (s->auto_enabled) s->last_run = sync_now_time();
        cp_pkg_sync_apply_url(err, sizeof(err));
    } else if (sync_in_rect(x, ly, rects[3]) || sync_in_rect(x, ly, rects[4])) {
        int step = sync_in_rect(x, ly, rects[4]) ? 1 : -1;
        s->interval_secs += step;
        if (s->interval_secs < WUBU_SYNC_MIN_INTERVAL) s->interval_secs = WUBU_SYNC_MIN_INTERVAL;
        if (s->interval_secs > WUBU_SYNC_MAX_INTERVAL) s->interval_secs = WUBU_SYNC_MAX_INTERVAL;
        cp_pkg_sync_apply_url(err, sizeof(err));
    } else if (sync_in_rect(x, ly, rects[0]) == false && ly >= 46 && ly < 72) {
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
