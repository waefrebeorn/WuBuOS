/* wubu_bottles_fs.c -- WuBuOS bottles: recursive filesystem delete.
 * Extracted from wubu_bottles.c (separable leaf). Self-contained: ftw only.
 * C11, minimal includes.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "wubu_bottles.h"
#include "wubu_bottles_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ftw.h>
#include <sys/stat.h>

/* Forward decl (static helper used by bottles_rm_rf) */
static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)typeflag; (void)ftwbuf;
    return unlink(fpath) == 0 ? 0 : -1;
}

int bottles_rm_rf(const char *path) {
    if (!path) return -1;
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

