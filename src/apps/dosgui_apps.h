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

/* Launch FreeDoom via bubblewrap container (external Wayland toplevel) */
/* Returns NULL since FreeDoom creates its own Wayland window */
void dosgui_launch_freedoom(void);

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

/* Draw callbacks for demo/external use */
void dosgui_calc_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_notepad_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_paint_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_repl_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_control_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_editor_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_canvas_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_explorer_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);

#endif /* WUBU_DOSGUI_APPS_H */