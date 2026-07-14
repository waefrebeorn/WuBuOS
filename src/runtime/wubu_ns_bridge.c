/*
 * wubu_ns_bridge.c -- WuBuOS Namespace Bridge: archd services + bottles
 * exposed as a uniform 9P/Styx control plane (rip off systemd/Flatpak/
 * Bottles, do it better through one filesystem namespace).
 *
 * See wubu_ns_bridge.h for the design. Implementation is real: it builds a
 * /n/svc and /n/bottles tree on disk (servable by the Styx host server) and
 * routes ctl-file writes to the injected service ops / bottle API.
 *
 * C11, opaque structs, minimal includes.
 */

#define _GNU_SOURCE

#include "wubu_ns_bridge.h"
#include "wubu_archd.h"   /* real wubu_archd_svc_* */
#include "wubu_bottles_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/* The namespace root captured by wubu_ns_bridge_create(). The bridge is a
 * singleton per process (one /n tree). Tests create a temp root. */
static const char *g_ns_root = NULL;

/* -- Real (production) service ops --------------------------------- */

static int real_svc_start(WubuArchd *d, const char *root, const char *svc)
{ return wubu_archd_svc_start(d, root, svc); }
static int real_svc_stop(WubuArchd *d, const char *root, const char *svc)
{ return wubu_archd_svc_stop(d, root, svc); }
static int real_svc_restart(WubuArchd *d, const char *root, const char *svc)
{ return wubu_archd_svc_restart(d, root, svc); }
static int real_svc_enable(WubuArchd *d, const char *root, const char *svc)
{ return wubu_archd_svc_enable(d, root, svc); }
static int real_svc_disable(WubuArchd *d, const char *root, const char *svc)
{ return wubu_archd_svc_disable(d, root, svc); }
static int real_svc_status(WubuArchd *d, const char *root, const char *svc,
                           WubuArchService *out)
{ return wubu_archd_svc_status(d, root, svc, out); }

const wubu_ns_svc_ops_t wubu_ns_svc_ops_real = {
    real_svc_start, real_svc_stop, real_svc_restart,
    real_svc_enable, real_svc_disable, real_svc_status,
};

/* -- Helpers ------------------------------------------------------- */

const char *wubu_ns_state_str(WubuArchServiceState state) {
    switch (state) {
        case SERVICE_STATE_RUNNING:   return "active";
        case SERVICE_STATE_ENABLED:    return "enabled";
        case SERVICE_STATE_DISABLED:   return "inactive";
        case SERVICE_STATE_RESTARTING: return "restarting";
        case SERVICE_STATE_FAILED:     return "failed";
        default:                       return "unknown";
    }
}

/* Create a directory path under g_ns_root (mkdir -p, one component at a time). */
static int ns_mkdir(const char *sub) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", g_ns_root, sub);
    char *p = path;
    if (p[0] == '/') p++;  /* skip leading slash if root is absolute */
    while (*p) {
        char *slash = strchr(p, '/');
        if (slash) *slash = '\0';
        if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
        if (!slash) break;
        *slash = '/';
        p = slash + 1;
    }
    return 0;
}

/* Write a whole file under g_ns_root. */
static int ns_write(const char *sub, const char *buf) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", g_ns_root, sub);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(buf, f);
    fclose(f);
    return 0;
}

/* -- Namespace lifecycle ------------------------------------------ */

