/* wubu_session_daemon_test.c -- P3 session-manager daemon (the role-up).
 * Composes the five contracts: owns the session-boot (bus + autostart), the
 * logind registry, and the polkit authority WIRED as the logind power gate,
 * and publishes /n/background (bus + session + auth). The test runs the whole
 * lifecycle:
 *   - register an active login session
 *   - add an auth rule granting uid 1000 power.suspend
 *   - start the XDG autostart apps through a fake launcher
 *   - publish the /n/background state and verify bus + session + auth files
 *   - power op is ALLOWED (auth grants) / DENIED (auth denies / no rule)
 */
#include "../egojar.h"
#include "wubu_session_daemon.h"
#include "wubu_logind.h"
#include "wubu_auth.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

extern const char *g_ns_root;
#define NSROOT "/tmp/wubu_session_daemon_test"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static int g_launched = 0;
static int launcher(wubu_session_boot_t *boot,
                    const wubu_xdga_entry_t *entry, char **argv, void *ud) {
    (void)boot; (void)entry; (void)argv; (void)ud;
    g_launched++;
    return 0;
}

static void write_entry(const char *dir, const char *file, const char *body) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    FILE *f = fopen(path, "w");
    if (f) { fputs(body, f); fclose(f); }
}

static int readfile(const char *path, char *buf, size_t n) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t r = fread(buf, 1, n - 1, f);
    buf[r] = '\0';
    fclose(f);
    return 0;
}

int main(void) {
    system("rm -rf " NSROOT);
    if (mkdir(NSROOT, 0755) != 0 && errno != EEXIST) return 1;
    *(const char **)&g_ns_root = NSROOT;

    /* autostart dir with 2 apps */
    char dir[] = "/tmp/wubu_sd_astart_XXXXXX";
    char *d = mkdtemp(dir);
    write_entry(d, "mintupdate.desktop",
        "[Desktop Entry]\nType=Application\nName=Mint Update\nExec=mintupdate\n");
    write_entry(d, "warpinator.desktop",
        "[Desktop Entry]\nType=Application\nName=Warpinator\nExec=warpinator\n");

    wubu_session_daemon_t *sd = wubu_session_daemon_create();
    CHECK(sd != NULL, "session daemon created");

    /* login: register the active session */
    wubu_logindd_session_t s;
    memset(&s, 0, sizeof(s));
    s.id = 1; strcpy(s.seat, "seat0");
    s.state = WUBU_SESSION_STATE_ACTIVE;
    s.type = WUBU_SESSION_TYPE_X11; strcpy(s.class_, "user"); s.uid = 1000;
    CHECK(wubu_session_daemon_add_session(sd, &s) == 0, "active session registered");

    /* auth: grant uid 1000 power.suspend */
    wubu_auth_add_rule(wubu_session_daemon_auth(sd), 1000, "power.suspend", WUBU_AUTH_ALLOW);
    CHECK(wubu_auth_rule_count(wubu_session_daemon_auth(sd)) == 1,
          "one auth rule installed (power.suspend for uid 1000)");

    /* start the session: autostart apps through the fake launcher */
    wubu_session_daemon_set_launcher(sd, launcher, NULL);
    char *dirs[1] = { d };
    int started = wubu_session_daemon_start(sd, dirs, 1);
    CHECK(started == 2, "session daemon started both autostart apps");
    CHECK(g_launched == 2, "launcher invoked for both");

    /* publish /n/background: bus + session + auth */
    CHECK(wubu_session_daemon_publish(sd) == 0, "full /n/background state published");

    char p[1024], buf[1024];

    /* bus surface */
    snprintf(p, sizeof(p), "%s/background/bus/names", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0, "bus/names published");
    CHECK(strstr(buf, "org.wubu.autostart.mintupdate") != NULL,
          "bus/names lists the started app's name");

    /* session surface */
    snprintf(p, sizeof(p), "%s/background/session/active", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0 && atoi(buf) == 1,
          "session/active == session id 1");
    snprintf(p, sizeof(p), "%s/background/session/seat", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0 && strcmp(buf, "seat0") == 0,
          "session/seat == seat0");
    snprintf(p, sizeof(p), "%s/background/session/type", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0 && strcmp(buf, "x11") == 0,
          "session/type == x11");

    /* auth surface */
    snprintf(p, sizeof(p), "%s/background/auth/rules", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0, "auth/rules published");
    CHECK(strstr(buf, "1000:power.suspend=allow") != NULL,
          "auth/rules contains the grant line");

    /* power through the wired authority */
    CHECK(wubu_session_daemon_power(sd, WUBU_POWER_SUSPEND) == 0,
          "authorized suspend allowed through the daemon power gate");
    CHECK(wubu_session_daemon_power(sd, WUBU_POWER_REBOOT) == 1,
          "reboot (no rule) denied through the authority (deny code != 0)");

    wubu_session_daemon_destroy(sd);
    snprintf(p, sizeof(p), "rm -rf %s %s", d, NSROOT);
    system(p);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
