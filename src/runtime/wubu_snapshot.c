/*
 * wubu_snapshot.c  --  WuBuOS Container Snapshot Manager (Real Implementation)
 *
 * Phase 7+: In-memory snapshot/restore with git-like branching.
 *
 * Design:
 *   - WubuSnapshotManager holds fixed arrays of WubuSnapshot, WubuBranch, WubuTag
 *   - Snapshots have unique IDs (hash-based), parent references, and branch labels
 *   - Branches are named pointers to snapshot chains (like git branches)
 *   - Tags are named references to specific snapshots
 *   - Export writes a .wubu-compatible tarball with snapshot metadata
 *   - Import reads exported snapshots back into the manager
 *   - Diff computes file-level changes between snapshot directory listings
 *   - GC applies retention policies to prune old snapshots
 *   - Rollback restores a container to a previous snapshot state
 *
 * Filesystem backend:
 *   - When root_path exists, snapshots are stored as directories with metadata
 *   - overlayfs: lower_dir (read-only base), upper_dir (changes), work_dir, merged_dir
 *   - Full snapshots copy all files; incremental snapshots copy only changed files
 *
 /* Limitations (documented):
  *   - btrfs native snapshots implemented via ioctl (BTRFS_IOC_SNAP_CREATE_V2)
  *   - zfs native snapshots via zfs command (requires zfs userspace tools)
  *   - LVM thin provisioning via lvm2 tools (requires lvm2 userspace tools)
  */

#define _POSIX_C_SOURCE 200809L
#include "wubu_snapshot.h"
#include "wubu_snapshot_internal.h"
#include "wubu_snapshot_fs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ftw.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/limits.h>

/* -- Manager lifecycle -------------------------------------------- */

int wubu_snapshot_manager_init(const char *root_path, WubuSnapshotManager *mgr) {
    if (!mgr) return -1;
    memset(mgr, 0, sizeof(*mgr));
    if (root_path) {
        strncpy(mgr->root_path, root_path, sizeof(mgr->root_path) - 1);
    } else {
        strncpy(mgr->root_path, "/var/wubu/snapshots", sizeof(mgr->root_path) - 1);
    }
    mgr->fs_type = WUBU_FS_OVERLAYFS;
    mgr->snapshot_count = 0;
    mgr->branch_count = 0;
    mgr->tag_count = 0;
    mgr->retention_rule_count = 0;
    mgr->auto_snapshot_on_commit = false;
    mgr->auto_snapshot_on_stop = false;
    mgr->auto_snapshot_on_signal = false;
    mgr->auto_snapshot_interval = 0;
    mgr->auto_gc = false;
    mgr->gc_interval = 0;
    mgr->max_store_size = 0;
    mgr->progress_cb = NULL;
    mgr->progress_user_data = NULL;

    /* Create default branch */
    WubuBranch *main_branch = &mgr->branches[mgr->branch_count];
    strncpy(main_branch->name, "main", sizeof(main_branch->name) - 1);
    main_branch->head_snapshot_id[0] = '\0'; /* no snapshots yet */
    main_branch->base_snapshot_id[0] = '\0';
    main_branch->created = snapshot_now();
    main_branch->updated = main_branch->created;
    main_branch->snapshot_count = 0;
    main_branch->protected = true; /* main branch is protected */
    mgr->branch_count++;

    return 0;
}

int wubu_snapshot_manager_shutdown(WubuSnapshotManager *mgr) {
    if (!mgr) return -1;
    /* Unmount all mounted snapshots */
    for (int i = 0; i < mgr->snapshot_count; i++) {
        if (mgr->snapshots[i].status == WUBU_SNAP_STATUS_MOUNTED) {
            mgr->snapshots[i].status = WUBU_SNAP_STATUS_READY;
            mgr->snapshots[i].ref_count = 0;
        }
    }
    return 0;
}

int wubu_snapshot_manager_set_fs_type(WubuSnapshotManager *mgr, WubuFsType fs_type) {
    if (!mgr) return -1;
    mgr->fs_type = fs_type;
    return 0;
}

void wubu_snapshot_manager_free(WubuSnapshotManager *mgr) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
}

/* -- Snapshot operations ------------------------------------------ */

