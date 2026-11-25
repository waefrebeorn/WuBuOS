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
#include "../gui/wm.h"
#include "../gui/shell/shell_session.h"
#include "../gui/shell/startmenu.h"
#include "../gui/buddy_sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Where an idle frame's time goes. The desktop repaints in full every frame,
 * so the only way to know whether to optimise the wallpaper, the panel or the
 * mascot is to time the phases. Off unless WUBU_RENDER_PROBE=1: an idle frame
 * must not pay for the probe. */
static int render_probe = -1;
static long long render_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/* Compose one desktop frame into the VBE back buffer. */
void hosted_render_desktop(hosted_state_t *state) {
    if (!state) return;
    static long long acc[3]; static int nacc;
    if (render_probe < 0) {
        const char *e = getenv("WUBU_RENDER_PROBE");
        render_probe = (e && *e && *e != '0') ? 1 : 0;
    }
    long long t0 = render_probe ? render_now_ns() : 0;
    desktop_render(NULL, state->width, state->height);
    long long t1 = render_probe ? render_now_ns() : 0;
    if (startmenu_is_open()) {
        startmenu_render(NULL, state->width, state->height);
    }
    long long t2 = render_probe ? render_now_ns() : 0;
    desktop_tick();
    if (render_probe) {
        acc[0] += t1 - t0; acc[1] += t2 - t1; acc[2] += render_now_ns() - t2;
        if (++nacc >= 120) {
            fprintf(stderr, "render probe: desktop=%.2fms startmenu+tick=%.2fms tick=%.2fms per frame\n",
                    (double)acc[0] / nacc / 1e6, (double)acc[1] / nacc / 1e6,
                    (double)acc[2] / nacc / 1e6);
            acc[0] = acc[1] = acc[2] = 0; nacc = 0;
        }
    }
}

/* Drain the kernel input queue and route events to the focused window. */
bool hosted_input_dispatch(void) {
    bool any = false;
    KeyEvent kev;
    while (input_key_poll(&kev)) {
        any = true;
        /* The WM is press-driven: hotkeys toggle and text widgets append, so
         * a key-up would fire every action twice (Win+W open+close, "not"
         * typed as "nno oott"). */
        /* Ups are delivered to the one surface that needs them (the Alt+Tab switcher) and
         * nowhere else: a key-up reaching the press-driven WM would fire toggles twice. */
        if (kev.kind != KEY_EVENT_DOWN) {
            wm_handle_key_up(kev.keycode, kev.modifiers);
            continue;
        }
        wm_handle_key(kev.keycode, kev.modifiers);
    }
    MouseEvent mev;
    while (input_mouse_poll(&mev)) {
        any = true;
        int kind = (mev.kind >= 0) ? mev.kind
                                   : (mev.buttons ? 1 : 2); /* 1=down, 2=up */
        wm_handle_mouse(mev.x, mev.y, mev.buttons, kind);
        wubu_bonzi_activity();
    }
    return any;
}
