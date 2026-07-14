/*
 * wubu_ns_snap.c -- WuBuOS Namespace Bridge: snapshots as a 9P control
 * plane (rip off snapper/btrfs rollback, do it better through /n).
 *
 * Exposes a container's snapshots as /n/snap/<container>/{list,create,
 * rollback,delete}. list is a live read of wubu_snapshot_list(); the ctl
 * files map writes to the pure dispatchers wubu_ns_snap_create/rollback/
 * delete -- the same pattern as the svc/bottle bridges. Because the
 * snapshot manager runs in-process (overlayfs store, no root needed), the
 * bridge is driven by the REAL API in tests (no DI mocks required).
 *
 * C11, opaque structs, minimal includes. Reuses g_ns_root/ns_mkdir/ns_write
 * from wubu_ns_bridge.c via wubu_ns_bridge_internal.h (no duplication).
 */

#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* forward decl: refreshes /n/snap/<c>/list from the live store */
static int snap_refresh_list(WubuSnapshotManager *mgr, const char *container_id);

static const char *snap_status_str(WubuSnapshotStatus s) {
    switch (s) {
        case WUBU_SNAP_STATUS_READY:    return "ready";
        case WUBU_SNAP_STATUS_CREATING: return "creating";
        case WUBU_SNAP_STATUS_MOUNTED:  return "mounted";
        case WUBU_SNAP_STATUS_ERROR:    return "error";
        case WUBU_SNAP_STATUS_DELETING: return "deleting";
        case WUBU_SNAP_STATUS_MERGING:  return "merging";
        default:                        return "unknown";
    }
}

int wubu_ns_snap_list_str(WubuSnapshotManager *mgr, const char *container_id,
                         char *buf, size_t buf_size) {
    if (!mgr || !container_id || !buf || buf_size == 0) return -1;
    /* Heap-allocate the snapshot scratch buffer: WUBU_MAX_SNAPSHOTS (1024)
     * WubuSnapshot structs are too large for the stack. */
    enum { SNAP_CAP = 256 };
    WubuSnapshot *snaps = calloc(SNAP_CAP, sizeof(WubuSnapshot));
    if (!snaps) return -1;
    int n = wubu_snapshot_list(mgr, snaps, SNAP_CAP, NULL, true);
    if (n < 0) { free(snaps); return -1; }

    size_t off = 0;
    buf[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (strcmp(snaps[i].container_id, container_id) != 0) continue;
        int w = snprintf(buf + off, buf_size - off,
                         "%s\t%s\t%s\t%llu\n",
                         snaps[i].id,
                         snaps[i].label[0] ? snaps[i].label : "-",
                         snap_status_str(snaps[i].status),
                         (unsigned long long)snaps[i].size_bytes);
        if (w < 0 || (size_t)w >= buf_size - off) { buf[off] = '\0'; free(snaps); return -1; }
        off += (size_t)w;
    }
    free(snaps);
    return (int)off;
}

int wubu_ns_publish_snapshots(WubuSnapshotManager *mgr, const char *container_id) {
    if (!g_ns_root || !mgr || !container_id) return -1;

    char dir[4096];
    snprintf(dir, sizeof(dir), "snap/%s", container_id);
    if (ns_mkdir(dir) != 0) return -1;

    /* list -- live view of wubu_snapshot_list (refreshed after each
     * mutation below so the namespace stays current). */
    if (snap_refresh_list(mgr, container_id) != 0) return -1;

    /* create / rollback / delete -- action files; writes dispatched by the
     * pure fns below (mirrors svc/bottle ctl pattern). */
    char sub[4096];
    snprintf(sub, sizeof(sub), "snap/%s/create", container_id);
    if (ns_write(sub, "# write a label to create a snapshot\n") != 0) return -1;
    snprintf(sub, sizeof(sub), "snap/%s/rollback", container_id);
    if (ns_write(sub, "# write a snapshot id to roll back to it\n") != 0) return -1;
    snprintf(sub, sizeof(sub), "snap/%s/delete", container_id);
    if (ns_write(sub, "# write a snapshot id to delete it\n") != 0) return -1;
    return 0;
}

/* Rewrite /n/snap/<container>/list from the current snapshot store. */
static int snap_refresh_list(WubuSnapshotManager *mgr, const char *container_id) {
    char list[8192];
    if (wubu_ns_snap_list_str(mgr, container_id, list, sizeof(list)) < 0)
        list[0] = '\0';
    char sub[4096];
    snprintf(sub, sizeof(sub), "snap/%s/list", container_id);
    return ns_write(sub, list);
}

int wubu_ns_snap_create(WubuSnapshotManager *mgr, const char *container_id,
                        const char *label) {
    if (!mgr || !container_id) return -1;
    WubuSnapshot *out = NULL;
    /* base_snapshot_id=NULL -> full snapshot; branch=NULL (default),
     * description="" . */
    int rc = wubu_snapshot_create(mgr, container_id, NULL, NULL,
                                  label ? label : "", "", &out);
    if (rc == 0) snap_refresh_list(mgr, container_id);
    return rc;
}

int wubu_ns_snap_rollback(WubuSnapshotManager *mgr, const char *snapshot_id) {
    if (!mgr || !snapshot_id) return -1;
    return wubu_snapshot_rollback(mgr, snapshot_id);
}

int wubu_ns_snap_delete(WubuSnapshotManager *mgr, const char *snapshot_id) {
    if (!mgr || !snapshot_id) return -1;
    return wubu_snapshot_delete(mgr, snapshot_id, true);
}