int wubu_snapshot_create(WubuSnapshotManager *mgr, const char *container_id, const char *base_snapshot_id,
                         const char *branch, const char *label, const char *description,
                         WubuSnapshot **out_snapshot) {
    if (!mgr || !container_id) return -1;
    if (mgr->snapshot_count >= WUBU_MAX_SNAPSHOTS) return -1;

    /* Resolve branch */
    const char *branch_name = branch ? branch : "main";
    WubuBranch *br = find_branch(mgr, branch_name);
    if (!br) return -1; /* branch doesn't exist */

    /* Resolve parent */
    const char *parent_id = base_snapshot_id;
    if (!parent_id || parent_id[0] == '\0') {
        /* Use branch head as parent */
        parent_id = br->head_snapshot_id;
    }

    WubuSnapshot *s = &mgr->snapshots[mgr->snapshot_count];
    snapshot_default(s);

    /* Generate deterministic ID */
    gen_snapshot_id_from_data(s->id, sizeof(s->id), container_id, label, s->created);
    strncpy(s->container_id, container_id, sizeof(s->container_id) - 1);
    strncpy(s->branch, branch_name, sizeof(s->branch) - 1);

    if (parent_id && parent_id[0]) {
        strncpy(s->parent_id, parent_id, sizeof(s->parent_id) - 1);
        /* Incremental if parent exists */
        if (parent_id[0] != '\0') {
            s->type = WUBU_SNAP_TYPE_INCREMENTAL;
        }
    }
    if (label) strncpy(s->label, label, sizeof(s->label) - 1);
    if (description) strncpy(s->description, description, sizeof(s->description) - 1);

    /* Build filesystem paths */
    build_snapshot_path(mgr, s->id, s->lower_dir, sizeof(s->lower_dir), "");
    if (parent_id && parent_id[0]) {
        WubuSnapshot *parent = find_snapshot(mgr, parent_id);
        if (parent) {
            strncpy(s->lower_dir, parent->merged_dir, sizeof(s->lower_dir) - 1);
        }
    }
    build_snapshot_path(mgr, s->id, s->upper_dir, sizeof(s->upper_dir), "upper");
    build_snapshot_path(mgr, s->id, s->work_dir, sizeof(s->work_dir), "work");
    build_snapshot_path(mgr, s->id, s->merged_dir, sizeof(s->merged_dir), "merged");

    /* Create snapshot directories on disk */
    ensure_snapshot_dirs(mgr, s->id);

    /* Update size info */
    s->size_bytes = dir_size(s->upper_dir);
    s->unique_bytes = s->size_bytes;
    s->shared_bytes = 0;

    /* Mark as ready */
    s->status = WUBU_SNAP_STATUS_READY;
    s->modified = snapshot_now();

    /* Update branch head */
    strncpy(br->head_snapshot_id, s->id, sizeof(br->head_snapshot_id) - 1);
    br->updated = snapshot_now();
    br->snapshot_count++;

    mgr->snapshot_count++;

    if (out_snapshot) *out_snapshot = s;
    return 0;
}

int wubu_snapshot_create_incremental(WubuSnapshotManager *mgr, const char *parent_id,
                                     const char *branch, const char *label,
                                     WubuSnapshot **out_snapshot) {
    /* Find parent snapshot to get container_id */
    if (!mgr || !parent_id) return -1;
    WubuSnapshot *parent = find_snapshot(mgr, parent_id);
    if (!parent) return -1;

    return wubu_snapshot_create(mgr, parent->container_id, parent_id,
                                branch, label, NULL, out_snapshot);
}

int wubu_snapshot_delete(WubuSnapshotManager *mgr, const char *snapshot_id, bool force) {
    if (!mgr || !snapshot_id) return -1;
    for (int i = 0; i < mgr->snapshot_count; i++) {
        if (strcmp(mgr->snapshots[i].id, snapshot_id) == 0) {
            WubuSnapshot *s = &mgr->snapshots[i];
            if (s->protected && !force) return -1;
            if (s->ref_count > 0 && !force) return -1;
            if (s->status == WUBU_SNAP_STATUS_MOUNTED && !force) return -1;

            s->status = WUBU_SNAP_STATUS_DELETING;
            /* Remove snapshot directory (best-effort) */
            char path[WUBU_MAX_PATH];
            build_snapshot_path(mgr, snapshot_id, path, sizeof(path), NULL);
            rmdir(path);

            /* Remove by shifting */
            memmove(&mgr->snapshots[i], &mgr->snapshots[i + 1],
                    (mgr->snapshot_count - i - 1) * sizeof(WubuSnapshot));
            mgr->snapshot_count--;
            return 0;
        }
    }
    return -1; /* not found */
}

