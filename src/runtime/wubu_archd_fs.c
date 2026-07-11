/* wubu_archd_fs.c -- WuBuOS archd: recursive filesystem delete.
 * Extracted from wubu_archd.c (separable leaf). Self-contained: ftw only.
 * C11, minimal includes.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "wubu_archd.h"
#include "wubu_archd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>
#include <sys/stat.h>

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)typeflag; (void)ftwbuf;
    return unlink(fpath) == 0 ? 0 : -1;
}

int archd_rm_rf(const char *path) {
    if (!path) return -1;
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

/* -- Helper: run command via fork+exec (no system()) ---------------- */


/* -- Helper: run command in chroot via fork+exec -------------------- */


/* -- Helper: write file --------------------------------------------- */


/* -- Helper: mkdir -p ----------------------------------------------- */


/* -- String Tables ----------------------------------------------- */

const char *wubu_archd_root_state_str(WubuArchRootState state) {
    switch (state) {
        case ROOT_STATE_INACTIVE:    return "inactive";
        case ROOT_STATE_ACTIVATING:  return "activating";
        case ROOT_STATE_ACTIVE:      return "active";
        case ROOT_STATE_DEACTIVATING:return "deactivating";
        case ROOT_STATE_FAILED:      return "failed";
        case ROOT_STATE_MAINTENANCE: return "maintenance";
        case ROOT_STATE_SNAPSHOT:    return "snapshot";
        default:                     return "unknown";
    }
}

const char *wubu_archd_root_type_str(WubuArchRootType type) {
    switch (type) {
        case ROOT_TYPE_BASE:     return "base";
        case ROOT_TYPE_GUI:      return "gui";
        case ROOT_TYPE_STEAM:    return "steam";
        case ROOT_TYPE_GAMING:   return "gaming";
        case ROOT_TYPE_PROTON:   return "proton";
        case ROOT_TYPE_DEVELOP:  return "develop";
        case ROOT_TYPE_CUSTOM:   return "custom";
        default:                 return "unknown";
    }
}

const char *wubu_archd_svc_state_str(WubuArchServiceState state) {
    switch (state) {
        case SERVICE_STATE_DISABLED:  return "disabled";
        case SERVICE_STATE_ENABLED:   return "enabled";
        case SERVICE_STATE_RUNNING:   return "running";
        case SERVICE_STATE_FAILED:    return "failed";
        case SERVICE_STATE_RESTARTING:return "restarting";
        default:                      return "unknown";
    }
}

const char *wubu_archd_cmd_str(WubuArchdCmd cmd) {
    switch (cmd) {
        case ARCHD_CMD_ROOT_CREATE:  return "root_create";
        case ARCHD_CMD_ROOT_DESTROY: return "root_destroy";
        case ARCHD_CMD_ROOT_LIST:    return "root_list";
        case ARCHD_CMD_ROOT_INFO:    return "root_info";
        case ARCHD_CMD_ROOT_CLONE:   return "root_clone";
        case ARCHD_CMD_ROOT_SNAPSHOT:return "root_snapshot";
        case ARCHD_CMD_ROOT_ROLLBACK:return "root_rollback";
        case ARCHD_CMD_PKG_INSTALL:  return "pkg_install";
        case ARCHD_CMD_PKG_REMOVE:   return "pkg_remove";
        case ARCHD_CMD_PKG_UPDATE:   return "pkg_update";
        case ARCHD_CMD_SVC_ENABLE:   return "svc_enable";
        case ARCHD_CMD_SVC_DISABLE:  return "svc_disable";
        case ARCHD_CMD_SVC_START:    return "svc_start";
        case ARCHD_CMD_SVC_STOP:     return "svc_stop";
        case ARCHD_CMD_SVC_RESTART:  return "svc_restart";
        case ARCHD_CMD_SVC_STATUS:   return "svc_status";
        case ARCHD_CMD_SVC_LIST:     return "svc_list";
        case ARCHD_CMD_PING:         return "ping";
        case ARCHD_CMD_STATS:        return "stats";
        case ARCHD_CMD_HEALTH:       return "health";
        case ARCHD_CMD_GPU_DETECT:   return "gpu_detect";
        case ARCHD_CMD_GPU_LIST:     return "gpu_list";
        case ARCHD_CMD_GPU_ASSIGN:   return "gpu_assign";
        case ARCHD_CMD_SHUTDOWN:     return "shutdown";
        case ARCHD_CMD_RELOAD:       return "reload";
        case ARCHD_CMD_VERSION:      return "version";
        default:                     return "unknown";
    }
}

const char *wubu_archd_version(void) {
    return WUBU_ARCHD_VERSION;
}

/* -- Logging ------------------------------------------------------ */

