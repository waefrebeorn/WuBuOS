/*
 * wubu_ns_pkg_test.c -- verify the package control plane (rip off
 * pacman/Chaotic-AUR through /n). Drives the REAL wubu_pkg_* API so routing
 * is verified end-to-end. Lives in its own small binary (the full bridge
 * test's link budget is already spent on archd+bottles+styx).
 */

#define _GNU_SOURCE
#include "wubu_ns_bridge.h"
#include "wubu_pkg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else      { g_fail++; printf("  ❌ %s\n", msg); } \
} while (0)

static char g_tmp[4096];
static int file_exists(const char *rel) {
    char p[8192]; snprintf(p, sizeof(p), "%s/%s", g_tmp, rel);
    struct stat st; return stat(p, &st) == 0;
}
static int file_contains(const char *rel, const char *needle) {
    char p[8192]; snprintf(p, sizeof(p), "%s/%s", g_tmp, rel);
    FILE *f = fopen(p, "r"); if (!f) return 0;
    char buf[4096]; int found = 0;
    while (fgets(buf, sizeof(buf), f))
        if (strstr(buf, needle)) { found = 1; break; }
    fclose(f); return found;
}

static void test_pkg_namespace(void) {
    printf("\n-- Package control plane (rip off pacman/Chaotic-AUR) --\n");
    wubu_ns_bridge_create(g_tmp);

    PkgManager mgr;
    pkg_init(&mgr);

    /* seed a registry + an enabled repo (Chaotic-AUR vibe) */
    pkg_register(&mgr, "vulkan-tools", "1.3", "Vulkan debug tools", 120000);
    pkg_register(&mgr, "proton-cachyos", "9.0", "CachyOS Proton build", 480000);
    wubu_ns_pkg_add_repo(&mgr, "chaotic-aur", "https://aur.chaotic.cx");

    int rc = wubu_ns_publish_pkg(&mgr);
    CHECK(rc == 0, "publish package control plane");
    CHECK(file_exists("pkg/install"),  "/n/pkg/install exists");
    CHECK(file_exists("pkg/remove"),   "/n/pkg/remove exists");
    CHECK(file_exists("pkg/addrepo"),  "/n/pkg/addrepo exists");
    CHECK(file_exists("pkg/list"),     "/n/pkg/list exists");
    CHECK(file_exists("pkg/repos"),    "/n/pkg/repos exists");

    /* repo shows up in /n/pkg/repos (the Chaotic-AUR vibe) */
    CHECK(file_contains("pkg/repos", "chaotic-aur"),
          "/n/pkg/repos lists chaotic-aur");

    /* install routes to pkg_install and updates /n/pkg/list live */
    CHECK(wubu_ns_pkg_install(&mgr, "vulkan-tools") == 0,
          "pkg_install routes to pkg_install");
    CHECK(pkg_is_installed(&mgr, "vulkan-tools") == 1,
          "pkg reports vulkan-tools installed");
    CHECK(file_contains("pkg/list", "vulkan-tools"),
          "/n/pkg/list reflects the install (live view)");

    /* remove routes to pkg_remove and updates /n/pkg/list live */
    CHECK(wubu_ns_pkg_remove(&mgr, "vulkan-tools") == 0,
          "pkg_remove routes to pkg_remove");
    CHECK(pkg_is_installed(&mgr, "vulkan-tools") == 0,
          "pkg reports vulkan-tools removed");
    CHECK(!file_contains("pkg/list", "vulkan-tools"),
          "/n/pkg/list no longer shows removed pkg (live view)");

    pkg_shutdown(&mgr);
}

int main(void) {
    char tmpl[] = "/tmp/wubu_ns_pkg_XXXXXX";
    char *d = mkdtemp(tmpl);
    if (!d) { printf("mkdtemp failed\n"); return 1; }
    strcpy(g_tmp, d);

    test_pkg_namespace();

    char cmd[9000]; snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmp);
    system(cmd);

    printf("\n==================================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("==================================================\n");
    return g_fail == 0 ? 0 : 1;
}