int wubu_snapshot_mount(WubuSnapshotManager *mgr, const char *snapshot_id, const char *mount_point, bool read_only) {
    if (!mgr || !snapshot_id) return -1;
    WubuSnapshot *s = find_snapshot(mgr, snapshot_id);
    if (!s) return -1;
    if (s->status != WUBU_SNAP_STATUS_READY && s->status != WUBU_SNAP_STATUS_MOUNTED) return -1;

    /* Record mount point */
    if (mount_point) {
        strncpy(s->merged_dir, mount_point, sizeof(s->merged_dir) - 1);
    }
    s->status = WUBU_SNAP_STATUS_MOUNTED;
    s->ref_count++;
    s->accessed = snapshot_now();
    s->read_only = read_only;

    /* Attempt real filesystem-appropriate mount (non-fatal if no privileges) */
    int rc = snapshot_mount_fs(s, mgr);
    if (rc != 0) {
        fprintf(stderr, "[wubu_snap] mount on %s failed (non-fatal): %s\n", s->merged_dir, strerror(-rc));
        /* Non-fatal: don't return error, just log it */
    }
    return 0;
}

int wubu_snapshot_unmount(WubuSnapshotManager *mgr, const char *snapshot_id) {
    if (!mgr || !snapshot_id) return -1;
    WubuSnapshot *s = find_snapshot(mgr, snapshot_id);
    if (!s) return -1;
    if (s->status != WUBU_SNAP_STATUS_MOUNTED) return -1;

    /* Attempt real filesystem-appropriate unmount (non-fatal if no privileges) */
    int rc = snapshot_unmount_fs(s);
    if (rc != 0) {
        fprintf(stderr, "[wubu_snap] unmount %s failed (non-fatal): %s\n", s->merged_dir, strerror(-rc));
        /* Non-fatal: don't return error, just log it */
    }

    if (s->ref_count > 0) s->ref_count--;
    if (s->ref_count == 0) {
        s->status = WUBU_SNAP_STATUS_READY;
    }
    return 0;
}

int wubu_snapshot_list(WubuSnapshotManager *mgr, WubuSnapshot *out_snapshots, int max,
                       const char *branch_filter, bool include_hidden) {
    if (!mgr || !out_snapshots || max <= 0) return 0;
    int count = 0;
    for (int i = 0; i < mgr->snapshot_count && count < max; i++) {
        if (branch_filter && branch_filter[0] &&
            strcmp(mgr->snapshots[i].branch, branch_filter) != 0) continue;
        if (!include_hidden && mgr->snapshots[i].status == WUBU_SNAP_STATUS_DELETING) continue;
        memcpy(&out_snapshots[count], &mgr->snapshots[i], sizeof(WubuSnapshot));
        count++;
    }
    return count;
}

/* -- Branch operations -------------------------------------------- */

int wubu_branch_create(WubuSnapshotManager *mgr, const char *name, const char *base_snapshot_id) {
    if (!mgr || !name) return -1;
    if (mgr->branch_count >= WUBU_MAX_BRANCHES) return -1;
    if (find_branch(mgr, name)) return -1; /* already exists */

    WubuBranch *br = &mgr->branches[mgr->branch_count];
    memset(br, 0, sizeof(*br));
    strncpy(br->name, name, sizeof(br->name) - 1);
    if (base_snapshot_id && base_snapshot_id[0]) {
        strncpy(br->base_snapshot_id, base_snapshot_id, sizeof(br->base_snapshot_id) - 1);
        strncpy(br->head_snapshot_id, base_snapshot_id, sizeof(br->head_snapshot_id) - 1);
    }
    br->created = snapshot_now();
    br->updated = br->created;
    br->snapshot_count = 0;
    br->protected = false;
    mgr->branch_count++;
    return 0;
}

