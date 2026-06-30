/*
 * dosgui_apps.h  --  App Registry for Desktop/StartMenu
 * Clean interface: just launch functions for each app
 * No god headers, minimal includes, C11
 */

#ifndef WUBU_DOSGUI_APPS_H
#define WUBU_DOSGUI_APPS_H

#include <stdint.h>

typedef struct DosGuiWindow DosGuiWindow;

/* Desktop icon types (from dosgui_desktop.h) */
#define DESK_ICON_MY_COMPUTER   0
#define DESK_ICON_TEMPLE_REPL   1
#define DESK_ICON_NOTEPAD       2
#define DESK_ICON_PAINT         3
#define DESK_ICON_CALCULATOR    4
#define DESK_ICON_TERMINAL      5
#define DESK_ICON_EXPLORER      6
#define DESK_ICON_SETTINGS      7
#define DESK_ICON_COUNT         8

/* Launch functions for each app */
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

/* External container apps */
void dosgui_launch_freedoom(void);

/* Generic launch by icon type or name */
DosGuiWindow* dosgui_app_launch(int icon_type);
DosGuiWindow* dosgui_app_launch_by_name(const char *name);

#endif