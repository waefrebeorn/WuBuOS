/* wubu_proton2_launch.c -- WuBuOS proton2: app process launch/control.
 * Extracted from wubu_proton2.c (separable leaf). Self-contained: builds the
 * wine command, drives the container (wubu_ct_set_cmd/start/wait), tracks
 * per-app run state. C11, minimal includes.
 */
#include "wubu_proton2.h"

#include <string.h>
#include <stdio.h>
#include <signal.h>

int wubu_proton_launch(WubuProtonManager *mgr, int app_idx) {
    if (!mgr || !mgr->container_running) return -1;
    if (app_idx < 0 || app_idx >= mgr->n_apps) return -1;
    WubuProtonApp *app = &mgr->apps[app_idx];

    /* Build Wine command */
    char cmd[2048];
    const char *wine = mgr->global.wine_path[0] ? mgr->global.wine_path : "/usr/bin/wine";
    if (app->args[0]) {
        snprintf(cmd, sizeof(cmd), "%s '%s' %s", wine, app->exe_path, app->args);
    } else {
        snprintf(cmd, sizeof(cmd), "%s '%s'", wine, app->exe_path);
    }

    /* Set container command */
    char *argv[4] = { "/bin/bash", "-c", cmd, NULL };
    wubu_ct_set_cmd(mgr->container, 3, argv);

    /* Start */
    int ret = wubu_ct_start(mgr->container);
    if (ret == 0) {
        app->running = true;
        app->pid = mgr->container->pid;
        mgr->apps_launched++;
    }
    return ret;
}

int wubu_proton_launch_name(WubuProtonManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    for (int i = 0; i < mgr->n_apps; i++) {
        if (strcmp(mgr->apps[i].name, name) == 0)
            return wubu_proton_launch(mgr, i);
    }
    return -1;
}

int wubu_proton_terminate(WubuProtonManager *mgr, int app_idx) {
    if (!mgr || app_idx < 0 || app_idx >= mgr->n_apps) return -1;
    WubuProtonApp *app = &mgr->apps[app_idx];
    if (!app->running) return -1;
    if (app->pid > 0) kill(app->pid, SIGTERM);
    app->running = false;
    return 0;
}

int wubu_proton_wait(WubuProtonManager *mgr, int app_idx) {
    if (!mgr || !mgr->container) return -1;
    int code = wubu_ct_wait(mgr->container);
    if (app_idx >= 0 && app_idx < mgr->n_apps) {
        mgr->apps[app_idx].running = false;
        mgr->apps[app_idx].exit_code = code;
    }
    mgr->apps_exited++;
    return code;
}
