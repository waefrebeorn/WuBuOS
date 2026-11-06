/*
 * wubu_session_boot.c -- P3 session-boot orchestrator.
 *
 * Ties wubu_xdg_autostart (scan) + wubu_bus (ownership/addressability) into
 * the cinnamon-session analogue: scan the autostart dirs, claim a well-known
 * bus name per launchable app, and launch it through an injectable hook.
 * C11, runtime host, single-threaded.
 */
#include "wubu_session_boot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WUBU_SBOOT_MAX WUBU_XDGA_MAX_ENTRY

struct wubu_session_boot {
    wubu_bus_t *bus;
    wubu_sboot_launch_fn launch;
    void *launch_ud;
    wubu_xdga_entry_t entries[WUBU_SBOOT_MAX];
    int n_entries;
};

wubu_session_boot_t *wubu_session_boot_create(void) {
    wubu_session_boot_t *b = (wubu_session_boot_t *)calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->bus = wubu_bus_create();
    if (!b->bus) { free(b); return NULL; }
    return b;
}

void wubu_session_boot_destroy(wubu_session_boot_t *boot) {
    if (!boot) return;
    if (boot->bus) wubu_bus_destroy(boot->bus);
    free(boot);
}

void wubu_session_boot_set_launcher(wubu_session_boot_t *boot,
                                    wubu_sboot_launch_fn fn, void *ud) {
    boot->launch = fn;
    boot->launch_ud = ud;
}

wubu_bus_t *wubu_session_boot_bus(wubu_session_boot_t *boot) {
    return boot ? boot->bus : NULL;
}

int wubu_session_boot_scan(wubu_session_boot_t *boot,
                           char *const *dirs, int n_dirs) {
    if (!boot) return -1;
    boot->n_entries = wubu_xdga_scan_paths(dirs, n_dirs,
                                           boot->entries, WUBU_SBOOT_MAX);
    return boot->n_entries;
}

/* derive a stable well-known bus name from the .desktop basename:
 * "mintupdate.desktop" -> "org.wubu.autostart.mintupdate".
 * Rejects names with characters invalid in a bus name (space, etc.). */
static int entry_busname(const wubu_xdga_entry_t *e, char *out, size_t outsz) {
    if (!e || !e->path[0]) return -1;
    const char *base = strrchr(e->path, '/');
    base = base ? base + 1 : e->path;
    /* strip the .desktop extension */
    static const char *sfx = ".desktop";
    size_t blen = strlen(base);
    if (blen > strlen(sfx) && strcmp(base + blen - strlen(sfx), sfx) == 0)
        blen -= strlen(sfx);
    if (blen == 0) return -1;
    size_t need = strlen("org.wubu.autostart.") + blen + 1;
    if (need > outsz) return -1;
    snprintf(out, outsz, "org.wubu.autostart.%.*s", (int)blen, base);
    return 0;
}

int wubu_session_boot_start_all(wubu_session_boot_t *boot) {
    if (!boot || !boot->launch || boot->n_entries <= 0) return -1;
    int started = 0;
    for (int i = 0; i < boot->n_entries; i++) {
        wubu_xdga_entry_t *e = &boot->entries[i];

        /* claim the well-known name for this app on the session bus */
        char bname[WUBU_BUS_MAX_NAME];
        int32_t conn = wubu_bus_connect(boot->bus);
        if (conn < 0) continue;
        if (entry_busname(e, bname, sizeof(bname)) != 0) {
            wubu_bus_disconnect(boot->bus, conn);
            continue;
        }
        /* a fresh conn claims so the app is addressable as its bus name even
         * before the launcher returns; on launcher failure the conn drops. */
        wubu_bus_request_name(boot->bus, conn, bname);

        char *argv[65];
        int na = wubu_xdga_build_argv(e, argv);
        if (na <= 0) {
            wubu_bus_disconnect(boot->bus, conn);
            continue;
        }
        int ok = boot->launch(boot, e, argv, boot->launch_ud);
        for (int j = 0; j < na; j++) free(argv[j]);
        if (ok == 0) { started++; continue; }

        /* launcher refused -> release the name + drop the conn */
        wubu_bus_release_name(boot->bus, conn, bname);
        wubu_bus_disconnect(boot->bus, conn);
    }
    return started;
}

int wubu_session_boot_count(const wubu_session_boot_t *boot) {
    return boot ? boot->n_entries : 0;
}

const wubu_xdga_entry_t *wubu_session_boot_entry(const wubu_session_boot_t *boot,
                                                 int i) {
    if (!boot || i < 0 || i >= boot->n_entries) return NULL;
    return &boot->entries[i];
}
