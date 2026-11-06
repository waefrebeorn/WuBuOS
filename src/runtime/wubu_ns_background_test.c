/* wubu_ns_background_test.c -- P3: the /n/background control subtree.
 * Builds a session-boot, starts its autostart apps through a fake launcher,
 * publishes the state via wubu_ns_publish_background(), and verifies the
 * /n/background files land: session, bus/names, bus/nconn, autostart/count,
 * autostart/N.
 *
 * Memory-constrained rule: the test links wubu_ns_fs.c (the FS core) and
 * points g_ns_root at a temp dir DIRECTLY (no heavy wubu_ns_bridge.o).
 */
#include "wubu_ns_background.h"
#include "wubu_session_boot.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* g_ns_root is defined in wubu_ns_fs.c; re-point it at a temp dir. */
extern const char *g_ns_root;
#define NSROOT "/tmp/wubu_ns_background_test"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static int launcher(wubu_session_boot_t *boot,
                    const wubu_xdga_entry_t *entry, char **argv, void *ud) {
    (void)boot; (void)entry; (void)argv; (void)ud;
    return 0;
}

static void write_entry(const char *dir, const char *file, const char *body) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    FILE *f = fopen(path, "w");
    if (f) { fputs(body, f); fclose(f); }
}

/* read a file fully (up to n) into buf; returns 0 on success */
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

    /* a real autostart dir with 2 launchable apps */
    char dir[] = "/tmp/wubu_nsbg_astart_XXXXXX";
    char *d = mkdtemp(dir);
    write_entry(d, "mintupdate.desktop",
        "[Desktop Entry]\nType=Application\nName=Mint Update\nExec=mintupdate\n");
    write_entry(d, "warpinator.desktop",
        "[Desktop Entry]\nType=Application\nName=Warpinator\nExec=warpinator\n");

    wubu_session_boot_t *boot = wubu_session_boot_create();
    CHECK(boot != NULL, "session-boot created");
    wubu_session_boot_set_launcher(boot, launcher, NULL);
    char *dirs[1] = { d };
    CHECK(wubu_session_boot_scan(boot, dirs, 1) == 2, "scan finds 2 apps");
    CHECK(wubu_session_boot_start_all(boot) == 2, "started 2 apps");

    int rc = wubu_ns_publish_background(boot);
    CHECK(rc == 0, "background state published to /n/background");

    /* verify the tree */
    char buf[1024];
    char p[1024];

    snprintf(p, sizeof(p), "%s/background/session_boot", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0, "session_boot file exists");
    CHECK(strcmp(buf, "desktop\n") == 0, "session_boot reports desktop mode");

    snprintf(p, sizeof(p), "%s/background/bus/names", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0, "bus/names exists");
    /* both apps own their bus names, plus the 2 connection conns */
    CHECK(strstr(buf, "org.wubu.autostart.mintupdate") != NULL,
          "bus/names lists mintupdate");
    CHECK(strstr(buf, "org.wubu.autostart.warpinator") != NULL,
          "bus/names lists warpinator");

    snprintf(p, sizeof(p), "%s/background/bus/nconn", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0, "bus/nconn exists");
    int nconn = atoi(buf);
    CHECK(nconn >= 2, "bus/nconn reflects live connections (>=2)");

    snprintf(p, sizeof(p), "%s/background/autostart/count", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0, "autostart/count exists");
    CHECK(atoi(buf) == 2, "autostart/count == 2");

    snprintf(p, sizeof(p), "%s/background/autostart/0", NSROOT);
    char a0[1024];
    CHECK(readfile(p, a0, sizeof(a0)) == 0, "autostart/0 exists");
    snprintf(p, sizeof(p), "%s/background/autostart/1", NSROOT);
    char a1[1024];
    CHECK(readfile(p, a1, sizeof(a1)) == 0, "autostart/1 exists");
    /* both app names must appear across the two files, independent of the
     * readdir scan order */
    int copy_a0 = (strcmp(a0, "Mint Update") == 0 || strcmp(a1, "Mint Update") == 0);
    int copy_w  = (strcmp(a0, "Warpinator") == 0 || strcmp(a1, "Warpinator") == 0);
    CHECK(copy_a0, "one autostart/N file is Mint Update");
    CHECK(copy_w, "one autostart/N file is Warpinator");

    wubu_session_boot_destroy(boot);
    snprintf(p, sizeof(p), "rm -rf %s", d);
    system(p);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
