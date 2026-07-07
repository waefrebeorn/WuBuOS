/*
 * wubu_system.c -- WuBuOS System Root (immutable / atomic) layer.
 *
 * Implementation wraps wubu_snapshot.c (real overlayfs manager). The system
 * root is a single "container" tracked by the manager; each commit is a
 * snapshot on the system branch, and the active base is the most recent
 * read-only snapshot. No stubs -- every function performs real manager calls.
 */

#define _POSIX_C_SOURCE 200809L
#include "wubu_system.h"
#include "wubu_snapshot.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

/* The system root is tracked as a single pseudo-container in the manager. */
#define WUBU_SYSTEM_CONTAINER  "wubu-system-root"

static WubuSnapshotManager *g_sys_mgr = NULL;
static bool                 g_ready   = false;
static bool                 g_dev     = false;
static char                 g_store[WUBU_MAX_PATH] = {0};

/* Resolve the "~" prefix to the user's home directory. Returns a heap buffer
 * (caller frees) or a strdup of the input if no '~' is present. */
static char *expand_store(const char *path) {
    if (path && path[0] == '~') {
        const char *home = getenv("HOME");
        if (!home || !*home) home = "/tmp";
        size_t need = strlen(home) + strlen(path + 1) + 2;
        char *out = malloc(need);
        if (!out) return NULL;
        snprintf(out, need, "%s%s", home, path + 1);
        return out;
    }
    return strdup(path ? path : WUBU_SYSTEM_STORE);
}

int wubu_system_init(const char *store_path) {
    if (g_ready) return 0; /* idempotent */

    g_dev = (getenv("WUBU_SYSTEM_DEV") != NULL);

    char *store = expand_store(store_path ? store_path : WUBU_SYSTEM_STORE);
    if (!store) return -1;

    WubuSnapshotManager *mgr = calloc(1, sizeof(WubuSnapshotManager));
    if (!mgr) { free(store); return -1; }

    if (wubu_snapshot_manager_init(store, mgr) != 0) {
        free(store); free(mgr); return -1;
    }

    strncpy(g_store, store, sizeof(g_store) - 1);
    free(store);
    g_sys_mgr = mgr;
    g_ready   = true;
    return 0;
}

void wubu_system_shutdown(void) {
    if (g_sys_mgr) {
        wubu_snapshot_manager_free(g_sys_mgr);
        free(g_sys_mgr);
        g_sys_mgr = NULL;
    }
    g_store[0] = '\0';
    g_ready   = false;
}

bool wubu_system_ready(void) {
    return g_ready && g_sys_mgr != NULL;
}

bool wubu_system_developer_mode(void) {
    return g_dev;
}

int wubu_system_commit(const char *label, char *out_id, int id_len) {
    if (!wubu_system_ready()) return -1;

    /* Find current branch head to chain incrementally from it. */
    const char *parent = g_sys_mgr->branches[0].head_snapshot_id;
    if (parent[0] == '\0') parent = NULL;

    const char *lbl = label ? label : "system-baseline";

    WubuSnapshot *snap = NULL;
    int rc = wubu_snapshot_create(g_sys_mgr, WUBU_SYSTEM_CONTAINER, parent,
                                  WUBU_SYSTEM_BRANCH, lbl,
                                  "WuBuOS system baseline (atomic commit)", &snap);
    if (rc != 0 || !snap) return -1;

    /* Mark the new baseline read-only (immutable rootfs guarantee). */
    snap->read_only = true;

    if (out_id && id_len > 0) {
        strncpy(out_id, snap->id, (size_t)id_len - 1);
        out_id[id_len - 1] = '\0';
    }
    return 0;
}

int wubu_system_rollback(const char *snapshot_id) {
    if (!wubu_system_ready() || !snapshot_id || !snapshot_id[0]) return -1;
    /* Real restore: flips branch head + copies the overlayfs upper layer back. */
    return wubu_snapshot_rollback(g_sys_mgr, snapshot_id);
}

bool wubu_system_is_readonly(void) {
    if (!wubu_system_ready()) return false;
    /* DEVELOPER mode disables the read-only guarantee for mutable hacking. */
    if (g_dev) return false;
    /* Read-only holds if we have at least one committed (read-only) baseline. */
    return g_sys_mgr->branches[0].head_snapshot_id[0] != '\0';
}

int wubu_system_active_label(char *out_label, int label_len) {
    if (!wubu_system_ready() || !out_label || label_len <= 0) return -1;
    out_label[0] = '\0';
    const char *head = g_sys_mgr->branches[0].head_snapshot_id;
    if (head[0] == '\0') return 0; /* no baseline yet */

    WubuSnapshot snap;
    if (wubu_snapshot_inspect(g_sys_mgr, head, &snap) != 0) return -1;
    strncpy(out_label, snap.label, (size_t)label_len - 1);
    out_label[label_len - 1] = '\0';
    return 0;
}

int wubu_system_baseline_count(void) {
    if (!wubu_system_ready()) return -1;
    return g_sys_mgr->branches[0].snapshot_count;
}
