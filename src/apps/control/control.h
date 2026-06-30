/*
 * control.h  --  Control Panel (9 tabs: Display, Theme, Desktop, Taskbar, Input, Startup, Containers, Network, About)
 * Opaque struct, C11, minimal includes, self-contained
 */

#ifndef WUBU_CONTROL_H
#define WUBU_CONTROL_H

#include <stdint.h>

typedef struct DosGuiWindow DosGuiWindow;

typedef struct ControlState ControlState;

ControlState* control_create(void);
void control_destroy(ControlState *ctrl);

void control_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, ControlState *ctrl);
DosGuiWindow* control_launch(void);

void control_set_tab(ControlState *ctrl, int tab);

#endif