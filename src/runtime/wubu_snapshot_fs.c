/*
 * wubu_snapshot_fs.c  --  WuBuOS Filesystem Native Snapshot Operations
 *
 * Extracted from wubu_snapshot.c (2026-07-06): btrfs/zfs/lvm native
 * snapshot creation and overlayfs mount/unmount logic.
 *
 * This module handles the filesystem-specific operations that were
 * inline in the monolithic file. It provides a unified interface
 * for creating and managing filesystem-level snapshots.
 *
 * C11 only. Linux-specific (btrfs/zfs/overlayfs ioctls).
 * Compiled only on __linux__.
 */

#ifdef __linux__

#include "wubu_snapshot.h"
#include "wubu_snapshot_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <libgen.h>  /* basename, dirname */
#include <sys/wait.h>
#include <linux/btrfs.h>  /* BTRFS_IOC_SNAP_CREATE_V2, BTRFS_PATH_NAME_MAX */

/* BTRFS_SUPER_MAGIC is defined in <linux/magic.h> but may not be available */
#ifndef BTRFS_SUPER_MAGIC
#define BTRFS_SUPER_MAGIC 0x9123683E
#endif

/* -- btrfs/zfs/lvm detection --------------------------------------- */

bool is_btrfs(const char *path) {
    struct statfs fs;
    if (statfs(path, &fs) < 0) return false;
    return fs.f_type == BTRFS_SUPER_MAGIC;
}

bool is_zfs(const char *path) {
    struct statfs fs;
    if (statfs(path, &fs) < 0) return false;
    return fs.f_type == 0x2fc12fc1; /* ZFS_SUPER_MAGIC */
}

/* -- btrfs subvolume snapshot -------------------------------------- */

