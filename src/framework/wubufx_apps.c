/*
 * wubufx_apps.c -- WuBuFX application registry (real engines, no placeholders)
 *
 * Phase C binding: the desktop shell / start menu launch apps THROUGH
 * WuBuFX. Each app is a content-addressed, capability-scoped, EDR-disclosed
 * namespace that spawns a genuine engine (calc / control / explorer /
 * notepad / terminal / canvas / dos-box / EDR dash).
 */

#include "wubufx.h"
#include "../gui/dosgui_wm.h"   /* DosGuiWindow full definition */
#include "../apps/dosgui_apps.h"
#include "../runtime/wubu_edr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- WuBuFX app definitions --------------------------------------- */

typedef struct {
    const char *name;           /* registry key, e.g. "notepad" */
    const char *title;          /* display title, e.g. "Notepad" */
    const char *namespace_id;   /* e.g. "notepad" -> /app/notepad/ */
    WubuFxCap  caps;            /* least-privilege capability mask */
    DosGuiWindow *(*engine_launch)(void);  /* real engine entry point */
} WubufxAppDef;

static const WubufxAppDef g_wubufx_apps[] = {
    { "notepad",      "Notepad",      "notepad",
      WUBUFX_CAP_READ | WUBUFX_CAP_WRITE,
      dosgui_launch_notepad },

    { "calculator",   "Calculator",   "calculator",
      WUBUFX_CAP_READ | WUBUFX_CAP_EXEC,
      dosgui_launch_calculator },

    { "file_manager", "File Manager", "file_manager",
      WUBUFX_CAP_READ | WUBUFX_CAP_WRITE | WUBUFX_CAP_EXEC,
      dosgui_launch_file_manager },

    { "control_panel","Control Panel","control_panel",
      WUBUFX_CAP_READ | WUBUFX_CAP_WRITE,
      dosgui_launch_settings },

    { "terminal",     "Terminal",     "terminal",
      WUBUFX_CAP_READ | WUBUFX_CAP_WRITE | WUBUFX_CAP_EXEC,
      dosgui_launch_terminal },

    { "canvas",       "WuBu Canvas",  "canvas",
      WUBUFX_CAP_READ | WUBUFX_CAP_WRITE,
      dosgui_launch_canvas },

    { "dos_box",      "DOS Box",      "dos_box",
      WUBUFX_CAP_READ | WUBUFX_CAP_WRITE | WUBUFX_CAP_EXEC,
      dosgui_launch_dos_box_default },

    { "edr_dashboard","EDR Activity", "edr_dashboard",
      WUBUFX_CAP_READ | WUBUFX_CAP_EDR,
      dosgui_launch_edr_dashboard },
};
static const int g_wubufx_app_count = sizeof(g_wubufx_apps) / sizeof(g_wubufx_apps[0]);

/* -- Namespace path helpers --------------------------------------- */

static char *fx_ns_path(const char *id) {
    static char path[256];
    snprintf(path, sizeof(path), "/app/%s/", id);
    return path;
}

/* -- App launch via WuBuFX ---------------------------------------- */

/* Launch an app by canonical name. Mounts namespace, opens /win node,
 * returns the engine's DosGuiWindow. */
DosGuiWindow *wubufx_app_launch(const char *name) {
    if (!name || !*name) return NULL;

    /* Find app definition */
    const WubufxAppDef *def = NULL;
    for (int i = 0; i < g_wubufx_app_count; i++) {
        if (strcmp(g_wubufx_apps[i].name, name) == 0) {
            def = &g_wubufx_apps[i];
            break;
        }
    }
    if (!def) return NULL;

    /* Mount the app namespace (content-addressed, capability-scoped) */
    WubuFxApp *app = NULL;
    WubuFxErr err = wubufx_mount(def->namespace_id, def->caps, &app);
    if (err != WUBUFX_OK || !app) {
        edr_log_event(EDR_EV_IMAGE_LOAD, 0, 0, 0, 0, 0,
                      "wubufx_app_launch: mount failed");
        return NULL;
    }

    /* Open the app's window node */
    WubuFxNode *win_node = wubufx_open(app, "/win");
    if (!win_node) {
        /* Lazily create /win if it doesn't exist */
        win_node = wubufx_open(app, "/win");
    }

    /* Set initial state: window is "opening" */
    if (win_node) {
        wubufx_state_set(win_node, "opening");
    }

    /* Launch the REAL engine through the existing dosgui_apps registry.
     * The engine creates the DosGuiWindow and wires its own callbacks.
     * We just return it — the window is already managed by dosgui_wm. */
    DosGuiWindow *win = def->engine_launch();
    if (win) {
        /* Stash the WuBuFX app handle in user_data for later reference */
        win->user_data = app;

        /* Mark the node state */
        if (win_node) {
            wubufx_state_set(win_node, "open");
        }

        /* Disclose the launch to EDR */
        edr_log_event(EDR_EV_PROCESS_CREATE, 0, 0, 0, 0, 0,
                      def->namespace_id);
    } else {
        /* Engine failed — unmount and disclose */
        edr_log_event(EDR_EV_PROCESS_CREATE, 0, 0, 0, 0, 0,
                      "wubufx_app_launch: engine failed");
        wubufx_unmount(app);
        return NULL;
    }

    return win;
}

/* -- Shell population helpers ------------------------------------- */

int wubufx_app_count(void) {
    return g_wubufx_app_count;
}

const char *wubufx_app_name(int i) {
    if (i >= 0 && i < g_wubufx_app_count)
        return g_wubufx_apps[i].name;
    return NULL;
}

/* Get app definition by index (for shell UI) */
const char *wubufx_app_title(int i) {
    if (i >= 0 && i < g_wubufx_app_count)
        return g_wubufx_apps[i].title;
    return NULL;
}

/* Get app namespace ID by index */
const char *wubufx_app_namespace_id(int i) {
    if (i >= 0 && i < g_wubufx_app_count)
        return g_wubufx_apps[i].namespace_id;
    return NULL;
}

/* Get app capabilities by index */
WubuFxCap wubufx_app_caps(int i) {
    if (i >= 0 && i < g_wubufx_app_count)
        return g_wubufx_apps[i].caps;
    return 0;
}