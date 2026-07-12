/*
 * dosgui_apps.h  --  Single source-of-truth App Registry for WuBuOS
 *
 * One data-driven table (g_app_defs[]) replaces the previous TWO hand-synced
 * hardcoded tables (dosgui_apps.c g_apps[] AND dosgui_desktop.c g_apps[]).
 * Adding an app is now a single table entry: name, title, icon type, a launch
 * function, and (optionally) on_draw / on_mouse / on_key bindings so the app
 * actually renders and receives input inside its window.
 *
 * No god headers: this header only forward-declares DosGuiWindow and the
 * standard integer types it needs.
 */

#ifndef WUBU_DOSGUI_APPS_H
#define WUBU_DOSGUI_APPS_H

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
 * AND wires its on_draw/on_mouse/on_key callbacks (see dosgui_apps.c). */
typedef struct {
    const char *name;       /* registry/launch key (e.g. "WuBu Canvas") */
    const char *title;      /* window title */
    int         icon_type;  /* DESK_ICON_* id (or DESK_ICON_COUNT + n) */
    uint32_t    icon_color; /* fallback icon box color */
    /* Launch: create the window + bind callbacks. Returns the window or NULL. */
    DosGuiWindow *(*launch)(void);
} DosGuiAppDef;

/* The single registry. Index = desktop icon_type for the built-ins. */
extern const DosGuiAppDef g_app_defs[];
extern const int          g_app_def_count;

/* Lookup helpers (single point of truth). */
const DosGuiAppDef *dosgui_app_find(int icon_type);
const DosGuiAppDef *dosgui_app_find_by_name(const char *name);

/* Generic launch by icon type or name (used by desktop + startmenu). */
DosGuiWindow* dosgui_app_launch(int icon_type);
DosGuiWindow* dosgui_app_launch_by_name(const char *name);

/* Individual launch functions (thin wrappers over the registry table). */
DosGuiWindow* dosgui_launch_my_computer(void);
DosGuiWindow* dosgui_launch_temple_repl(void);
DosGuiWindow* dosgui_launch_notepad(void);
DosGuiWindow* dosgui_launch_paint(void);
DosGuiWindow* dosgui_launch_calculator(void);
DosGuiWindow* dosgui_launch_terminal(void);
DosGuiWindow* dosgui_launch_file_manager(void);
DosGuiWindow* dosgui_launch_settings(void);
DosGuiWindow* dosgui_launch_editor(void);
DosGuiWindow* dosgui_launch_canvas(void);
DosGuiWindow* dosgui_launch_holyc_term(void);

/* External container apps (no in-process window). */
void dosgui_launch_freedoom(void);

#endif