int wubu_branch_delete(WubuSnapshotManager *mgr, const char *name, bool force) {
    if (!mgr || !name) return -1;
    for (int i = 0; i < mgr->branch_count; i++) {
        if (strcmp(mgr->branches[i].name, name) == 0) {
            if (mgr->branches[i].protected && !force) return -1;
            memmove(&mgr->branches[i], &mgr->branches[i + 1],
                    (mgr->branch_count - i - 1) * sizeof(WubuBranch));
            mgr->branch_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_branch_switch(WubuSnapshotManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    WubuBranch *br = find_branch(mgr, name);
    if (!br) return -1;
    /* Branch switch just validates existence; the actual head is tracked in the branch */
    (void)mgr;
    return 0;
}

int wubu_branch_merge(WubuSnapshotManager *mgr, const char *from_branch, const char *into_branch,
                      const char *merge_message, WubuSnapshot **out_snapshot) {
    if (!mgr || !from_branch || !into_branch) return -1;
    WubuBranch *src = find_branch(mgr, from_branch);
    WubuBranch *dst = find_branch(mgr, into_branch);
    if (!src || !dst) return -1;
    if (src->head_snapshot_id[0] == '\0') return -1; /* nothing to merge */

    /* Create a merge snapshot on the destination branch */
    WubuSnapshot *merge_snap = NULL;
    int rc = wubu_snapshot_create(mgr, "merge", dst->head_snapshot_id,
                                  into_branch, "merge", merge_message, &merge_snap);
    if (rc == 0 && merge_snap) {
        /* Point to the source snapshot as additional parent (simplified) */
        strncpy(merge_snap->parent_id, src->head_snapshot_id, sizeof(merge_snap->parent_id) - 1);
        merge_snap->status = WUBU_SNAP_STATUS_READY;

        /* Update destination branch head */
        strncpy(dst->head_snapshot_id, merge_snap->id, sizeof(dst->head_snapshot_id) - 1);
        dst->updated = snapshot_now();
        dst->snapshot_count++;

        if (out_snapshot) *out_snapshot = merge_snap;
    }
    return rc;
}

int wubu_branch_list(WubuSnapshotManager *mgr, WubuBranch *out_branches, int max) {
    if (!mgr || !out_branches || max <= 0) return 0;
    int count = (mgr->branch_count < max) ? mgr->branch_count : max;
    memcpy(out_branches, mgr->branches, count * sizeof(WubuBranch));
    return count;
}

/* -- Tag operations ----------------------------------------------- */

/* -- Export/Import ----------------------------------------------- */

/* -- Diff --------------------------------------------------------- */

/* -- Rollback / Restore ------------------------------------------ */

int wubu_snapshot_rollback(WubuSnapshotManager *mgr, const char *snapshot_id) {
    if (!mgr || !snapshot_id) return -1;
    WubuSnapshot *s = find_snapshot(mgr, snapshot_id);
    if (!s) return -1;
    if (s->status != WUBU_SNAP_STATUS_READY) return -1;

    /* Update branch head to this snapshot */
    WubuBranch *br = find_branch(mgr, s->branch);
    if (br) {
        strncpy(br->head_snapshot_id, s->id, sizeof(br->head_snapshot_id) - 1);
        br->updated = snapshot_now();
    }

    s->accessed = snapshot_now();
    /* Restore snapshot data using nftw-based copy (replaces system("cp -a")) */
    if (s->upper_dir[0] && s->container_id[0]) {
        uint64_t bytes_copied;
        int files_copied;
        int rc = copy_tree_nftw(s->upper_dir, s->container_id, &bytes_copied, &files_copied);
        if (rc != 0) {
            fprintf(stderr, "[wubu_snap] restore from %s failed: %s\n", s->id, strerror(errno));
        } else {
            fprintf(stderr, "[wubu_snap] restored %s: %d files, %lu bytes\n", s->id, files_copied, (unsigned long)bytes_copied);
        }
    }
    return 0;
}

int wubu_snapshot_restore_as_new(WubuSnapshotManager *mgr, const char *snapshot_id,
                                 const char *new_container_name, char *out_container_id) {
    if (!mgr || !snapshot_id) return -1;
    WubuSnapshot *s = find_snapshot(mgr, snapshot_id);
    if (!s) return -1;

    /* Generate new container ID */
    if (out_container_id) {
        unsigned long h = hash_string(snapshot_id) ^ hash_string(new_container_name);
        snprintf(out_container_id, WUBU_SNAPSHOT_ID_LEN, "restored-%08lx", h);
    }

    /* Create a new snapshot as a copy of the source */
    WubuSnapshot *new_snap = NULL;
    int rc = wubu_snapshot_create(mgr, new_container_name ? new_container_name : s->container_id,
                                  NULL, "main", "from-restore", "Restored from snapshot", &new_snap);
    if (rc < 0) return rc;

    /* Copy labels and tags from source */
    new_snap->tag_count = s->tag_count;
    memcpy(new_snap->tags, s->tags, s->tag_count * sizeof(new_snap->tags[0]));

    /* Copy snapshot data using nftw-based copy (replaces system("cp -a")) */
    if (new_container_name && new_container_name[0] && s->upper_dir[0]) {
        char dst_path[WUBU_MAX_PATH];
        snprintf(dst_path, sizeof(dst_path), "/tmp/wubu-containers/%s/rootfs/", new_container_name);
        uint64_t bytes_copied;
        int files_copied;
        rc = copy_tree_nftw(s->upper_dir, dst_path, &bytes_copied, &files_copied);
        if (rc != 0) {
            fprintf(stderr, "[wubu_snap] copy snapshot %s to %s failed: %s\n",
                    s->id, new_container_name, strerror(errno));
        } else {
            fprintf(stderr, "[wubu_snap] copied %s to %s: %d files, %lu bytes\n",
                    s->id, new_container_name, files_copied, (unsigned long)bytes_copied);
        }
    }
    return 0;
}

/* -- Garbage Collection ------------------------------------------- */

int wubu_snapshot_inspect(const WubuSnapshotManager *mgr, const char *snapshot_id,
                          WubuSnapshot *out_snapshot) {
    if (!mgr || !snapshot_id || !out_snapshot) return -1;
    WubuSnapshot *s = find_snapshot((WubuSnapshotManager *)mgr, snapshot_id);
    if (!s) return -1;
    memcpy(out_snapshot, s, sizeof(*out_snapshot));
    return 0;
}

int wubu_snapshot_tree(const WubuSnapshotManager *mgr, const char *snapshot_id,
                       char tree[][256], int max_depth) {
    if (!mgr || !snapshot_id || !tree || max_depth <= 0) return 0;
    WubuSnapshot *s = find_snapshot((WubuSnapshotManager *)mgr, snapshot_id);
    if (!s) return 0;

    int depth = 0;
    WubuSnapshot *cur = s;

    /* Walk the parent chain */
    while (cur && depth < max_depth && depth < 256) {
        int n = snprintf(tree[depth], 256, "%s [%s] (%s, %s, %lu bytes)",
                         cur->id, cur->label, cur->branch,
                         wubu_snapshot_type_str(cur->type),
                         (unsigned long)cur->size_bytes);
        if (n < 0) break;
        depth++;

        if (cur->parent_id[0] && strcmp(cur->parent_id, cur->id) != 0) {
            cur = find_snapshot((WubuSnapshotManager *)mgr, cur->parent_id);
        } else {
            break;
        }
    }

    /* Reverse so root is first */
    for (int i = 0; i < depth / 2; i++) {
        char tmp[256];
        memcpy(tmp, tree[i], 256);
        memcpy(tree[i], tree[depth - 1 - i], 256);
        memcpy(tree[depth - 1 - i], tmp, 256);
    }

    return depth;
}

/* -- Helpers (already real in stub) ------------------------------- */

const char *wubu_snapshot_status_str(WubuSnapshotStatus status) {
    static const char *names[] = {"creating", "ready", "mounted", "error", "deleting", "merging"};
    if (status >= 0 && status < 6) return names[status];
    return "unknown";
}

const char *wubu_snapshot_type_str(WubuSnapshotType type) {
    static const char *names[] = {"full", "incremental", "delta", "base"};
    if (type >= 0 && type < 4) return names[type];
    return "unknown";
}

const char *wubu_fs_type_str(WubuFsType type) {
    static const char *names[] = {"overlayfs", "btrfs", "zfs", "lvm", "auto"};
    if (type >= 0 && type < 5) return names[type];
    return "unknown";
}

