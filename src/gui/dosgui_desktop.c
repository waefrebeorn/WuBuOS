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

/* -- App Registry ------------------------------------------- */

typedef struct {
    const char *name;
    const char *title;
    int         icon_type;
    /* External launch command (for real host apps like doom) */
    const char *exec_cmd;
} AppEntry;

static AppEntry g_apps[] = {
    { "My Computer",  "My Computer",   DESK_ICON_MY_COMPUTER,   NULL },
    { "Temple REPL",  "HolyC REPL",    DESK_ICON_TEMPLE_REPL,   NULL },
    { "Notepad",      "Notepad",       DESK_ICON_NOTEPAD,       NULL },
    { "Paint",        "Paint",         DESK_ICON_PAINT,         NULL },
    { "Calculator",   "Calculator",    DESK_ICON_CALCULATOR,    NULL },
    { "Terminal",     "Terminal",      DESK_ICON_TERMINAL,      NULL },
    { "File Manager", "File Manager",  DESK_ICON_EXPLORER,      NULL },
    { "Settings",     "Control Panel", DESK_ICON_SETTINGS,      NULL },
    { "Editor",       "Editor",        DESK_ICON_COUNT,         NULL },
    { "WuBu Canvas",  "WuBu Canvas",   DESK_ICON_COUNT + 1,     NULL },
    { "FreeDoom",     "FreeDoom",      DESK_ICON_COUNT + 2,     NULL },
    { "HolyC Term",   "HolyC Terminal", DESK_ICON_COUNT + 3,    NULL },
    { "Container Manager", "Containers", DESK_ICON_COUNT + 4,  NULL },
    { "HolyC Sessions", "HolyC Sessions", DESK_ICON_COUNT + 5,  NULL },
};
#define NUM_APPS (sizeof(g_apps) / sizeof(g_apps[0]))

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
    /* Register desktop icons with dosgui_wm */
    dosgui_icon_add("My Computer",  0, 0, dosgui_launch_my_computer);
    dosgui_icon_add("Temple REPL",  0, 1, dosgui_launch_temple_repl);
    dosgui_icon_add("Notepad",      0, 2, dosgui_launch_notepad);
    dosgui_icon_add("Paint",        0, 3, dosgui_launch_paint);
    dosgui_icon_add("Calculator",   0, 4, dosgui_launch_calculator);
    dosgui_icon_add("Terminal",     0, 5, dosgui_launch_terminal);
    dosgui_icon_add("File Manager", 0, 6, dosgui_launch_file_manager);
    dosgui_icon_add("Settings",     0, 7, dosgui_launch_settings);
    dosgui_icon_add("Editor",       1, 0, dosgui_launch_editor);
    dosgui_icon_add("WuBu Canvas",  1, 1, dosgui_launch_canvas);
    dosgui_icon_add("FreeDoom",     1, 2, dosgui_launch_freedoom);
    dosgui_icon_add("HolyC Term",   1, 3, dosgui_launch_holyc_term);

    /* Initialize daemon panel (system tray icons, socket connections) */
    dosgui_daemon_panel_init();

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
    if (icon_id < 0 || (size_t)icon_id >= NUM_APPS) return;

    AppEntry *app = &g_apps[icon_id];

    /* Check if already launched — re-focus */
    for (int i = 0; i < g_launched_count; i++) {
        if (g_launched[i].active && g_launched[i].app_idx == icon_id &&
            g_launched[i].win_id > 0) {
            DosGuiWindow *w = dosgui_wm_find_by_id(g_launched[i].win_id);
            if (w) dosgui_wm_set_focus(w);
            return;
        }
    }

    if (app->exec_cmd) {
        /* External host app */
        launch_host_app(app->exec_cmd);
        /* Also create a placeholder window */
        DosGuiWindow *win = dosgui_wm_create(
            100 + (g_launched_count * 30) % 200,
            80 + (g_launched_count * 25) % 150,
            640, 480, app->title);
        if (win && g_launched_count < 16) {
            g_launched[g_launched_count].app_idx = icon_id;
            g_launched[g_launched_count].win_id = win->id;
            g_launched[g_launched_count].pid = 0;
            g_launched[g_launched_count].active = 1;
            g_launched_count++;
        }
    } else {
        /* In-process app window - uses dosgui_apps */
        dosgui_launch_app(app->name);
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