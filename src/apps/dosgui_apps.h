/*
 * dosgui_apps.h  --  WuBuOS DosGui App Wrappers Header
 *
 * Cell 401+402: Desktop + StartMenu launch real app content.
 * Provides dosgui_wm-compatible draw callbacks for in-process apps.
 */

#ifndef WUBU_DOSGUI_APPS_H
#define WUBU_DOSGUI_APPS_H

#include <stdint.h>

/* Forward declaration */
typedef struct DosGuiWindow DosGuiWindow;

/* Launch an app by desktop icon type (DESK_ICON_* from dosgui_desktop.h) */
DosGuiWindow* dosgui_app_launch(int icon_type);

/* Launch an app by name (for start menu) */
DosGuiWindow* dosgui_app_launch_by_name(const char *name);

/* Desktop icon click handlers */
void dosgui_launch_my_computer(void);
void dosgui_launch_temple_repl(void);
void dosgui_launch_notepad(void);
void dosgui_launch_paint(void);
void dosgui_launch_calculator(void);
void dosgui_launch_terminal(void);
void dosgui_launch_file_manager(void);
void dosgui_launch_settings(void);
void dosgui_launch_editor(void);
void dosgui_launch_canvas(void);

#endif /* WUBU_DOSGUI_APPS_H */