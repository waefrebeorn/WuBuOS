/*
 * wubu_fs_util.h -- shared filesystem utilities (dedup home).
 * See wubu_fs_util.c. C11 opaque-free, minimal public API.
 */
#ifndef WUBU_FS_UTIL_H
#define WUBU_FS_UTIL_H

/* Recursively delete path and everything under it (like `rm -rf`).
 * Returns 0 on success, -1 on error (e.g. NULL path). */
int wubu_fs_rm_rf(const char *path);

#endif /* WUBU_FS_UTIL_H */