int btrfs_snapshot_create(const char *src, const char *dst) {
    int fd = open(dst, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        /* Try creating parent dir first */
        char *parent = strdup(dst);
        if (!parent) return -1;
        char *d = dirname(parent);
        mkdir(d, 0755);
        free(parent);
        fd = open(dst, O_RDONLY | O_DIRECTORY);
        if (fd < 0) return -1;
    }

    struct btrfs_ioctl_vol_args_v2 args = {0};
    const char *src_base = basename((char *)src);
    size_t name_len = strlen(src_base);
    if (name_len >= BTRFS_PATH_NAME_MAX) name_len = BTRFS_PATH_NAME_MAX - 1;
    memcpy(args.name, src_base, name_len);
    args.name[name_len] = '\0';
    args.flags = 0; /* read-only snapshot */

    int rc = ioctl(fd, BTRFS_IOC_SNAP_CREATE_V2, &args);
    close(fd);

    if (rc < 0) {
        fprintf(stderr, "[wubu_snap] btrfs snapshot create failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/* -- zfs snapshot (via zfs userspace tools) ------------------------ */

int zfs_snapshot_create(const char *src, const char *dst) {
    char *src_copy = strdup(src);
    if (!src_copy) return -1;
    char *dataset = basename(src_copy);

    char *dst_copy = strdup(dst);
    if (!dst_copy) { free(src_copy); return -1; }
    char *snapshot_name = basename(dst_copy);

    pid_t pid = fork();
    if (pid < 0) {
        free(src_copy);
        free(dst_copy);
        return -1;
    }
    if (pid == 0) {
        execlp("zfs", "zfs", "snapshot", dataset, snapshot_name, (char *)NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    free(src_copy);
    free(dst_copy);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

/* -- lvm thin snapshot (lvcreate --snapshot) ---------------------- */

int lvm_snapshot_create(const char *src, const char *dst) {
    /* src: existing LV path, e.g. /dev/vg0/root
     * dst: snapshot LV path, e.g. /dev/vg0/root-snap
     * Use `lvcreate --snapshot --name <snap> --size <extent> <origin>` */
    char *src_copy = strdup(src);
    if (!src_copy) return -1;
    char *origin = basename(src_copy);

    char *dst_copy = strdup(dst);
    if (!dst_copy) { free(src_copy); return -1; }
    char *snap = basename(dst_copy);

    pid_t pid = fork();
    if (pid < 0) {
        free(src_copy);
        free(dst_copy);
        return -1;
    }
    if (pid == 0) {
        /* lvcreate --snapshot -n <snap> -L 1G <origin> */
        execlp("lvcreate", "lvcreate", "--snapshot",
               "-n", snap, "-L", "1G", origin, (char *)NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    free(src_copy);
    free(dst_copy);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

/* -- Mount filesystem-appropriate snapshot ------------------------- */

int snapshot_mount_fs(WubuSnapshot *s, WubuSnapshotManager *mgr) {
    if (!s || !s->lower_dir[0] || !s->upper_dir[0] || !s->work_dir[0] || !s->merged_dir[0]) {
        return -EINVAL;
    }

    if (mkdir(s->merged_dir, 0755) < 0 && errno != EEXIST) {
        fprintf(stderr, "[wubu_snap] mkdir %s failed: %s\n", s->merged_dir, strerror(errno));
        return -errno;
    }

    if (mgr->fs_type == WUBU_FS_BTRFS && is_btrfs(s->lower_dir)) {
        return btrfs_snapshot_create(s->lower_dir, s->upper_dir);
    } else if (mgr->fs_type == WUBU_FS_ZFS && is_zfs(s->lower_dir)) {
        return zfs_snapshot_create(s->lower_dir, s->upper_dir);
    } else if (mgr->fs_type == WUBU_FS_LVM) {
        return lvm_snapshot_create(s->lower_dir, s->upper_dir);
    } else {
        char mount_opts[4096];
        snprintf(mount_opts, sizeof(mount_opts),
                 "lowerdir=%s,upperdir=%s,workdir=%s",
                 s->lower_dir, s->upper_dir, s->work_dir);
        int rc = mount("overlay", s->merged_dir, "overlay", 0, mount_opts);
        if (rc != 0) {
            if (errno == ENOSYS || errno == ENODEV || errno == EOPNOTSUPP) {
                fprintf(stderr, "[wubu_snap] overlayfs not supported, trying bind mount fallback\n");
                rc = mount(s->lower_dir, s->merged_dir, NULL, MS_BIND | MS_REC, NULL);
                if (rc == 0) {
                    rc = mount(s->upper_dir, s->merged_dir, NULL, MS_BIND | MS_REC, NULL);
                }
            }
            if (rc != 0) {
                fprintf(stderr, "[wubu_snap] mount on %s failed: %s\n", s->merged_dir, strerror(errno));
                return -errno;
            }
        }
        return 0;
    }
}

/* -- Unmount filesystem-appropriate snapshot ----------------------- */

int snapshot_unmount_fs(WubuSnapshot *s) {
    if (!s || !s->merged_dir[0]) return -EINVAL;

    int rc = umount2(s->merged_dir, MNT_DETACH);
    if (rc != 0 && errno != EINVAL && errno != ENOENT && errno != ENOTCONN) {
        fprintf(stderr, "[wubu_snap] umount %s failed: %s\n", s->merged_dir, strerror(errno));
        return -errno;
    }
    return 0;
}

#else /* __linux__ */

int snapshot_mount_fs(WubuSnapshot *s, WubuSnapshotManager *mgr) {
    (void)mgr;
    if (!s || !s->lower_dir[0] || !s->upper_dir[0] || !s->work_dir[0] || !s->merged_dir[0]) {
        return -EINVAL;
    }
    char mount_opts[2048];
    snprintf(mount_opts, sizeof(mount_opts),
             "lowerdir=%s,upperdir=%s,workdir=%s",
             s->lower_dir, s->upper_dir, s->work_dir);
    int rc = mount("overlay", s->merged_dir, "overlay", 0, mount_opts);
    if (rc != 0) {
        fprintf(stderr, "[wubu_snap] mount on %s failed: %s\n", s->merged_dir, strerror(errno));
        return -errno;
    }
    return 0;
}

int snapshot_unmount_fs(WubuSnapshot *s) {
    if (!s || !s->merged_dir[0]) return -EINVAL;
    int rc = umount2(s->merged_dir, MNT_DETACH);
    if (rc != 0 && errno != EINVAL && errno != ENOENT) {
        fprintf(stderr, "[wubu_snap] umount %s failed: %s\n", s->merged_dir, strerror(errno));
        return -errno;
    }
    return 0;
}

#endif /* __linux__ */