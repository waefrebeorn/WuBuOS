/*
 * wubu_snapshot_fs.h  --  WuBuOS Snapshot Filesystem Operations
 *
 * Extracted from wubu_snapshot.c (2026-07-06): filesystem-specific
 * mount/unmount and native snapshot creation (btrfs, zfs, overlayfs).
 *
 * C11 only. Linux-only code is #ifdef __linux__.
 */
#ifndef WUBU_SNAPSHOT_FS_H
#define WUBU_SNAPSHOT_FS_H

#include "wubu_snapshot.h"

#ifdef __linux__
/* Check if path is on btrfs/zfs */
bool is_btrfs(const char *path);
bool is_zfs(const char *path);

/* Native snapshot creation */
int btrfs_snapshot_create(const char *src, const char *dst);
int zfs_snapshot_create(const char *src, const char *dst);
int lvm_snapshot_create(const char *src, const char *dst);
#endif

/* Mount/unmount snapshot using filesystem-appropriate method */
int snapshot_mount_fs(WubuSnapshot *s, WubuSnapshotManager *mgr);
int snapshot_unmount_fs(WubuSnapshot *s);

#endif /* WUBU_SNAPSHOT_FS_H */