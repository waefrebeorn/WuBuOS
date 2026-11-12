/*
 * win11_shell.c -- strong definitions of the Win11 shell glue.
 *
 * One concern: route WM key/press/render events to the Win11 layers
 * (snap-layout flyout + widgets board).  The WM only knows the weak hooks
 * declared in win11_shell.h, so a build without this file simply has no
 * Win11 shell.
 *
 * C11, no god headers.
 */

#include "win11_shell.h"
#include "win11_snap.h"
#include "win11_widgets.h"
#include "../kernel/input.h"   /* MOD_WIN */

void win11_shell_render(uint32_t *fb, int fb_w, int fb_h) {
    win11_widgets_render(fb, fb_w, fb_h);
    win11_snap_render(fb, fb_w, fb_h);
}

bool win11_shell_key(uint32_t key, uint32_t mods) {
    /* Win+Z: snap layout flyout (Windows 11 parity). */
    if ((mods & MOD_WIN) && (key == 0x2C || key == 'z' || key == 'Z')) {
        win11_snap_toggle();
        return true;
    }
    /* Win+W: widgets board (Windows 11 parity). */
    if ((mods & MOD_WIN) && (key == 0x11 || key == 'w' || key == 'W')) {
        win11_widgets_toggle();
        return true;
    }
    return false;
}

bool win11_shell_click(int x, int y) {
    /* While either layer is open it owns the press; a press outside closes
     * it and the WM still gets the event (the click falls through). */
    if (win11_snap_is_open()) {
        win11_snap_click(x, y);
        return true;
    }
    if (win11_widgets_is_open()) {
        win11_widgets_click(x, y);
        return true;
    }
    return false;
}
