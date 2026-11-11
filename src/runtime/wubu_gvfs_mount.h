/*
 * wubu_gvfs_mount.h -- GVfs: the mount table and path-to-mount resolution.
 *
 * A mount table maps a virtual path to the filesystem that owns it.  The rule
 * that matters is LONGEST PREFIX WINS: with mounts at "/mnt" and "/mnt/smb", the
 * path "/mnt/smb/photos/x.jpg" belongs to "/mnt/smb" -- the deeper mount -- and
 * not to "/mnt".  A first-match or shortest-prefix resolver hands the deeper
 * mount's files to the shallower filesystem, which is a sandbox escape.
 *
 * The other rules:
 *   * a mount root must be NORMALIZED: no "..", no trailing slash (except the
 *     root itself).  An unnormalized root makes prefix comparison ambiguous.
 *   * a mount root may be nested inside another, but a duplicate root is an
 *     ERROR, not a replace.
 *   * a path that matches no mount resolves to nothing -- never to the root
 *     mount by default.
 *   * the INNER path (the part handed to the owning filesystem) is always
 *     rooted, so a filesystem never sees a relative path.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_GVFS_MOUNT_H
#define WUBU_GVFS_MOUNT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WUBU_GVFS_MAX_MOUNTS 16
#define WUBU_GVFS_ROOT_MAX  512
#define WUBU_GVFS_SCHEME_MAX 32

typedef struct {
    char   root[WUBU_GVFS_ROOT_MAX];
    char   scheme[WUBU_GVFS_SCHEME_MAX];
    bool   read_only;
    bool   active;
} wubu_gvfs_mount_t;

typedef struct {
    uint32_t         count;
    wubu_gvfs_mount_t mounts[WUBU_GVFS_MAX_MOUNTS];
} wubu_gvfs_mount_table_t;

void wubu_gvfs_mount_table_init(wubu_gvfs_mount_table_t *t);

/* Mount.  Returns 0 on success, -1 on a bad root, a duplicate, or a full
 * table. */
int wubu_gvfs_mount(wubu_gvfs_mount_table_t *t, const char *root,
                    const char *scheme, bool read_only);

/* Unmount.  Returns -1 for an unknown root. */
int wubu_gvfs_unmount(wubu_gvfs_mount_table_t *t, const char *root);

/* Resolve a path to its mount.  `inner` receives the path RELATIVE to the
 * mount root, always rooted.  Returns NULL when no mount owns the path. */
const wubu_gvfs_mount_t *
wubu_gvfs_resolve(const wubu_gvfs_mount_table_t *t, const char *path,
                  char *inner, size_t n);

/* Is `path` inside this mount?  Exposed so a caller can check before an
 * operation rather than after. */
bool wubu_gvfs_mount_contains(const wubu_gvfs_mount_t *m, const char *path);

#endif /* WUBU_GVFS_MOUNT_H */
