/* wubu_archd_internal.h -- Internal helpers shared by wubu_archd sub-modules.
 * Public API + types in wubu_archd.h (WUBU_ARCHD_MAX_CMD defined there).
 * The process/fs utilities live in wubu_archd_util.c and are declared here so
 * all submodules link the SAME implementation (no double-coding).
 */

#ifndef WUBU_ARCHD_INTERNAL_H
#define WUBU_ARCHD_INTERNAL_H

#include "wubu_archd.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* -- Process / fs helpers (wubu_archd_util.c) --------------------- */
int  run_cmd(const char *cmd);
int  run_chroot_cmd(const char *root, const char *fmt, ...);
bool archd_write_file(const char *path, const char *content);
int  archd_mkdir_p(const char *path, mode_t mode);

#endif /* WUBU_ARCHD_INTERNAL_H */
