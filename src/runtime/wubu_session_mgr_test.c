/* wubu_session_mgr_test.c -- P3 SESSION-BOOT GLUE integration test.
 * Boots the whole /n/background rich runtime in one call: session daemon
 * (bus + logind + polkit wired as the power gate), default power policy,
 * XDG autostart via a fake launcher, de-surface registration, and the FULL
 * /n/background publish. Verifies the control plane is observable end to end.
 */
#include "../egojar.h"
#include "wubu_session_mgr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

extern const char *g_ns_root;
#define NSROOT "/tmp/wubu_session_mgr_test"

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
    if (mkdir(NSROOT, 0755) != 0) return 1;
    *(const char **)&g_ns_root = NSROOT;

    char dir[] = "/tmp/wubu_bg_astart_XXXXXX";
    char *d = mkdtemp(dir);
    write_entry(d, "mintupdate.desktop",
        "[Desktop Entry]\nType=Application\nName=Mint Update\nExec=mintupdate\n");

    char *dl[1] = { d };
    wubu_session_mgr_t *bg = wubu_session_mgr_boot(dl, 1, launcher, NULL);
    CHECK(bg != NULL, "background booted (one-call bring-up)");
    if (!bg) return 1;

    /* autostart apps launched through the injectable launcher */
    CHECK(g_launched == 1, "the XDG autostart app was launched");

    /* register desktop + tray owners */
    CHECK(wubu_session_mgr_register_desktop(bg, "nemo-desktop") == 0,
          "nemo-desktop registered as desktop owner");
    CHECK(wubu_session_mgr_register_tray(bg, "battery", "wubu-tray") == 0,
          "battery tray slot claimed by wubu-tray");

    /* publish the whole tree */
    CHECK(wubu_session_daemon_publish(wubu_session_mgr_daemon(bg)) == 0,
          "/n/background published (daemon)");
    CHECK(wubu_ns_publish_deskenv(wubu_session_mgr_deskenv(bg)) == 0,
          "/n/background/de published");

    char p[1024], buf[1024];

    /* autostart app owned its bus name */
    snprintf(p, sizeof(p), "%s/background/bus/names", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0 &&
          strstr(buf, "org.wubu.autostart.mintupdate"),
          "bus/names lists the autostart app's well-known name");

    /* session active + seat */
    snprintf(p, sizeof(p), "%s/background/session/seat", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0 && strcmp(buf, "seat0") == 0,
          "session/seat == seat0");

    /* auth policy published: suspend=allow for uid 1000 */
    snprintf(p, sizeof(p), "%s/background/auth/rules", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0 &&
          strstr(buf, "1000:power.suspend=allow"),
          "auth/rules contains the suspend grant");

    /* DE surface */
    snprintf(p, sizeof(p), "%s/background/de/desktop", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0 && strcmp(buf, "nemo-desktop") == 0,
          "de/desktop == nemo-desktop");
    snprintf(p, sizeof(p), "%s/background/de/tray", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0 &&
          strstr(buf, "battery wubu-tray"),
          "de/tray lists the battery slot owner");

    /* power gating flows through the wired authority */
    CHECK(wubu_session_daemon_power(wubu_session_mgr_daemon(bg),
                                    WUBU_POWER_SUSPEND) == 0,
          "suspend allowed (auth grant)");
    int rc = wubu_session_daemon_power(wubu_session_mgr_daemon(bg),
                                       WUBU_POWER_REBOOT);
    CHECK(rc != 0, "reboot needs an agent (interactive rule, none set) -> denied");

    wubu_session_mgr_shutdown(bg);
    snprintf(p, sizeof(p), "rm -rf %s %s", d, NSROOT);
    system(p);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
