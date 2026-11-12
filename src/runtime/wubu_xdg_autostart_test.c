/* wubu_xdg_autostart_test.c -- P3: the XDG Autostart .desktop contract
 * (Mint baseline §3.2). Writes real .desktop files into a temp dir, scans,
 * and asserts the freedesktop autostart gating:
 *   - a plain app has its Exec parsed (field codes stripped, name read)
 *   - Hidden=true is skipped
 *   - X-GNOME-Autostart-enabled=false is skipped (the "disable autostart" out)
 *   - a non-Application Type is skipped
 *   - Exec quoting: "prog 'arg with space' --flag %f" -> tokens split right,
 *     the %f field code removed, the quoted arg kept as one token.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "../egojar.h"
#include "wubu_xdg_autostart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static void write_entry(const char *dir, const char *file, const char *body) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(body, f);
    fclose(f);
}

int main(void) {
    char dir[] = "/tmp/wubu_xdga_test_XXXXXX";
    char *d = mkdtemp(dir);
    CHECK(d != NULL, "temp autostart dir created");

    write_entry(d, "mintupdate.desktop",
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Mint Update\n"
        "Exec=mintupdate-launcher\n"
        "X-GNOME-Autostart-enabled=true\n");
    write_entry(d, "hidden.desktop",
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Hidden App\n"
        "Exec=hidden-app\n"
        "Hidden=true\n");
    write_entry(d, "disabled.desktop",
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Disabled App\n"
        "Exec=disabled-app\n"
        "X-GNOME-Autostart-enabled=false\n");
    write_entry(d, "notapp.desktop",
        "[Desktop Entry]\n"
        "Type=Link\n"
        "Name=Not an App\n"
        "Exec=notapp\n");
    write_entry(d, "quoted.desktop",
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Quoted\n"
        "Exec=/usr/bin/launcher 'arg with space' --flag %f\n");

    wubu_xdga_entry_t entries[WUBU_XDGA_MAX_ENTRY];
    int n = wubu_xdga_scan_dir(d, entries, WUBU_XDGA_MAX_ENTRY);
    CHECK(n == 2, "two launchable entries (mintupdate + quoted) parsed");
    fprintf(stderr, "> scanned %d launchable of 5 .desktop files\n", n);

    /* find by name */
    int mi = -1, qi = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(entries[i].name, "Mint Update") == 0) mi = i;
        if (strcmp(entries[i].name, "Quoted") == 0)     qi = i;
    }
    CHECK(mi >= 0 && qi >= 0, "both expected entries found by name");
    if (mi < 0 || qi < 0) return 1;

    /* Exec extraction for the plain app */
    CHECK(strcmp(entries[mi].exec, "mintupdate-launcher") == 0,
          "Exec parsed verbatim for the plain app");

    /* argv build: field-code strip + quoted-arg integrity */
    char *argv[65];
    int na = wubu_xdga_build_argv(&entries[qi], argv);
    CHECK(na == 3, "quoted exec split into 3 argv tokens (code removed)");
    if (na == 3) {
        fprintf(stderr, "> tokens: [%s] [%s] [%s]\n", argv[0], argv[1], argv[2]);
        CHECK(strcmp(argv[0], "/usr/bin/launcher") == 0, "argv[0] is the binary");
        CHECK(strcmp(argv[1], "arg with space") == 0,
              "quoted argument kept as ONE token");
        CHECK(strcmp(argv[2], "--flag") == 0, "argv[2] is --flag (%f removed)");
        CHECK(argv[3] == NULL, "argv is NULL-terminated");
        for (int i = 0; i < na; i++) free(argv[i]);
    }

    /* the gated-out entries were NOT picked up */
    int saw_hidden = 0, saw_disabled = 0, saw_link = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(entries[i].name, "Hidden App") == 0) saw_hidden = 1;
        if (strcmp(entries[i].name, "Disabled App") == 0) saw_disabled = 1;
        if (strcmp(entries[i].name, "Not an App") == 0) saw_link = 1;
    }
    CHECK(!saw_hidden, "Hidden=true entry skipped");
    CHECK(!saw_disabled, "X-GNOME-Autostart-enabled=false entry skipped");
    CHECK(!saw_link, "non-Application Type entry skipped");

    /* missing dir -> -1, not crash */
    CHECK(wubu_xdga_scan_dir("/tmp/definitely-not-a-real-autostart-dir-9",
                             entries, 4) == -1,
          "missing dir returns -1");

    /* cleanup */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", d);
    system(cmd);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
