/*
 * apps.h  --  Single source-of-truth App Registry for WuBuOS
 *
 * One data-driven table (g_app_defs[]) replaces the previous TWO hand-synced
 * hardcoded tables (apps.c g_apps[] AND desktop.c g_apps[]).
 * Adding an app is now a single table entry: name, title, icon type, a launch
 * function, and (optionally) on_draw / on_mouse / on_key bindings so the app
 * actually renders and receives input inside its window.
 *
 * No god headers: this header only forward-declares DosGuiWindow and the
 * standard integer types it needs.
 */

#ifndef WUBU_DOSGUI_APPS_H
#define WUBU_DOSGUI_APPS_H

#include <stddef.h>

#include <stdint.h>

typedef struct DosGuiWindow DosGuiWindow;

/* Desktop icon types (stable IDs used by the registry + desktop layout). */
#define DESK_ICON_MY_COMPUTER   0
#define DESK_ICON_TEMPLE_REPL   1
#define DESK_ICON_NOTEPAD       2
#define DESK_ICON_PAINT         3
#define DESK_ICON_CALCULATOR    4
#define DESK_ICON_TERMINAL      5
#define DESK_ICON_EXPLORTER     6
#define DESK_ICON_SETTINGS      7
#define DESK_ICON_COUNT         8   /* first dynamically-allocated icon id */

/* App definition: one entry per desktop app. The launch fn creates the window
 * AND wires its on_draw/on_mouse/on_key callbacks (see apps.c). */
typedef struct {
    const char *name;       /* registry/launch key (e.g. "WuBu Canvas") */
    const char *title;      /* window title */
    int         icon_type;  /* DESK_ICON_* id (or DESK_ICON_COUNT + n) */
    uint32_t    icon_color; /* fallback icon box color */
    /* Launch: create the window + bind callbacks. Returns the window or NULL. */
    DosGuiWindow *(*launch)(void);
    /* Set for a discovered .wet applet: the script to run.  When non-NULL the
     * generic launch runs launch_wet_applet(path) instead of `launch`. */
    const char *path;
} DosGuiAppDef;

/* The single registry. Index = desktop icon_type for the built-ins. */
extern const DosGuiAppDef g_app_defs[];
extern const int          g_app_def_count;

/* -- Discovered .wet applets -------------------------------------------
 *
 * A program is a FILE.  Anything ending in .wet found in the applet
 * directory becomes a desktop program -- created from a VWS template, edited
 * in the IDE, or written by hand -- with no C change and no rebuild.  This is
 * the point of the template library: making a program is writing a file. */
int                app_scan_wet_dir(const char *dir);
int                app_wet_count(void);
const DosGuiAppDef *app_wet_at(int i);

/* Resolve the applet directory: $WUBU_APPLET_DIR, else $HOME/.wubu/applets. */
void app_wet_dir(char *out, size_t cap);

/* Lookup helpers (single point of truth). */
const DosGuiAppDef *app_find(int icon_type);
const DosGuiAppDef *app_find_wet(int icon_type);        /* owned by apps_wet.c */
const DosGuiAppDef *app_find_wet_by_name(const char *name);
DosGuiWindow       *app_launch_wet_path(const char *path);
const DosGuiAppDef *app_find_by_name(const char *name);

/* Generic launch by icon type or name (used by desktop + startmenu). */
DosGuiWindow* app_launch(int icon_type);
DosGuiWindow* app_launch_by_name(const char *name);

/* Individual launch functions (thin wrappers over the registry table). */
DosGuiWindow* launch_my_computer(void);
DosGuiWindow* launch_temple_repl(void);
DosGuiWindow* launch_notepad(void);
DosGuiWindow* launch_paint(void);
DosGuiWindow* launch_calculator(void);
DosGuiWindow* launch_terminal(void);
DosGuiWindow* launch_file_manager(void);
DosGuiWindow* launch_settings(void);
DosGuiWindow* launch_editor(void);
DosGuiWindow* launch_wet(void);
DosGuiWindow* launch_vws_designer(void);
DosGuiWindow* launch_getting_started(void);
DosGuiWindow* launch_tutorials(void);
DosGuiWindow* launch_canvas(void);
DosGuiWindow* launch_holyc_term(void);
/* Launch a real 16-bit .COM/.EXE (path may be NULL → built-in demo .COM). */
DosGuiWindow* launch_dos_box(const char *path);

/* EDR Activity Dashboard (disclosure surface). Defined in apps/edr_dash.c. */
DosGuiWindow* edr_dash_launch(void);
DosGuiWindow* launch_edr_dashboard(void);

/* Bonzi Buddy desktop AGI agent + Comfy node-graph editor. */
DosGuiWindow* bonzi_launch(void);
DosGuiWindow* comfy_launch(void);
DosGuiWindow* tandem_launch(void);

/* DOS Box launchers (defined in apps/apps.c) */
DosGuiWindow* launch_dos_box(const char *path);
DosGuiWindow* launch_dos_box_default(void);

/* External container apps (no in-process window). */
void launch_explorer(void);

/* DOS Box launchers (defined in apps/apps.c) */
DosGuiWindow* launch_dos_box(const char *path);
DosGuiWindow* launch_dos_box_default(void);

#endif
