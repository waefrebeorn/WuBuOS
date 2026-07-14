/*
 * hosted_render.c -- WuBuOS hosted-mode frame composition + input routing
 *
 * Self-contained concern split out of hosted.c (the launcher facade):
 *   - hosted_render_desktop(): composite desktop + start menu + tick into
 *     the VBE back buffer (the in-process ZealOS GUI shell).
 *   - hosted_input_dispatch(): drain the kernel input queue and route
 *     key/mouse events to the DosGui WM (Cell 202 input pipeline).
 *
 * Depends only on the public WM/input/VBE APIs + hosted_internal.h.
 * No god headers; nothing here touches Wayland or the launch lifecycle.
 */

#include "hosted_internal.h"

#include "../kernel/vbe.h"
#include "../kernel/input.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_desktop.h"
#include "../gui/dosgui_startmenu.h"

#include <stdio.h>

/* Compose one desktop frame into the VBE back buffer. */
void hosted_render_desktop(hosted_state_t *state) {
    if (!state) return;
    dosgui_desktop_render(NULL, state->width, state->height);
    if (dosgui_startmenu_is_open()) {
        dosgui_startmenu_render(NULL, state->width, state->height);
    }
    dosgui_desktop_tick();
}

/* Drain the kernel input queue and route events to the focused window. */
void hosted_input_dispatch(void) {
    KeyEvent kev;
    while (input_key_poll(&kev)) {
        dosgui_wm_handle_key(kev.keycode, kev.modifiers);
    }
    MouseEvent mev;
    while (input_mouse_poll(&mev)) {
        int kind = mev.buttons ? 1 : 2; /* 1=down, 2=up */
        if (mev.buttons & 2) kind = 0;   /* 0=move while dragging */
        dosgui_wm_handle_mouse(mev.x, mev.y, mev.buttons, kind);
    }
}
