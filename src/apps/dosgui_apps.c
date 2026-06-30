/*
 * dosgui_apps.c  --  App Registry Implementation
 * Wires up all app launch functions
 * C11, minimal includes, self-contained
 */

#include "dosgui_apps.h"
#include "calc/calc.h"
#include "notepad/notepad.h"
#include "paint/paint.h"
#include "taskmgr/taskmgr.h"
#include "regedit/regedit.h"
#include "fm/fm.h"
#include "repl/repl.h"
#include "control/control.h"
#include "editor/editor.h"
#include "canvas/canvas.h"
#include "../gui/dosgui_wm.h"
#include "../runtime/wubu_host_exec.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Simple app registry */
typedef struct {
    int icon_type;
    const char *title;
    DosGuiWindow* (*launch_fn)(void);
} AppEntry;

static AppEntry g_apps[] = {
    {DESK_ICON_MY_COMPUTER,   "My Computer",   dosgui_launch_file_manager},
    {DESK_ICON_TEMPLE_REPL,  "HolyC REPL",    dosgui_launch_temple_repl},
    {DESK_ICON_NOTEPAD,      "Notepad",       dosgui_launch_notepad},
    {DESK_ICON_PAINT,        "Paint",         dosgui_launch_paint},
    {DESK_ICON_CALCULATOR,   "Calculator",    dosgui_launch_calculator},
    {DESK_ICON_TERMINAL,     "Terminal",      dosgui_launch_terminal},
    {DESK_ICON_EXPLORER,     "File Manager",  dosgui_launch_file_manager},
    {DESK_ICON_SETTINGS,     "Control Panel", dosgui_launch_settings},
    {DESK_ICON_COUNT,        "Editor",        dosgui_launch_editor},
    {DESK_ICON_COUNT+1,      "WuBu Canvas",   dosgui_launch_canvas},
};

static int g_app_count = sizeof(g_apps) / sizeof(g_apps[0]);

DosGuiWindow* dosgui_app_launch(int icon_type) {
    fprintf(stderr, "DEBUG: dosgui_app_launch called with icon_type=%d\n", icon_type);
    for (int i = 0; i < g_app_count; i++) {
        if (g_apps[i].icon_type == icon_type) {
            fprintf(stderr, "DEBUG: Found icon_type %d at index %d, calling launch_fn\n", icon_type, i);
            return g_apps[i].launch_fn();
        }
    }
    fprintf(stderr, "DEBUG: No match for icon_type=%d\n", icon_type);
    return NULL;
}

DosGuiWindow* dosgui_app_launch_by_name(const char *name) {
    fprintf(stderr, "DEBUG: dosgui_app_launch_by_name called with name='%s'\n", name);
    fprintf(stderr, "DEBUG: g_app_count=%d\n", g_app_count);
    for (int i = 0; i < g_app_count; i++) {
        fprintf(stderr, "DEBUG: checking g_apps[%d] title='%s'\n", i, g_apps[i].title);
        if (strcmp(g_apps[i].title, name) == 0) {
            fprintf(stderr, "DEBUG: Found match, calling launch_fn\n");
            return g_apps[i].launch_fn();
        }
    }
    fprintf(stderr, "DEBUG: No match found for name='%s'\n", name);
    return NULL;
}

/* Individual launch functions - these create windows directly, NOT calling dosgui_app_launch again */
DosGuiWindow* dosgui_launch_my_computer(void)     { 
    fprintf(stderr, "DEBUG: dosgui_launch_my_computer called\n");
    DosGuiWindow* win = dosgui_wm_create(100, 80, 800, 600, "My Computer");
    return win;
}
DosGuiWindow* dosgui_launch_temple_repl(void)     { 
    fprintf(stderr, "DEBUG: dosgui_launch_temple_repl called\n");
    dosgui_wm_spawn_holyc_term(80, 60, 640, 480);
    return NULL;
}
DosGuiWindow* dosgui_launch_notepad(void)         { 
    fprintf(stderr, "DEBUG: dosgui_launch_notepad called\n");
    DosGuiWindow* win = dosgui_wm_create(150, 100, 640, 480, "Notepad");
    return win;
}
DosGuiWindow* dosgui_launch_paint(void)           { 
    fprintf(stderr, "DEBUG: dosgui_launch_paint called\n");
    DosGuiWindow* win = dosgui_wm_create(200, 120, 800, 600, "Paint");
    return win;
}
DosGuiWindow* dosgui_launch_calculator(void)      { 
    fprintf(stderr, "DEBUG: dosgui_launch_calculator called\n");
    DosGuiWindow* win = dosgui_wm_create(250, 140, 400, 500, "Calculator");
    return win;
}
DosGuiWindow* dosgui_launch_terminal(void)        { 
    fprintf(stderr, "DEBUG: dosgui_launch_terminal called\n");
    DosGuiWindow* win = dosgui_wm_create(300, 160, 800, 600, "Terminal");
    return win;
}
DosGuiWindow* dosgui_launch_file_manager(void)    { 
    fprintf(stderr, "DEBUG: dosgui_launch_file_manager called\n");
    DosGuiWindow* win = dosgui_wm_create(100, 80, 800, 600, "File Manager");
    return win;
}
DosGuiWindow* dosgui_launch_settings(void)        { 
    fprintf(stderr, "DEBUG: dosgui_launch_settings called\n");
    DosGuiWindow* win = dosgui_wm_create(350, 180, 640, 480, "Control Panel");
    return win;
}
DosGuiWindow* dosgui_launch_editor(void)          { 
    fprintf(stderr, "DEBUG: dosgui_launch_editor called\n");
    DosGuiWindow* win = dosgui_wm_create(400, 200, 800, 600, "Editor");
    return win;
}
DosGuiWindow* dosgui_launch_canvas(void)          { 
    fprintf(stderr, "DEBUG: dosgui_launch_canvas called\n");
    DosGuiWindow* win = dosgui_wm_create(450, 220, 800, 600, "WuBu Canvas");
    return win;
}

DosGuiWindow* dosgui_launch_holyc_term(void) {
    dosgui_wm_spawn_holyc_term(80, 60, 640, 480);
    return NULL;
}

void dosgui_launch_freedoom(void) {
    WubuCt *ct = wubu_ct_bwrap_freedoom("freedoom");
    if (!ct) { fprintf(stderr, "Failed to create FreeDoom container\n"); return; }
    if (wubu_ct_start_bwrap(ct) != 0) { fprintf(stderr, "Failed to start FreeDoom container\n"); wubu_ct_destroy(ct); return; }
    fprintf(stderr, "FreeDoom launched via bubblewrap (PID %d)\n", ct->pid);
}