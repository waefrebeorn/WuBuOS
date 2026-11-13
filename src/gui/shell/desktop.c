/*
 * desktop.c  --  WuBuOS DosGui Desktop Implementation
 *
 * Cell 401: THEMED Win98/XP desktop with launchable app icons.
 * ZealOS kernel runs in-process, Fable sauce renders the desktop,
 * apps launch as in-process windows or host processes.
 * Full theme engine integration (Win98, XP Luna, XP Media, WuBu).
 */

#include "../../egojar.h"
#include "../../egojar_net.h"
#include <sys/socket.h>
#include "./desktop.h"
#include "../wm.h"
#include "../wm_internal.h"
#include "wubu_mime.h"
#include "wubu_mime_internal.h"
#include "wubu_runline.h"
#include "../daemon_panel.h"
#include "../wubu_trash.h"
#include "../wubu_settings.h"
#include "./startmenu.h"
#include "../service_mgr.h"
#include "./wubu_notify.h"
#include "../../runtime/wubu_session_mgr.h"
#include "../../runtime/wubu_deskenv.h"
#include "../../runtime/wubu_ns_kernels.h"
#include "../../apps/apps.h"
#include "../era_apps.h"
#include "../../kernel/vbe.h"
#include "../../gui/wubu_theme.h"
#include "../wm.h"
#include "../../hosted/hosted.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* -- Theme Helpers --------------------------------------------------
 * tc() and theme() come from wm_internal.h; this file used to define its own
 * static copies, which now collide with those declarations.
 */

/* -- App Registry (single source of truth: apps.c) -------- */

/* Tracking launched app windows for re-focus */
typedef struct {
    int          app_idx;
    int          win_id;       /* wm window id */
    pid_t        pid;          /* host process pid, 0 = in-process */
    int          active;
} LaunchedApp;

static LaunchedApp g_launched[16];
static int g_launched_count = 0;

/* -- Setup Icons ---------------------------------------------------- */

int desktop_init(void) {
    /* Register desktop icons from the single app registry. */
    for (int i = 0; i < g_app_def_count; i++) {
        const DosGuiAppDef *d = &g_app_defs[i];
        /* Skip duplicate entries that map to the same launch (e.g. Paint==Canvas). */
        bool dup = false;
        for (int j = 0; j < i; j++)
            if (g_app_defs[j].launch == d->launch && g_app_defs[j].icon_type == d->icon_type) { dup = true; break; }
        if (dup) continue;
        icon_add_ex(d->name, DESK_ICON_APP, NULL,
                           i % 6, i / 6, d->icon_color, NULL);
    }

    /* A program is a file.  Anything .wet in the applet directory becomes a
     * desktop icon and a startmenu entry -- written by hand, edited in the
     * IDE, or created from a VWS template -- with no C change and no rebuild. */
    char apdir[512];
    app_wet_dir(apdir, sizeof apdir);
    if (app_scan_wet_dir(apdir) > 0) {
        for (int i = 0; i < app_wet_count(); i++) {
            const DosGuiAppDef *d = app_wet_at(i);
            if (!d) continue;
            icon_add_ex(d->name, DESK_ICON_APP, NULL,
                        (g_app_def_count + i) % 6, (g_app_def_count + i) / 6,
                        d->icon_color, NULL);
        }
    }

    /* ReactOS explorer/desktop.cpp lesson: the desktop is a LIVE namespace.
     * Enumerate ~/Desktop for .desktop shortcuts so they appear on boot,
     * not only after a manual Refresh. */
    wm_refresh_desktop();

    /* Re-apply any persisted icon layout (drag-end positions saved via
     * wm_save_icon_layout) so positions survive a real restart. */
    wm_restore_icon_layout();

    /* Restore the persisted View -> Auto-arrange toggle (ReactOS NTUSER).
     * Using the public setter also re-flows the icons live. */
    WubuSettings *s = wubu_settings_get();
    if (s) wm_set_auto_arrange(s->theme.auto_arrange);

    /* Control Panel: hide desktop icons on demand. */
    if (s && !s->theme.show_desktop_icons) wm_set_icons_visible(false);

    /* Initialize the trash/Recycle Bin so context-menu Delete moves files
     * there (ReactOS shell32 lesson) instead of just hiding the icon. */
    wubu_trash_init();

    /* E3 integration: wubu_archd (16/16) is the Desktop's service/autostart
     * manager. Initialize it and boot every registered autostart service.
     * N4: boot failures surface as desktop toasts, so the notification
     * daemon must be live first. */
    wubu_notify_init();
    if (service_mgr_init() == 0) {
        service_mgr_boot();
    }

    /* P3 /n/background: bring up the rich session-manager runtime and mount
     * its control surface. The desktop owns the /n root (a real directory
     * under ~/.wubu); boot the session manager (bus + logind + auth wired as
     * the power gate), publish the /n/background tree, and register WHO owns
     * the desktop + the tray slots so the owning process is visible through
     * /n regardless of who paints it. Guarded so a missing/raw home dir never
     * fails the desktop boot. */
    {
        const char *home = getenv("HOME");
        static char nsroot[512];
        if (home) {
            snprintf(nsroot, sizeof(nsroot), "%s/.wubu", home);
            mkdir(nsroot, 0755);
            snprintf(nsroot, sizeof(nsroot), "%s/.wubu/n", home);
            mkdir(nsroot, 0755);
        } else {
            snprintf(nsroot, sizeof(nsroot), "/tmp/wubu_n");
            mkdir(nsroot, 0755);
        }
        extern int wubu_ns_bridge_create(const char *);
        if (wubu_ns_bridge_create(nsroot) == 0) {
            /* the autostart dir the session manager scans (XDG system dir) */
            char *const autodirs[2] = {
                (char*)"/etc/xdg/autostart", (char*)"$HOME"};
            /* the kernel interchange, published for the operator + the AGI */
            wubu_ns_publish_kernels();
            (void)autodirs[1];
            wubu_session_mgr_t *mgr = wubu_session_mgr_boot(autodirs, 1, NULL, NULL);
            if (mgr) {
                wubu_session_mgr_register_desktop(mgr, "dosgui-desktop");
                wubu_session_mgr_register_tray(mgr, "battery", "dosgui-tray");
                wubu_session_mgr_register_tray(mgr, "network", "dosgui-tray");
                wubu_session_mgr_register_tray(mgr, "heat", "dosgui-tray");
                wubu_session_daemon_publish(wubu_session_mgr_daemon(mgr));
                wubu_ns_publish_deskenv(wubu_session_mgr_deskenv(mgr));
                /* keep alive for the session: stash in a file-scope holder */
                static wubu_session_mgr_t *gbg = NULL;
                gbg = mgr;
                (void)gbg;
            }
        }
    }

    return 0;
}

