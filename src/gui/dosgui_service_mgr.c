/*
 * dosgui_service_mgr.c -- Desktop-side service/autostart manager (E3).
 *
 * Real wiring of wubu_archd (16/16) as the Desktop's service manager.
 * See dosgui_service_mgr.h for the contract.
 *
 * C11, no nested functions.
 */
#include "dosgui_service_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef DOSGUI_SVC_AUTOSTART_CONF
#define DOSGUI_SVC_AUTOSTART_CONF "/etc/wubu/archd-autostart.conf"
#endif

static WubuArchd        g_archd;
static bool             g_active = false;
static DosguiAutostartEntry g_autostart[DOSGUI_SVC_MGR_MAX_AUTOSTART];
static int              g_autostart_count = 0;

/* ---- internals -------------------------------------------------- */

static int find_entry(const char *root, const char *svc) {
    for (int i = 0; i < g_autostart_count; i++) {
        if (strcmp(g_autostart[i].root, root) == 0 &&
            strcmp(g_autostart[i].svc, svc) == 0) {
            return i;
        }
    }
    return -1;
}

/* Load autostart entries from the config file:
 *   one entry per non-empty, non-comment line: "<root>:<service>"   */
static void load_autostart_conf(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        /* strip trailing newline / CR */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        /* skip blanks / comments */
        if (len == 0 || line[0] == '#') continue;
        /* find ':' separator */
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        const char *root = line;
        const char *svc = colon + 1;
        if (!*root || !*svc) continue;
        dosgui_service_mgr_register_autostart(root, svc);
    }
    fclose(fp);
}

/* ---- public API ------------------------------------------------- */

int dosgui_service_mgr_init(void) {
    if (g_active) return 0;

    WubuArchdConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    /* Defaults come from wubu_archd_init; we only set what we need. */
    strncpy(cfg.roots_path, WUBU_ARCHD_ROOTS_PATH, sizeof(cfg.roots_path) - 1);
    strncpy(cfg.socket_path, WUBU_ARCHD_SOCKET_PATH, sizeof(cfg.socket_path) - 1);
    cfg.max_roots = WUBU_ARCHD_MAX_ROOTS;
    cfg.health_check_interval_sec = 60;
    cfg.update_check_interval_sec = 3600;
    cfg.log_level = 2;

    if (wubu_archd_init(&g_archd, &cfg) != 0) {
        return -1;
    }

    g_autostart_count = 0;
    load_autostart_conf(DOSGUI_SVC_AUTOSTART_CONF);

    g_active = true;
    return 0;
}

int dosgui_service_mgr_register_autostart(const char *root, const char *svc) {
    if (!root || !svc || !*root || !*svc) return -1;
    if (g_autostart_count >= DOSGUI_SVC_MGR_MAX_AUTOSTART) return -1;
    if (find_entry(root, svc) >= 0) {
        return find_entry(root, svc); /* already registered */
    }
    DosguiAutostartEntry *e = &g_autostart[g_autostart_count];
    memset(e, 0, sizeof(*e));
    strncpy(e->root, root, sizeof(e->root) - 1);
    strncpy(e->svc, svc, sizeof(e->svc) - 1);
    e->booted = false;
    e->last_result = 0;
    return g_autostart_count++;
}

DosguiBootResult dosgui_service_mgr_boot(void) {
    DosguiBootResult res;
    memset(&res, 0, sizeof(res));

    for (int i = 0; i < g_autostart_count; i++) {
        DosguiAutostartEntry *e = &g_autostart[i];
        res.attempted++;
        e->booted = true;
        /* Real work: drive wubu_archd to start the service in its root.
         * On a real host this runs `arch-chroot <root> systemctl start <svc>`.
         * In a sandbox without Arch roots it returns <0, which we record. */
        int r = wubu_archd_svc_start(&g_archd, e->root, e->svc);
        e->last_result = r;
        if (r == 0) res.started++;
        else        res.failed++;
    }
    return res;
}

void dosgui_service_mgr_stop_all(void) {
    for (int i = 0; i < g_autostart_count; i++) {
        DosguiAutostartEntry *e = &g_autostart[i];
        if (e->booted && e->last_result == 0) {
            wubu_archd_svc_stop(&g_archd, e->root, e->svc);
        }
        e->booted = false;
    }
}

void dosgui_service_mgr_shutdown(void) {
    if (!g_active) return;
    dosgui_service_mgr_stop_all();
    wubu_archd_shutdown(&g_archd);
    g_active = false;
    g_autostart_count = 0;
}

int dosgui_service_mgr_count(void) {
    return g_autostart_count;
}

const DosguiAutostartEntry *dosgui_service_mgr_entry(int i) {
    if (i < 0 || i >= g_autostart_count) return NULL;
    return &g_autostart[i];
}

WubuArchd *dosgui_service_mgr_handle(void) {
    return g_active ? &g_archd : NULL;
}

bool dosgui_service_mgr_active(void) {
    return g_active;
}
