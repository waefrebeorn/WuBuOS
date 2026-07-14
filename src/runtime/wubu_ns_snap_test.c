/*
 * wubu_ns_snap_test.c -- verify the snapshot control plane (rip off
 * snapper/btrfs rollback through /n).
 *
 * Drives the REAL wubu_snapshot_* API with a temp overlayfs store (no root
 * needed) so the routing is verified end-to-end, not via mocks. The snapshot
 * objects are small enough to link standalone (the full bridge test's link
 * budget is already spent on archd+bottles+styx).
 */

#define _GNU_SOURCE
#include "wubu_ns_bridge.h"
#include "wubu_snapshot.h"
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

static void test_snapshot_namespace(void) {
    printf("\n-- Snapshot control plane (rip off snapper/btrfs rollback) --\n");
    wubu_ns_bridge_create(g_tmp);

    /* real snapshot store in a temp dir (overlayfs, no root needed) */
    char store_tmpl[] = "/tmp/wubu_ns_snap_XXXXXX";
    char *store = mkdtemp(store_tmpl);
    CHECK(store != NULL, "create temp snapshot store");
    /* WubuSnapshotManager is ~56MB -- heap-allocate, never a stack local. */
    WubuSnapshotManager *mgr = calloc(1, sizeof(WubuSnapshotManager));
    CHECK(mgr != NULL, "allocate snapshot manager (heap)");
    CHECK(wubu_snapshot_manager_init(store, mgr) == 0, "snapshot manager init");

    const char *cid = "deck-root";
    int rc = wubu_ns_publish_snapshots(mgr, cid);
    CHECK(rc == 0, "publish snapshots for container deck-root");
    CHECK(file_exists("snap/deck-root/list"),     "/n/snap/deck-root/list exists");
    CHECK(file_exists("snap/deck-root/create"),   "/n/snap/deck-root/create exists");
    CHECK(file_exists("snap/deck-root/rollback"), "/n/snap/deck-root/rollback exists");
    CHECK(file_exists("snap/deck-root/delete"),   "/n/snap/deck-root/delete exists");

    /* create a snapshot via the pure dispatcher */
    CHECK(wubu_ns_snap_create(mgr, cid, "before-update") == 0,
          "snap_create routes to wubu_snapshot_create");

    /* list should now contain it (id + label + status + size) */
    char list[8192];
    int lw = wubu_ns_snap_list_str(mgr, cid, list, sizeof(list));
    CHECK(lw > 0, "snap_list_str returns non-empty after create");
    CHECK(strstr(list, "before-update") != NULL,
          "list contains the created snapshot label");
    CHECK(file_contains("snap/deck-root/list", "before-update"),
          "/n/snap/deck-root/list file reflects the snapshot");

    /* capture the snapshot id from the list (first field) */
    char snap_id[WUBU_SNAPSHOT_ID_LEN] = {0};
    if (lw > 0) {
        const char *tab = strchr(list, '\t');
        size_t idlen = tab ? (size_t)(tab - list) : strlen(list);
        if (idlen >= WUBU_SNAPSHOT_ID_LEN) idlen = WUBU_SNAPSHOT_ID_LEN - 1;
        memcpy(snap_id, list, idlen);
    }
    CHECK(snap_id[0] != '\0', "extracted snapshot id from list");

    /* rollback + delete route correctly (no crash; real API) */
    CHECK(wubu_ns_snap_rollback(mgr, snap_id) == 0,
          "snap_rollback routes to wubu_snapshot_rollback");
    CHECK(wubu_ns_snap_delete(mgr, snap_id) == 0,
          "snap_delete routes to wubu_snapshot_delete");

    /* list should now be empty (deleted) */
    char list2[8192];
    int lw2 = wubu_ns_snap_list_str(mgr, cid, list2, sizeof(list2));
    CHECK(lw2 == 0, "snap_list_str empty after delete");

    wubu_snapshot_manager_shutdown(mgr);
    char cmd[9000]; snprintf(cmd, sizeof(cmd), "rm -rf %s", store);
    system(cmd);
    free(mgr);
}

int main(void) {
    char tmpl[] = "/tmp/wubu_ns_snap_XXXXXX";
    char *d = mkdtemp(tmpl);
    if (!d) { printf("mkdtemp failed\n"); return 1; }
    strcpy(g_tmp, d);

    test_snapshot_namespace();

    char cmd[9000]; snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmp);
    system(cmd);

    printf("\n==================================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("==================================================\n");
    return g_fail == 0 ? 0 : 1;
}
