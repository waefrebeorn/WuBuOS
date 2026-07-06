/*
 * wubu_snapshot_internal.h  --  WuBuOS Snapshot Internal Helpers
 *
 * Extracted from wubu_snapshot.c (2026-07-06): shared internal
 * helpers, ID generation, path utilities, copy operations.
 *
 * C11 only. Used by wubu_snapshot.c, wubu_snapshot_fs.c,
 * and future extracted modules.
 */
#ifndef WUBU_SNAPSHOT_INTERNAL_H
#define WUBU_SNAPSHOT_INTERNAL_H

#include "wubu_snapshot.h"
#include "wubu_snapshot_fs.h"

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ftw.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/limits.h>
/* FTW_MOUNT, FTW_DEPTH, FTW_PHYS require _GNU_SOURCE on some systems */
#ifndef FTW_MOUNT
#define FTW_MOUNT 0x0100
#endif
#ifndef FTW_DEPTH
#define FTW_DEPTH 0x0001
#endif
#ifndef FTW_PHYS
#define FTW_PHYS 0x0002
#endif
#ifndef FTW_NS
#define FTW_NS 0x0004
#endif
#ifndef FTW_SL
#define FTW_SL 0x0010
#endif
#ifndef FTW_SLN
#define FTW_SLN 0x0010
#endif
#ifndef FTW_DP
#define FTW_DP 0x0004
#endif

/* -- Time/ID helpers ----------------------------------------------- */

static inline uint64_t snapshot_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static inline void gen_snapshot_id(char *out, size_t len) {
    snprintf(out, len, "snap-%lx-%lx",
             snapshot_now() & 0xFFFFFFFFFFFF, (unsigned long)rand() & 0xFFFF);
}

static inline unsigned long hash_string(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = *s++)) h = ((h << 5) + h) + c;
    return h;
}

static inline void gen_snapshot_id_from_data(char *out, size_t len,
                                              const char *container_id,
                                              const char *parent_id,
                                              uint64_t timestamp) {
    unsigned long h = hash_string(container_id);
    if (parent_id) h ^= hash_string(parent_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
    snprintf(out, len, "snap-%lx-%lx-%lx",
             h & 0xFFFFFFFF, timestamp & 0xFFFFFFFF, (unsigned long)rand() & 0xFFFF);
}

/* -- Manager lookup helpers ---------------------------------------- */

static inline WubuSnapshot *find_snapshot(WubuSnapshotManager *mgr, const char *id) {
    if (!mgr || !id) return NULL;
    for (int i = 0; i < mgr->snapshot_count; i++) {
        if (strcmp(mgr->snapshots[i].id, id) == 0)
            return &mgr->snapshots[i];
    }
    return NULL;
}

static inline WubuBranch *find_branch(WubuSnapshotManager *mgr, const char *name) {
    if (!mgr || !name) return NULL;
    for (int i = 0; i < mgr->branch_count; i++) {
        if (strcmp(mgr->branches[i].name, name) == 0)
            return &mgr->branches[i];
    }
    return NULL;
}

static inline WubuTag *find_tag(WubuSnapshotManager *mgr, const char *name) {
    if (!mgr || !name) return NULL;
    for (int i = 0; i < mgr->tag_count; i++) {
        if (strcmp(mgr->tags[i].name, name) == 0)
            return &mgr->tags[i];
    }
    return NULL;
}

/* -- Path/size utilities ------------------------------------------- */

static inline void snapshot_default(WubuSnapshot *s) {
    if (!s) return;
    s->status = WUBU_SNAP_STATUS_CREATING;
    s->type = WUBU_SNAP_TYPE_FULL;
    s->fs_type = WUBU_FS_OVERLAYFS;
    s->tag_count = 0;
    s->branch[0] = '\0';
}

static inline void build_snapshot_path(WubuSnapshotManager *mgr,
                                        const char *snapshot_id,
                                        char *out_path, size_t out_len,
                                        void *flags) {
    (void)flags;
    snprintf(out_path, out_len, "%s/snapshots/%s",
             mgr->root_path, snapshot_id);
}

static inline int ensure_dir(const char *path) {
    if (!path || !path[0]) return -EINVAL;
    if (mkdir(path, 0755) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -errno;
}

static inline int ensure_snapshot_dirs(WubuSnapshotManager *mgr,
                                        const char *snapshot_id) {
    char path[WUBU_MAX_PATH];
    snprintf(path, sizeof(path), "%s/snapshots/%s", mgr->root_path, snapshot_id);
    if (ensure_dir(path) < 0) return -1;
    snprintf(path, sizeof(path), "%s/snapshots/%s/upper", mgr->root_path, snapshot_id);
    if (ensure_dir(path) < 0) return -1;
    snprintf(path, sizeof(path), "%s/snapshots/%s/work", mgr->root_path, snapshot_id);
    if (ensure_dir(path) < 0) return -1;
    snprintf(path, sizeof(path), "%s/snapshots/%s/merged", mgr->root_path, snapshot_id);
    if (ensure_dir(path) < 0) return -1;
    return 0;
}

/* dir_size is implemented in wubu_snapshot.c */
uint64_t dir_size(const char *path);

/* -- nftw-based recursive copy ------------------------------------- */
int copy_tree_nftw(const char *src, const char *dst,
                   uint64_t *out_bytes, int *out_files);

#endif /* WUBU_SNAPSHOT_INTERNAL_H */