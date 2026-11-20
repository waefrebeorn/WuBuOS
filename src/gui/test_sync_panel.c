/* test_sync_panel.c -- hosted GUI differential gate for the revocation-sync
 * Control Panel widget.
 *
 * Drives the REAL applet callbacks (key/mouse), not a copy of its logic:
 *   - types a trusted loopback URL into the widget and presses Enter (Save);
 *   - clicks "Sync Now" and asserts keys revoked AND persisted after a fresh
 *     pkgmgr load;
 *   - types a bad-scheme URL and a bad-signature URL, clicks Sync, and
 *     asserts REFUSED with no DB change;
 *   - enables the bounded scheduled loop + interval through the widget,
 *     injects a clock, rotates the signed feed at the SAME URL, and asserts
 *     the next tick applies the rotated feed.
 *
 * The whole desktop draws through vbe_*, so a render assertion reads vbe_
 * pixels after vbe_swap(). All URLs/keys come from the gate script; nothing
 * is hardcoded here.
 */
#include "wubu_pkgmgr_sync_panel.h"
#include "wubu_settings.h"
#include "wubu_pkgmgr.h"
#include "../kernel/vbe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_checks, g_fails;
#define CHECK(c, msg) do { \
    g_checks++; \
    if (c) { printf("  PASS %s\n", msg); } \
    else   { printf("  FAIL %s\n", msg); g_fails++; } \
} while (0)

static const char *env_or(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return (v && *v) ? v : fallback;
}

static int64_t g_fake;
static int64_t fake_clock(void) { return g_fake; }

static void type_url(CpApplet *a, const char *url) {
    PkSyncPanelState *s = cp_pkg_sync_state();
    /* Real GUI: clear the field before typing a new value (Backspace). */
    while (s->url_len > 0) a->key(a, 8, 0);
    for (const unsigned char *p = (const unsigned char *)url; *p; ++p)
        a->key(a, (uint32_t)*p, 0);
    a->key(a, '\r', 0);   /* Enter = Save */
}

static void click_button(CpApplet *a, int idx) {
    int r[5][4];
    cp_pkg_sync_button_rects(r);
    /* controlpanel_render places applet content at y=32; mouse coords are
     * window coords, so add the content origin. */
    a->mouse(a, r[idx][0] + 2, r[idx][1] + 2 + 32, 1, 1);
}

static void rotate_feed(const char *dir) {
    char live[1024], livesig[1024], next[1024], nextsig[1024];
    snprintf(live, sizeof(live), "%s/live", dir);
    snprintf(livesig, sizeof(livesig), "%s/live.sig", dir);
    snprintf(next, sizeof(next), "%s/.next", dir);
    snprintf(nextsig, sizeof(nextsig), "%s/.nextsig", dir);
    unlink(next); unlink(nextsig);
    /* Rotate BOTH the feed and its detached signature sidecar. */
    if (symlink("feed_b", next) == 0) (void)rename(next, live);
    if (symlink("feed_b.sig", nextsig) == 0) (void)rename(nextsig, livesig);
}

static bool settings_persisted(const char *needle) {
    char path[1024];
    const char *xdg = getenv("XDG_CONFIG_HOME");
    snprintf(path, sizeof(path), "%s/wubu/settings.conf", xdg ? xdg : "/tmp");
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    return strstr(buf, needle) != NULL;
}

