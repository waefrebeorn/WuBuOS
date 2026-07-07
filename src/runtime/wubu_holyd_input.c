/*
 * wubu_holyd_input.c  --  WuBuOS HolyC DOS Daemon: Input
 */

#include "wubu_holyd_internal.h"

/* -- Input Routing ------------------------------------------------ */

int wubu_holyd_input_key(WubuHoly *d, const char *session,
                           int keycode, int modifiers) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    /* Add to input queue */
    int tail = s->input_tail;
    /* Simple: store keycode as char for now */
    if (keycode >= 0 && keycode < 256 && tail < (int)sizeof(s->input_buf) - 1) {
        s->input_buf[tail] = (char)keycode;
        s->input_tail = tail + 1;
    }
    s->last_active = time(NULL);
    return 0;
}

int wubu_holyd_input_mouse(WubuHoly *d, const char *session,
                             int x, int y, int buttons) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;

    /* Route to focused window's mouse handler */
    if (s->focused_window >= 0 && s->focused_window < s->window_count) {
        WubuHolyWindow *win = &s->windows[s->focused_window];
        /* Compute local coordinates relative to window */
        int local_x = x - win->x;
        int local_y = y - win->y;
        /* Only forward if mouse is within window bounds */
        if (local_x >= 0 && local_x < win->w &&
            local_y >= 0 && local_y < win->h && win->visible) {
            /* Hit test: window accepts the mouse event.
             * Forward to the DosGui WM which handles focus,
             * drag, resize, and client-area dispatch. */
            {
                /* Translate buttons bitmask to btn+kind.
                 * buttons: bit0=LMB, bit1=RMB, bit2=MMB.
                 * kind: 0=move, 1=down, 2=up.
                 * We detect transitions via prev_buttons. */
                int prev = s->prev_buttons;
                int kind = 0; /* default: move (no change) */
                int btn = 0;
                if (buttons && !prev) {
                    kind = 1; /* press */
                    btn = (buttons & 1) ? 1 : (buttons & 4) ? 3 : 2;
                } else if (!buttons && prev) {
                    kind = 2; /* release */
                    btn = (prev & 1) ? 1 : (prev & 4) ? 3 : 2;
                }
                dosgui_wm_handle_mouse(x, y, btn, kind);
                s->prev_buttons = buttons;
            }
        }
    } else {
        /* No focused window: try to find the window under the cursor */
        for (int i = 0; i < s->window_count; ++i) {
            WubuHolyWindow *win = &s->windows[i];
            if (!win->visible) continue;
            if (x >= win->x && x < win->x + win->w &&
                y >= win->y && y < win->y + win->h) {
                s->focused_window = i;
                /* Clear focus from other windows */
                for (int j = 0; j < s->window_count; ++j) {
                    s->windows[j].focused = (j == i);
                }
                break;
            }
        }
    }

    s->last_active = time(NULL);
    return 0;
}

int wubu_holyd_input_paste(WubuHoly *d, const char *session,
                             const char *text) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s || !text) return -1;
    int len = strlen(text);
    int avail = (int)sizeof(s->input_buf) - s->input_tail;
    if (len > avail) len = avail;
    memcpy(s->input_buf + s->input_tail, text, len);
    s->input_tail += len;
    s->last_active = time(NULL);
    return 0;
}

