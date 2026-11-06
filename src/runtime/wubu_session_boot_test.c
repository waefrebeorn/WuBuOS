/* wubu_session_boot_test.c -- P3 session-boot orchestrator: the
 * cinnamon-session analogue that owns the bus and runs the XDG autostart
 * apps. A FAKE launcher records which entries the boot asked to start and
 * verifies each launched app got a well-known bus name claimed on the session
 * bus (so the running background app is addressable afterwards).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "wubu_session_boot.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static char g_launched[8][256];
static int  g_n_launched = 0;
static int launch_cb(wubu_session_boot_t *boot,
                     const wubu_xdga_entry_t *entry, char **argv, void *ud) {
    (void)boot;
    (void)ud;
    if (g_n_launched < 8) {
        char *csv = g_launched[g_n_launched];
        size_t off = 0;
        off += (size_t)snprintf(csv + off, 256 - off, "%s", entry->name);
        for (char **a = argv; a && *a && off < 220; a++)
            off += (size_t)snprintf(csv + off, 256 - off, "|%s", *a);
        g_n_launched++;
    }
    return 0; /* accept: app started */
}

static void write_entry(const char *dir, const char *file, const char *body) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    FILE *f = fopen(path, "w");
    if (f) { fputs(body, f); fclose(f); }
}

int main(void) {
    char dir[] = "/tmp/wubu_sboot_test_XXXXXX";
    char *d = mkdtemp(dir);
    CHECK(d != NULL, "temp autostart dir created");

    write_entry(d, "mintupdate.desktop",
        "[Desktop Entry]\nType=Application\nName=Mint Update\nExec=mintupdate \"--auto\"\n");
    write_entry(d, "warpinator.desktop",
        "[Desktop Entry]\nType=Application\nName=Warpinator\nExec=warpinator\n");
    write_entry(d, "hidden.desktop",
        "[Desktop Entry]\nType=Application\nName=Hidden\nExec=hidden\nHidden=true\n");

    char *dirs[1] = { d };
    wubu_session_boot_t *boot = wubu_session_boot_create();
    CHECK(boot != NULL, "session-boot created");

    int n = wubu_session_boot_scan(boot, dirs, 1);
    CHECK(n == 2, "scan finds 2 launchable apps (hidden gate applied)");
    CHECK(wubu_session_boot_count(boot) == 2, "count matches scan");

    /* no launcher set -> start refuses */
    CHECK(wubu_session_boot_start_all(boot) == -1, "start refuses before a launcher is set");

    wubu_session_boot_set_launcher(boot, launch_cb, NULL);
    int started = wubu_session_boot_start_all(boot);
    CHECK(started == 2, "both launchable apps started via the launcher");
    fprintf(stderr, "> boot started %d apps\n", started);

    CHECK(g_n_launched == 2, "launcher invoked exactly twice");
    CHECK(strcmp(g_launched[0], "Mint Update|mintupdate|--auto") == 0,
          "first launcher argv is mintupdate --auto");
    CHECK(strcmp(g_launched[1], "Warpinator|warpinator") == 0,
          "second launcher argv is warpinator");

    /* each launched app has a well-known name claimed on the session bus */
    wubu_bus_t *bus = wubu_session_boot_bus(boot);
    CHECK(wubu_bus_owner_of(bus, "org.wubu.autostart.mintupdate") != -1,
          "mintupdate owns its bus name (addressable after launch)");
    CHECK(wubu_bus_owner_of(bus, "org.wubu.autostart.warpinator") != -1,
          "warpinator owns its bus name");
    CHECK(wubu_bus_owner_of(bus, "org.wubu.autostart.hidden") == -1,
          "hidden app has NO bus name (was never launched)");

    wubu_session_boot_destroy(boot);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", d);
    system(cmd);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