void desktop_shutdown(void) {
    /* Kill any host processes */
    for (int i = 0; i < g_launched_count; i++) {
        if (g_launched[i].pid > 0) {
            kill(g_launched[i].pid, SIGTERM);
            waitpid(g_launched[i].pid, NULL, 0);
        }
    }
    g_launched_count = 0;

    /* Shutdown daemon panel */
    daemon_panel_shutdown();

    /* Tear down the archd service manager (stops booted services). */
    service_mgr_shutdown();

    /* Tear down the notification daemon. */
    wubu_notify_shutdown();
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

void launch_app(const char *name) {
    /* Daemon panel windows */
    if (strcmp(name, "Container Manager") == 0) {
        archd_tray_click();
        return;
    }
    if (strcmp(name, "HolyC Sessions") == 0) {
        holyd_tray_click();
        return;
    }
    /* Builtin in-process apps. A registry entry IS the launch path: several
     * launchers spawn their window through wm_spawn_* and return NULL, so the
     * return value must not be read as "not found". */
    if (app_find_by_name(name) != NULL) {
        app_launch_by_name(name);
        return;
    }
    /* Fallback: era apps (one-per-era registry). These launch through their
     * VSL syscall personality's REAL exec backend (DOS->8086 shim, Win->Wine,
     * Linux->VSL ELF, HolyC->JIT). Gaps (CP/M, Classic Mac) report clearly. */
    if (era_apps_launch_by_name(name) == 0) return;
    fprintf(stderr, "wubu: no launch path for '%s'\n", name);
}

void desktop_launch(int icon_id) {
    if (icon_id < 0 || icon_id >= g_app_def_count) return;

    /* Re-focus an already-launched in-process window. */
    for (int i = 0; i < g_launched_count; i++) {
        if (g_launched[i].active && g_launched[i].app_idx == icon_id &&
            g_launched[i].win_id > 0) {
            DosGuiWindow *w = wm_find_by_id(g_launched[i].win_id);
            if (w) wm_set_focus(w);
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

void desktop_render(uint32_t *fb, int fb_w, int fb_h) {
    wm_render(fb, fb_w, fb_h);
}

/* -- Tick ------------------------------------------------------- */

void desktop_tick(void) {
    tick();

    /* Process daemon events */
    daemon_panel_tick();

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

void wubu_shutdown() {
    extern void platform_shutdown(void);
    platform_shutdown();
}

/* Strip .desktop field codes (%f %F %u %U %i %c %k ...) from an Exec value.
 * Shortcuts we write never use them, but a user-supplied .desktop may, and a
 * stray "%f" handed to a shell is just noise on the command line. */
static void strip_field_codes(const char *in, char *out, size_t cap)
{
    size_t o = 0;
    for (const char *p = in; *p && o + 1 < cap; p++) {
        if (*p == '%' && p[1]) { p++; continue; }
        out[o++] = *p;
    }
    out[o] = '\0';
}

void desktop_launch_icon(int icon_id)
{
    if (icon_id < 0 || icon_id >= g_dwm.icon_count) return;
    const DosGuiIcon *ic = &g_dwm.icons[icon_id];
    if (!ic->alive) return;

    switch (ic->type) {
    case DESK_ICON_SHORTCUT: {
        if (!ic->target[0]) break;
        DesktopEntry e;
        if (!parse_desktop_file(ic->target, &e) || !e.exec[0]) {
            fprintf(stderr, "wubu: shortcut '%s' has no usable Exec in %s\n",
                    ic->name, ic->target);
            break;
        }
        char cmd[MAX_HANDLER_CMD_LEN];
        strip_field_codes(e.exec, cmd, sizeof(cmd));

        /* A bare application name still goes through the registry first, so
         * "notepad" launches the real Notepad rather than a shell search. */
        char first[MAX_HANDLER_CMD_LEN];
        snprintf(first, sizeof(first), "%s", cmd);
        char *sp = strchr(first, ' ');
        if (sp) *sp = '\0';
        if (app_find_by_name(first)) { launch_app(first); break; }

        wubu_runline_execute(cmd, "shortcut-output.txt");
        break;
    }
    case DESK_ICON_FOLDER:
    case DESK_ICON_FILE:
        if (ic->target[0]) wubu_mime_launch(ic->target, NULL);
        break;
    default:
        launch_app(ic->name);
        break;
    }
}
