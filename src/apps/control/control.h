/*
 * control.h  --  Control Panel (9 tabs: Display, Theme, Desktop, Taskbar, Input, Startup, Containers, Network, About)
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_CONTROL_H
#define WUBU_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct DosGuiWindow DosGuiWindow;

typedef struct ControlState ControlState;

ControlState* control_create(void);
void control_destroy(ControlState *ctrl);

void control_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, ControlState *ctrl);
DosGuiWindow* control_launch(void);

void control_set_tab(ControlState *ctrl, int tab);

/* Desktop tab: apply wallpaper path + placement mode (persists + live reload). */
void control_desktop_apply(const char *wallpaper_path, int mode);

/* Desktop tab: switch the active theme (Win98 Classic / XP Luna / etc.),
 * persists theme_id + applies it live via wubu_theme_set. */
void control_set_theme(int theme_id);

/* Desktop tab: toggle View -> Auto-arrange (persists, re-flows live). */
void control_set_auto_arrange(bool on);

/* Desktop tab: show/hide desktop icons. */
void control_set_show_icons(bool show);

/* Read-only inspection accessor (opaque-struct safe; for tests/debug). */
int control_get_tab(const ControlState *ctrl);

#endif