int main(void) {
    const char *db     = env_or("WUBU_SYNC_DB", NULL);
    const char *admin  = env_or("WUBU_SYNC_ADMIN", NULL);
    const char *url    = env_or("WUBU_SYNC_URL", NULL);
    const char *badsig = env_or("WUBU_SYNC_BADSIG_URL", NULL);
    const char *badscheme = env_or("WUBU_SYNC_BAD_URL", NULL);
    const char *serve  = env_or("WUBU_SYNC_SERVE_DIR", NULL);
    const char *k1     = env_or("WUBU_SYNC_KEY1", NULL);
    const char *k2     = env_or("WUBU_SYNC_KEY2", NULL);
    const char *k3     = env_or("WUBU_SYNC_KEY3", NULL);
    const char *kt     = env_or("WUBU_SYNC_TAMPER_KEY", NULL);
    char line[1024], err[256];

    if (!db || !admin || !url || !badsig || !badscheme || !serve ||
        !k1 || !k2 || !k3 || !kt) {
        fprintf(stderr, "test_sync_panel: missing WUBU_SYNC_* environment\n");
        return 2;
    }
    unsetenv("WUBU_PKGMGR_SYNC_URLS");   /* trust must come from the widget/settings only */

    if (wubu_settings_init() != 0) { fprintf(stderr, "settings init failed\n"); return 2; }

    wubu_pkgmgr_config_t cfg;
    wubu_pkgmgr_get_default_config(&cfg);
    snprintf(cfg.db_path, sizeof(cfg.db_path), "%s", db);
    snprintf(cfg.cache_dir, sizeof(cfg.cache_dir), "%s/cache", db);
    snprintf(cfg.install_prefix, sizeof(cfg.install_prefix), "%s/prefix", db);
    snprintf(cfg.repo_config_dir, sizeof(cfg.repo_config_dir), "%s/repos", db);
    CHECK(wubu_pkgmgr_init(&cfg), "pkgmgr DB initialized");
    CHECK(wubu_pkgmgr_key_admin_bootstrap(admin), "admin key bootstrapped");

    /* Widget under test: the real CpApplet callbacks. */
    CpApplet a = cp_create_pkg_sync_applet();
    if (a.init) a.init(&a);
    CHECK(strcmp(a.name, "Revocation Sync") == 0, "control-panel applet registered");
    CHECK(cp_pkg_sync_state()->auto_enabled == false, "scheduled sync OFF by default");
    CHECK(cp_pkg_sync_state()->interval_secs == 0, "interval 0 by default (no daemon)");

    /* Render through vbe_* and read the pixels back after swap. */
    CHECK(vbe_init(640, 400) == 0, "vbe_init for widget render");
    if (a.render) a.render(&a, NULL, 0, 32, 640, 368);
    vbe_swap();
    CHECK(vbe_get_pixel(620, 390) == 0xFF101820u, "widget renders a real panel background");

    /* -- Drive the widget: type URL + Enter (Save) ------------------ */
    type_url(&a, url);
    CHECK(strcmp(cp_pkg_sync_state()->url, url) == 0, "widget URL field holds typed URL");
    CHECK(strcmp(wubu_settings_get()->trusted_sync_url, url) == 0,
          "Save writes trusted_sync_url into GUI settings");
    CHECK(settings_persisted("trusted_sync_url"), "settings.conf persisted the URL key");

    /* Settings survive a fresh process-like reload. */
    wubu_settings_shutdown();
    CHECK(wubu_settings_init() == 0, "settings reload after save");
    CHECK(strcmp(wubu_settings_get()->trusted_sync_url, url) == 0,
          "URL survives settings save/load round-trip");
    cp_pkg_sync_reload_from_settings();

    /* -- Manual sync through the Sync Now button -------------------- */
    click_button(&a, 1);
    CHECK(cp_pkg_sync_state()->manual_runs == 1, "Sync Now click dispatched once");
    CHECK(cp_pkg_sync_state()->last_ok, "signed feed sync accepted");
    cp_pkg_sync_test_line(line, sizeof(line));
    printf("  [widget] %s\n", line);
    wubu_pkgmgr_shutdown();
    CHECK(wubu_pkgmgr_init(&cfg), "pkgmgr reload from DB");
    CHECK(wubu_pkgmgr_key_is_revoked(k1), "key1 revoked after sync");
    CHECK(wubu_pkgmgr_key_is_revoked(k2), "key2 revoked after sync");
    CHECK(wubu_pkgmgr_key_revoked_count() == 2, "two keys revoked and persisted");

    /* -- Bad URL scheme refused, no DB change ----------------------- */
    int before = wubu_pkgmgr_key_revoked_count();
    type_url(&a, badscheme);
    click_button(&a, 1);
    CHECK(!cp_pkg_sync_state()->last_ok, "bad-scheme URL refused");
    cp_pkg_sync_test_line(line, sizeof(line));
    CHECK(strstr(line, "last_ok=0") != NULL, "bad URL shown as refused in widget status");
    wubu_pkgmgr_shutdown(); wubu_pkgmgr_init(&cfg);
    CHECK(wubu_pkgmgr_key_revoked_count() == before, "bad URL: no DB change");

    /* -- Bad signature refused, no DB change ------------------------ */
    type_url(&a, badsig);
    click_button(&a, 1);
    CHECK(!cp_pkg_sync_state()->last_ok, "bad-signature feed refused");
    wubu_pkgmgr_shutdown(); wubu_pkgmgr_init(&cfg);
    CHECK(wubu_pkgmgr_key_revoked_count() == before, "bad signature: no DB change");
    CHECK(!wubu_pkgmgr_key_is_revoked(kt), "tamper target key not revoked");

    /* -- Restore trusted URL, configure scheduled loop via widget ---- */
    type_url(&a, url);
    CHECK(wubu_settings_get()->sync_auto_enabled == false, "auto still OFF before toggle");
    click_button(&a, 2);   /* Auto ON */
    click_button(&a, 4);   /* interval + -> 1 second */
    CHECK(cp_pkg_sync_state()->auto_enabled, "Auto toggle enables scheduled loop");
    CHECK(cp_pkg_sync_state()->interval_secs == 1, "interval configured to 1s via widget");
    CHECK(settings_persisted("sync_auto_enabled"), "auto flag persisted");
    CHECK(settings_persisted("sync_interval_secs"), "interval persisted");

    /* -- Bounded scheduled loop: rotate the signed feed at same URL -- */
    g_fake = 1000;
    cp_pkg_sync_set_clock(fake_clock);
    cp_pkg_sync_reload_from_settings();   /* last_run = 1000 */
    CHECK(cp_pkg_sync_tick() == 0, "no scheduled run before interval elapses");
    rotate_feed(serve);
    g_fake = 1002;
    CHECK(cp_pkg_sync_tick() == 1, "scheduled run applies rotated signed feed after interval");
    CHECK(cp_pkg_sync_state()->scheduled_runs == 1, "exactly one bounded scheduled attempt");
    cp_pkg_sync_test_line(line, sizeof(line));
    printf("  [widget] %s\n", line);
    wubu_pkgmgr_shutdown(); wubu_pkgmgr_init(&cfg);
    CHECK(wubu_pkgmgr_key_is_revoked(k3), "rotated feed key revoked by scheduled loop");
    CHECK(wubu_pkgmgr_key_revoked_count() == 3, "scheduled sync added exactly one revocation");

    cp_pkg_sync_set_clock(NULL);
    if (a.cleanup) a.cleanup(&a);
    wubu_pkgmgr_shutdown();
    wubu_settings_shutdown();

    printf("=== test_sync_panel: %d checks, %d failures ===\n", g_checks, g_fails);
    (void)err;
    return g_fails == 0 ? 0 : 1;
}
