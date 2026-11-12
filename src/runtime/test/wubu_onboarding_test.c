/* wubu_onboarding_test.c -- GAP F (docs/GAP_ROADMAP): a HOSTED END-TO-END
 * first-boot onboarding stream.
 *
 * Drives the P4 flow through the real session-boot launcher:
 *   boot #1 (first): target = GREETER -> session launches the Onboarding entry;
 *     the greeter runs and completes (persists a config signature sentinel).
 *   boot #2 (re-load same path): target = DESKTOP -> session launches the
 *     Desktop entry, NOT the greeter. The recorded signature round-trips.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "../../egojar.h"
#include "wubu_onboarding.h"
#include "wubu_session_boot.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static char g_started[8][64];
static int  g_n_started = 0;
/* ud = &int first_boot_perspective: 1 (boot#1) start only Onboarding; else
 * start only Desktop. The greeter completing flips the module state mid-loop,
 * but the captured perspective still gates the Desktop entry for boot#1. */
static int launch_cb(wubu_session_boot_t *boot,
                     const wubu_xdga_entry_t *entry, char **argv, void *ud) {
    (void)boot; (void)argv;
    int first = *(int *)ud;
    const char *nm = entry->name;
    if (first && !strcmp(nm, "Onboarding")) {
        if (g_n_started < 8) strcpy(g_started[g_n_started++], "greeter");
        /* the greeter runs: writes its config -> onboarding complete. */
        wubu_onboarding_complete("user=wubu;theme=win98");
        return 0;
    }
    if (!first && !strcmp(nm, "Desktop")) {
        if (g_n_started < 8) strcpy(g_started[g_n_started++], "desktop");
        return 0;
    }
    return 1;   /* not for this boot variation */
}

static void write_entry(const char *dir, const char *file, const char *name) {
    char path[1024]; char body[1600];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    snprintf(body, sizeof(body),
             "[Desktop Entry]\nType=Application\nName=%s\nExec=%s\n", name, name);
    FILE *f = fopen(path, "w");
    if (f) { fputs(body, f); fclose(f); }
}

int main(void) {
    char d[] = "/tmp/wubu_onboard_test_XXXXXX";
    char *dir = mkdtemp(d);
    CHECK(dir != NULL, "temp dir created");
    char autostart[1200]; char sentinel[1200];
    snprintf(autostart, sizeof(autostart), "%s/autostart", dir);
    snprintf(sentinel, sizeof(sentinel), "%s/onboarded", dir);
    mkdir(autostart, 0755);
    write_entry(autostart, "onboarding.desktop", "Onboarding");
    write_entry(autostart, "desktop.desktop", "Desktop");

    /* ---- boot #1: first boot -> GREETER ---- */
    wubu_onboarding_load(sentinel);
    CHECK(wubu_onboarding_first_boot() == 1, "first boot detected (no sentinel)");
    CHECK(wubu_onboarding_target() == WUBU_ONBOARD_GREETER,
          "first boot targets the GREETER");

    int first = 1;
    char *dirs[1]; dirs[0] = autostart;
    wubu_session_boot_t *b1 = wubu_session_boot_create();
    CHECK(b1 != NULL, "boot#1 session created");
    wubu_session_boot_scan(b1, dirs, 1);
    wubu_session_boot_set_launcher(b1, launch_cb, &first);
    int started = wubu_session_boot_start_all(b1);
    CHECK(started >= 1, "boot#1 launched the onboarding greeter");
    wubu_session_boot_destroy(b1);
    CHECK(g_n_started == 1 && !strcmp(g_started[0], "greeter"),
          "first boot launched ONLY the greeter (desktop gated)");
    CHECK(wubu_onboarding_first_boot() == 0,
          "greeter completed -> onboarding persisted (no longer first boot)");

    /* ---- boot #2: re-load the SAME sentinel -> DESKTOP ---- */
    g_n_started = 0;
    wubu_onboarding_load(sentinel);           /* fresh load reads the sentinel */
    CHECK(wubu_onboarding_first_boot() == 0, "second boot sees a completed onboarding");
    CHECK(wubu_onboarding_target() == WUBU_ONBOARD_DESKTOP,
          "second boot targets the DESKTOP");
    CHECK(!strcmp(wubu_onboarding_signature(), "user=wubu;theme=win98"),
          "the greeter's config signature round-tripped");

    int second = 0;
    wubu_session_boot_t *b2 = wubu_session_boot_create();
    CHECK(b2 != NULL, "boot#2 session created");
    wubu_session_boot_scan(b2, dirs, 1);
    wubu_session_boot_set_launcher(b2, launch_cb, &second);
    int s2 = wubu_session_boot_start_all(b2);
    CHECK(s2 >= 1, "boot#2 launched the desktop");
    wubu_session_boot_destroy(b2);
    CHECK(g_n_started == 1 && !strcmp(g_started[0], "desktop"),
          "second boot launched ONLY the desktop (greeter not relaunched)");

    char cmd[2400];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", d);
    system(cmd);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
