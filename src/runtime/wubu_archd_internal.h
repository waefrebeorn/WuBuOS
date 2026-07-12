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
int  archd_rm_rf(const char *path);

/* -- Daemon core shared state/functions -------------------------- *
 * wubu_archd.c was split into wubu_archd_daemon.c (lifecycle + main)
 * and wubu_archd_svc.c (pkg/repo/svc/aur/hook/health/gpu mgmt). The
 * file-statics below are now shared across both siblings via this header.
 * Single definition each (no double-coding).                          */

/* Logging / pidfile / socket / root-scan shared by daemon + svc modules. */
void archd_log(WubuArchd *d, int level, const char *fmt, ...);
int  archd_write_pid(WubuArchd *d);
void archd_remove_pid(void);
int  archd_socket_create(WubuArchd *d);
int  archd_scan_roots(WubuArchd *d);

#endif /* WUBU_ARCHD_INTERNAL_H */
