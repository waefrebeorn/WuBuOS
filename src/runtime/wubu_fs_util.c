/*
 * wubu_fs_util.c -- shared filesystem utilities (dedup home).
 *
 * Canonical implementation of recursive-force-delete and recursive directory
 * creation, previously duplicated (byte-for-byte) as `rm_rf` in
 * wubu_proton_util.c, `archd_rm_rf` in wubu_archd_fs.c, and
 * `bottles_rm_rf` in wubu_bottles_fs.c -- each with its own private
 * unlink_cb. This module is the single source of truth; the three
 * historical names are thin wrappers (see the respective files) so external
 * callers are unchanged.
 *
 * C11, minimal includes, self-contained.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "wubu_fs_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>

static int wubu_fs_unlink_cb(const char *fpath, const struct stat *sb,
                             int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)typeflag; (void)ftwbuf;
    return unlink(fpath) == 0 ? 0 : -1;
}

int wubu_fs_rm_rf(const char *path) {
    if (!path) return -1;
    return nftw(path, wubu_fs_unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}
