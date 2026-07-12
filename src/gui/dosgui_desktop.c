/*
 * dosgui_desktop.c  --  WuBuOS DosGui Desktop Implementation
 *
 * Cell 401: THEMED Win98/XP desktop with launchable app icons.
 * ZealOS kernel runs in-process, Fable sauce renders the desktop,
 * apps launch as in-process windows or host processes.
 * Full theme engine integration (Win98, XP Luna, XP Media, WuBu).
 */

#include "dosgui_desktop.h"
#include "dosgui_wm.h"
#include "dosgui_daemon_panel.h"
#include "dosgui_startmenu.h"
#include "dosgui_service_mgr.h"
#include "../apps/dosgui_apps.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "dosgui_wm.h"
#include "../hosted/hosted.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

/* -- Theme Helpers ------------------------------------------------ */

static const WubuThemeColors *tc(void) { return wubu_theme_colors(); }
static const WubuTheme *theme(void) { return wubu_theme_get(); }

/* -- App Registry (single source of truth: dosgui_apps.c) -------- */

/* Tracking launched app windows for re-focus */
typedef struct {
    int          app_idx;
    int          win_id;       /* dosgui_wm window id */
    pid_t        pid;          /* host process pid, 0 = in-process */
    int          active;
} LaunchedApp;

static LaunchedApp g_launched[16];
static int g_launched_count = 0;

/* -- Setup Icons ---------------------------------------------------- */

int dosgui_desktop_init(void) {
    /* Register desktop icons from the single app registry. */
    for (int i = 0; i < g_app_def_count; i++) {
        const DosGuiAppDef *d = &g_app_defs[i];
        /* Skip duplicate entries that map to the same launch (e.g. Paint==Canvas). */
        bool dup = false;
        for (int j = 0; j < i; j++)
            if (g_app_defs[j].launch == d->launch && g_app_defs[j].icon_type == d->icon_type) { dup = true; break; }
        if (dup) continue;
        dosgui_icon_add_ex(d->name, DESK_ICON_APP, NULL,
                           i % 6, i / 6, d->icon_color, NULL);
    }

    /* Initialize daemon panel (system tray icons, socket connections) */
    dosgui_daemon_panel_init();

    /* E3 integration: wubu_archd (16/16) is the Desktop's service/autostart
     * manager. Initialize it and boot every registered autostart service. */
    if (dosgui_service_mgr_init() == 0) {
        dosgui_service_mgr_boot();
    }

    return 0;
}

void dosgui_desktop_shutdown(void) {
    /* Kill any host processes */
    for (int i = 0; i < g_launched_count; i++) {
        if (g_launched[i].pid > 0) {
            kill(g_launched[i].pid, SIGTERM);
            waitpid(g_launched[i].pid, NULL, 0);
        }
    }
    g_launched_count = 0;

    /* Shutdown daemon panel */
    dosgui_daemon_panel_shutdown();

    /* Tear down the archd service manager (stops booted services). */
    dosgui_service_mgr_shutdown();
}

/* -- Launch ------------------------------------------------------ */

static void launch_host_app(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {"/bin/sh", "-c", (char *)cmd, NULL};
        execv("/bin/sh", argv);
        _exit(1);
    } else if (pid > 0) {
        if (g_launched_count < 16) {
            g_launched[g_launched_count].app_idx = -1;
            g_launched[g_launched_count].win_id = -1;
            g_launched[g_launched_count].pid = pid;
            g_launched[g_launched_count].active = 1;
            g_launched_count++;
        }
    }
}

void dosgui_launch_app(const char *name) {
    fprintf(stderr, "DEBUG: dosgui_launch_app called with name='%s'\n", name);
    /* Daemon panel windows */
    if (strcmp(name, "Container Manager") == 0) {
        fprintf(stderr, "DEBUG: Launching Container Manager\n");
        archd_tray_click();
        return;
    }
    if (strcmp(name, "HolyC Sessions") == 0) {
        fprintf(stderr, "DEBUG: Launching HolyC Sessions\n");
        holyd_tray_click();
        return;
    }
    fprintf(stderr, "DEBUG: Calling dosgui_app_launch_by_name\n");
    dosgui_app_launch_by_name(name);
    fprintf(stderr, "DEBUG: dosgui_app_launch_by_name returned\n");
}

void dosgui_desktop_launch(int icon_id) {
    if (icon_id < 0 || icon_id >= g_app_def_count) return;

    /* Re-focus an already-launched in-process window. */
    for (int i = 0; i < g_launched_count; i++) {
        if (g_launched[i].active && g_launched[i].app_idx == icon_id &&
            g_launched[i].win_id > 0) {
            DosGuiWindow *w = dosgui_wm_find_by_id(g_launched[i].win_id);
            if (w) dosgui_wm_set_focus(w);
            return;
        }
    }

    const DosGuiAppDef *d = &g_app_defs[icon_id];
    DosGuiWindow *win = d->launch();
    if (win && g_launched_count < 16) {
        g_launched[g_launched_count].app_idx = icon_id;
        g_launched[g_launched_count].win_id = win->id;
        g_launched[g_launched_count].pid = 0;
        g_launched[g_launched_count].active = 1;
        g_launched_count++;
    }
}

/* -- Render ------------------------------------------------------ */

void dosgui_desktop_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    dosgui_wm_render_desktop(fb, fb_w, fb_h);
}

/* -- Tick ------------------------------------------------------- */

void dosgui_desktop_tick(void) {
    dosgui_tick();

    /* Process daemon events */
    dosgui_daemon_panel_tick();

    /* Reap zombie host processes */
    for (int i = 0; i < g_launched_count; i++) {
        if (g_launched[i].pid > 0) {
            int status;
            pid_t r = waitpid(g_launched[i].pid, &status, WNOHANG);
            if (r == g_launched[i].pid) {
                g_launched[i].active = 0;
                g_launched[i].pid = 0;
            }
        }
    }
}

/* -- Shutdown ------------------------------------------------------ */

void dosgui_shutdown(void) {
    extern void dosgui_platform_shutdown(void);
    dosgui_platform_shutdown();
}