/*
 * control.h  --  Control Panel (8 XP-style categories over a detail pane)
 *
 * The old comment here promised 9 tabs (Display, Theme, Desktop, Taskbar,
 * Input, Startup, Containers, Network, About); the implementation only ever
 * rendered one static page. What exists now is the category grid the renderer
 * actually draws, each with a detail pane whose buttons act on the running
 * system.
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_CONTROL_IMPL_H
#define WUBU_CONTROL_IMPL_H

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

/* Window callback: category + detail-button hit-test (press, not release). */
void control_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);

/* Read-only inspection accessors (opaque-struct safe; for tests/debug). */
int control_get_tab(const ControlState *ctrl);
int control_category_count(const ControlState *ctrl);
bool control_category_rect(const ControlState *ctrl, int i, int *x, int *y, int *w, int *h);
int control_action_count(const ControlState *ctrl);
bool control_action_rect(const ControlState *ctrl, int i, int *x, int *y, int *w, int *h);
const char *control_action_label(const ControlState *ctrl, int i);

#endif