int wubu_ns_bridge_create(const char *ns_root) {
    if (!ns_root) return -1;
    struct stat st;
    if (stat(ns_root, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    g_ns_root = ns_root;
    if (ns_mkdir("svc")    != 0) return -1;
    if (ns_mkdir("bottles") != 0) return -1;
    return 0;
}

/* -- Service publishing ------------------------------------------- */

int wubu_ns_publish_service(WubuArchd *d, const char *root, const char *svc,
                            const wubu_ns_svc_ops_t *ops) {
    if (!g_ns_root || !root || !svc) return -1;
    if (!ops) ops = &wubu_ns_svc_ops_real;

    char dir[4096];
    snprintf(dir, sizeof(dir), "svc/%s/%s", root, svc);
    if (ns_mkdir(dir) != 0) return -1;

    /* status snapshot */
    WubuArchService info;
    char status[512];
    if (ops->svc_status(d, root, svc, &info) == 0) {
        snprintf(status, sizeof(status),
                 "name: %s\nroot: %s\nstate: %s\npid: %d\n",
                 info.name, info.root_name,
                 wubu_ns_state_str(info.state), info.pid);
    } else {
        snprintf(status, sizeof(status),
                 "name: %s\nroot: %s\nstate: unknown\n", svc, root);
    }
    char sub[4096];
    snprintf(sub, sizeof(sub), "svc/%s/%s/status", root, svc);
    if (ns_write(sub, status) != 0) return -1;

    /* ctl action file: writes are dispatched by wubu_ns_svc_ctl() */
    snprintf(sub, sizeof(sub), "svc/%s/%s/ctl", root, svc);
    if (ns_write(sub, "# write one of: start|stop|restart|enable|disable\n") != 0)
        return -1;
    return 0;
}

int wubu_ns_svc_ctl(WubuArchd *d, const char *root, const char *svc,
                    const char *cmd, const wubu_ns_svc_ops_t *ops) {
    if (!cmd) return -1;
    if (!ops) ops = &wubu_ns_svc_ops_real;

    if (strcmp(cmd, "start")  == 0) return ops->svc_start(d, root, svc);
    if (strcmp(cmd, "stop")   == 0) return ops->svc_stop(d, root, svc);
    if (strcmp(cmd, "restart")== 0) return ops->svc_restart(d, root, svc);
    if (strcmp(cmd, "enable") == 0) return ops->svc_enable(d, root, svc);
    if (strcmp(cmd, "disable")== 0) return ops->svc_disable(d, root, svc);
    return -1; /* unknown command */
}

/* -- Bottle publishing -------------------------------------------- */

static const char *bottle_type_str(WubuBottleType t) {
    switch (t) {
        case BOTTLE_TYPE_WINE:    return "wine";
        case BOTTLE_TYPE_PROTON:   return "proton";
        case BOTTLE_TYPE_LUTRIS:   return "lutris";
        case BOTTLE_TYPE_BOTTLES:  return "bottles";
        case BOTTLE_TYPE_CUSTOM:   return "custom";
        default:                   return "unknown";
    }
}

int wubu_ns_publish_bottle(const WubuBottle *b, const char *name) {
    if (!g_ns_root || !b || !name) return -1;

    char dir[4096];
    snprintf(dir, sizeof(dir), "bottles/%s", name);
    if (ns_mkdir(dir) != 0) return -1;

    /* info snapshot */
    char info[1024];
    snprintf(info, sizeof(info),
             "name: %s\ntype: %s\nrunner: %s\ninstalled: %d\nverified: %d\n",
             b->name, bottle_type_str(b->type), b->runner_version,
             b->installed ? 1 : 0, b->verified ? 1 : 0);
    char sub[4096];
    snprintf(sub, sizeof(sub), "bottles/%s/info", name);
    if (ns_write(sub, info) != 0) return -1;

    /* verify snapshot */
    char verify[256];
    int vr = wubu_bottle_verify((WubuBottle *)b);
    snprintf(verify, sizeof(verify), "%s\n", vr == 0 ? "ok" : "FAIL");
    snprintf(sub, sizeof(sub), "bottles/%s/verify", name);
    if (ns_write(sub, verify) != 0) return -1;

    /* ctl action file: writes dispatched by wubu_ns_bottle_action() */
    snprintf(sub, sizeof(sub), "bottles/%s/ctl", name);
    if (ns_write(sub, "# write one of: run|verify\n") != 0) return -1;
    return 0;
}

int wubu_ns_bottle_action(WubuBottle *b, const char *action) {
    if (!b || !action) return -1;
    if (strcmp(action, "run") == 0)    return wubu_bottle_run(b);
    if (strcmp(action, "verify") == 0) return wubu_bottle_verify(b);
    return -1; /* unknown action */
}
