/* wubu_ns_de_test.c -- P3 /n/background/de DE-daemon framework surface
 * (Mint baseline §154 / §5). Tests tray-slot ownership (XApp.StatusIcon: one
 * owner per named slot, late claim refused) + desktop ownership (nemo-desktop:
 * one owner), and the /n/background/de publish surface.
 */
#include "../egojar.h"
#include "wubu_deskenv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

extern const char *g_ns_root;
#define NSROOT "/tmp/wubu_ns_de_test"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

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

    wubu_deskenv_t *de = wubu_deskenv_create();
    CHECK(de != NULL, "de framework created");

    /* tray claim by first owner */
    CHECK(wubu_deskenv_tray_claim(de, "volume", "pulseaudio") == 0,
          "pulseaudio claims the volume slot");
    CHECK(wubu_deskenv_tray_claim(de, "network", "nm-applet") == 0,
          "nm-applet claims the network slot");
    CHECK(wubu_deskenv_tray_count(de) == 2, "two tray slots claimed");

    /* a different owner is REFUSED the same slot */
    CHECK(wubu_deskenv_tray_claim(de, "volume", "other-app") != 0,
          "a second owner claiming an owned slot is refused");
    /* the SAME owner re-claiming is idempotent success */
    CHECK(wubu_deskenv_tray_claim(de, "volume", "pulseaudio") == 0,
          "same owner re-claim is a no-op success");

    /* ownership lookup */
    CHECK(wubu_deskenv_tray_owner(de, "volume") != NULL &&
          strcmp(wubu_deskenv_tray_owner(de, "volume"), "pulseaudio") == 0,
          "volume is owned by pulseaudio");

    /* release: wrong owner cannot release */
    CHECK(wubu_deskenv_tray_release(de, "volume", "other-app") != 0,
          "wrong owner cannot release the slot");
    /* release by the owner works */
    CHECK(wubu_deskenv_tray_release(de, "volume", "pulseaudio") == 0,
          "owner releases the slot");
    CHECK(wubu_deskenv_tray_owner(de, "volume") == NULL, "volume now unowned");
    CHECK(wubu_deskenv_tray_count(de) == 1, "one slot remains");

    /* desktop owner (nemo-desktop role) */
    CHECK(wubu_deskenv_desktop_owner(de) == NULL, "no desktop owner initially");
    CHECK(wubu_deskenv_desktop_set(de, "nemo-desktop") == 0, "nemo-desktop set as owner");
    CHECK(wubu_deskenv_desktop_owner(de) != NULL &&
          strcmp(wubu_deskenv_desktop_owner(de), "nemo-desktop") == 0,
          "desktop owner is nemo-desktop");
    wubu_deskenv_desktop_clear(de);
    CHECK(wubu_deskenv_desktop_owner(de) == NULL, "desktop owner cleared");

    /* publish the surface */
    wubu_deskenv_desktop_set(de, "nemo-desktop");
    CHECK(wubu_ns_publish_deskenv(de) == 0, "/n/background/de published");

    char p[1024], buf[1024];
    snprintf(p, sizeof(p), "%s/background/de/tray", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0, "de/tray published");
    CHECK(strstr(buf, "network nm-applet") != NULL, "de/tray lists the network slot owner");

    snprintf(p, sizeof(p), "%s/background/de/traycount", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0 && atoi(buf) == 1,
          "de/traycount == 1");
    snprintf(p, sizeof(p), "%s/background/de/desktop", NSROOT);
    CHECK(readfile(p, buf, sizeof(buf)) == 0 && strcmp(buf, "nemo-desktop") == 0,
          "de/desktop == nemo-desktop");

    wubu_deskenv_destroy(de);
    snprintf(p, sizeof(p), "rm -rf %s", NSROOT);
    system(p);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
