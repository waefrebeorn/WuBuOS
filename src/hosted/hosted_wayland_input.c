#include "hosted.h"
#include "hosted_internal.h"
#include "wayland_state.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_desktop.h"
#include "../gui/dosgui_startmenu.h"
#include "../gui/wubu_screenshot.h"
#include "../gui/wubu_theme.h"
#include "../bridge/bridge.h"
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "xdg-shell-client.header"
#include "primary-selection-client.header"

/* === hosted_wayland_input.c -- keyboard/pointer/seat/touch listeners + key translation === */
/* Per-device input state (owned by this module). */
static uint32_t g_key_map[256];
static int g_pointer_x = 0, g_pointer_y = 0;
static int g_mouse_buttons = 0;
static void handle_wl_keyboard_key(uint32_t key, int pressed);
static void handle_wl_pointer_button(uint32_t button, int pressed);

static void handle_wl_keyboard_key(uint32_t key, int pressed) {
    uint32_t wu_key = 0;
    switch (key) {
    case 99:  /* KEY_SYSRQ / PrintScr */
        if (pressed) {
            if (g_key_map[56] || g_key_map[100]) {      /* Alt+PrintScr = focused window */
                wubu_screenshot_handle_alt_printscr();
            } else if (g_key_map[42] || g_key_map[54]) { /* Shift+PrintScr = region select */
                wubu_screenshot_handle_shift_printscr();
            } else {                                     /* PrintScr = full screen */
                wubu_screenshot_handle_printscr();
            }
        }
        break;
    case 28:  wu_key = 0x1C; break;  /* KEY_ENTER */
    case 1:   wu_key = 0x01; break;  /* KEY_ESC */
    case 14:  wu_key = 0x0E; break;  /* KEY_BACKSPACE */
    case 15:  wu_key = 0x0F; break;  /* KEY_TAB */
    case 29:  wu_key = 0x1D; break;  /* KEY_LEFTCTRL */
    case 42:  wu_key = 0x2A; break;  /* KEY_LEFTSHIFT */
    case 56:  wu_key = 0x38; break;  /* KEY_LEFTALT */
    case 57:  wu_key = 0x39; break;  /* KEY_SPACE */
    case 105: wu_key = 0xE04B; break; /* KEY_LEFT */
    case 103: wu_key = 0xE048; break; /* KEY_UP */
    case 106: wu_key = 0xE04D; break; /* KEY_RIGHT */
    case 108: wu_key = 0xE050; break; /* KEY_DOWN */
    case 20:  /* KEY_T */
        if (g_key_map[29] && g_key_map[56]) {
            bridge_toggle_mode();
            if (g_hosted_state)
                hosted_set_mode(g_hosted_state,
                    bridge_get_mode() == MODE_TEMPLE ? HMODE_TEMPLE : HMODE_GUI);
        } else if (g_key_map[29] && !g_key_map[56]) {
            /* Ctrl+T (without Alt) = cycle theme */
            if (pressed) {
                wubu_theme_cycle();
                fprintf(stderr, "Theme: %s\n", wubu_theme_name(wubu_theme_current()));
            }
        }
        wu_key = 0x14;
        break;
    default:
        if (key >= 30 && key <= 38) wu_key = 0x1E + (uint32_t)(key - 30); /* A-L */
        else if (key >= 44 && key <= 50) wu_key = 0x1E + (uint32_t)(key - 44 + 16); /* Z-M */
        else if (key >= 16 && key <= 25) wu_key = 0x1E + (uint32_t)(key - 16); /* Q-P */
        else if (key >= 2 && key <= 11) wu_key = 0x0B + (uint32_t)(key - 2); /* 1-0 */
        else if (key >= 59 && key <= 68) wu_key = 0x3B + (uint32_t)(key - 59); /* F1-F10 */
        else if (key == 87) wu_key = 0x3B + 10; /* F11 */
        else if (key == 88) wu_key = 0x3B + 11; /* F12 */
        break;
    }

    if (wu_key) {
        KeyEvent ev = {0};
        ev.scancode = wu_key;
        ev.keycode = wu_key;
        ev.kind = pressed ? KEY_EVENT_DOWN : KEY_EVENT_UP;
        uint32_t mods = 0;
        if (g_key_map[42] || g_key_map[54]) mods |= MOD_SHIFT;
        if (g_key_map[29] || g_key_map[97]) mods |= MOD_CTRL;
        if (g_key_map[56] || g_key_map[100]) mods |= MOD_ALT;
        ev.modifiers = mods;
        input_key_push(ev);
    }
}

static void handle_wl_pointer_button(uint32_t button, int pressed) {
    if (button == 272) { if (pressed) g_mouse_buttons |= 1; else g_mouse_buttons &= ~1; }
    else if (button == 273) { if (pressed) g_mouse_buttons |= 2; else g_mouse_buttons &= ~2; }
    else if (button == 274) { if (pressed) g_mouse_buttons |= 4; else g_mouse_buttons &= ~4; }
}

static void keyboard_keymap(void *data, struct wl_keyboard *wl_kb,
                             uint32_t format, int32_t fd, uint32_t size) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        fprintf(stderr, "Wayland: unexpected keymap format %u\n", format);
        close(fd);
        return;
    }
    close(fd);
}

static void keyboard_enter(void *data, struct wl_keyboard *wl_kb,
                            uint32_t serial, struct wl_surface *surface,
                            struct wl_array *keys) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    fprintf(stderr, "Wayland: keyboard enter (serial=%u)\n", serial);
}

static void keyboard_leave(void *data, struct wl_keyboard *wl_kb,
                            uint32_t serial, struct wl_surface *surface) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    memset(g_key_map, 0, sizeof(g_key_map));
    fprintf(stderr, "Wayland: keyboard leave (serial=%u)\n", serial);
}

static void keyboard_key(void *data, struct wl_keyboard *wl_kb,
                          uint32_t serial, uint32_t time, uint32_t key,
                          uint32_t key_state) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    int pressed = (key_state == WL_KEYBOARD_KEY_STATE_PRESSED);
    handle_wl_keyboard_key(key, pressed);
    if (key < 256) g_key_map[key] = pressed;
}

static void keyboard_modifiers(void *data, struct wl_keyboard *wl_kb,
                                uint32_t serial, uint32_t mods_depressed,
                                uint32_t mods_latched, uint32_t mods_locked,
                                uint32_t group) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    /* Track modifiers from canonical source */
    g_key_map[42] = (mods_depressed & 1) ? 1 : 0;  /* Left Shift */
    g_key_map[54] = (mods_depressed & 1) ? 1 : 0;  /* Right Shift */
    g_key_map[29] = (mods_depressed & 4) ? 1 : 0;  /* Left Ctrl */
    g_key_map[97] = (mods_depressed & 4) ? 1 : 0;  /* Right Ctrl */
    g_key_map[56] = (mods_depressed & 8) ? 1 : 0;  /* Left Alt */
    g_key_map[100] = (mods_depressed & 8) ? 1 : 0; /* Right Alt */
    if (mods_locked & 2) g_key_map[58] = 1;  /* CapsLock */
    fprintf(stderr, "Wayland: modifiers (serial=%u, depressed=%u, group=%u)\n", serial, mods_depressed, group);
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *wl_kb,
                                   int32_t rate, int32_t delay) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    fprintf(stderr, "Wayland: keyboard repeat info (rate=%d, delay=%dms)\n", rate, delay);
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

static void pointer_enter(void *data, struct wl_pointer *wl_ptr,
                           uint32_t serial, struct wl_surface *surface,
                           wl_fixed_t sx, wl_fixed_t sy) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    g_pointer_x = wl_fixed_to_int(sx);
    g_pointer_y = wl_fixed_to_int(sy);
}

static void pointer_leave(void *data, struct wl_pointer *wl_ptr,
                           uint32_t serial, struct wl_surface *surface) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    g_pointer_x = 0; g_pointer_y = 0; g_mouse_buttons = 0;
}

static void pointer_motion(void *data, struct wl_pointer *wl_ptr,
                            uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    hosted_state_t *state = (hosted_state_t*)data;
    int x = wl_fixed_to_int(sx);
    int y = wl_fixed_to_int(sy);
    int dx = x - g_pointer_x;
    int dy = y - g_pointer_y;
    g_pointer_x = x;
    g_pointer_y = y;

    if (state && (dx != 0 || dy != 0)) {
        MouseEvent ev = {0};
        ev.dx = dx;
        ev.dy = dy;
        ev.buttons = g_mouse_buttons;
        ev.scroll = 0;
        input_mouse_push(ev);
    }
    /* Track start menu hover */
    dosgui_startmenu_track_hover(x, y);

    /* Update region selector if active */
    if (wubu_screenshot_has_active_region_selector()) {
        wubu_screenshot_update_region_selector(x, y, g_mouse_buttons & 1);
    }
}

static void pointer_button(void *data, struct wl_pointer *wl_ptr,
                            uint32_t serial, uint32_t time,
                            uint32_t button, uint32_t state_wl) {
    hosted_state_t *state = (hosted_state_t*)data;
    int pressed = (state_wl == WL_POINTER_BUTTON_STATE_PRESSED);

    handle_wl_pointer_button(button, pressed);

    if (state) {
        MouseEvent ev = {0};
        ev.dx = 0;
        ev.dy = 0;
        ev.buttons = g_mouse_buttons;
        ev.scroll = 0;
        input_mouse_push(ev);
    }
}

static void pointer_axis(void *data, struct wl_pointer *wl_ptr,
                          uint32_t time, uint32_t axis, wl_fixed_t value) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (state && axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        MouseEvent ev = {0};
        ev.dx = 0;
        ev.dy = 0;
        ev.scroll = wl_fixed_to_int(value);
        input_mouse_push(ev);
    }
}

static void pointer_frame(void *data, struct wl_pointer *wl_ptr) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
}

static void pointer_axis_source(void *data, struct wl_pointer *wl_ptr,
                                  uint32_t axis_source) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    fprintf(stderr, "Wayland: axis source=%u\n", axis_source);
}

static void pointer_axis_stop(void *data, struct wl_pointer *wl_ptr,
                               uint32_t time, uint32_t axis) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        MouseEvent ev = {0};
        ev.scroll = 0;
        input_mouse_push(ev);
    }
}

static void pointer_axis_discrete(void *data, struct wl_pointer *wl_ptr,
                                    uint32_t axis, int32_t discrete) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        MouseEvent ev = {0};
        ev.scroll = discrete * 3;
        input_mouse_push(ev);
    }
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
};

static void seat_capabilities(void *data, struct wl_seat *seat,
                              uint32_t capabilities) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    if ((capabilities & WL_SEAT_CAPABILITY_TOUCH) && !state->touch) {
        state->touch = wl_seat_get_touch(seat);
        wl_touch_add_listener(state->touch, &touch_listener, state);
        fprintf(stderr, "Wayland: touch enabled\n");
    } else if (!(capabilities & WL_SEAT_CAPABILITY_TOUCH) && state->touch) {
        wl_touch_destroy(state->touch);
        state->touch = NULL;
        fprintf(stderr, "Wayland: touch disabled\n");
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name) {
    (void)data; (void)seat; (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static void touch_down(void *data, struct wl_touch *wl_touch,
                       uint32_t serial, uint32_t time, struct wl_surface *surface,
                       int32_t id, wl_fixed_t x_w, wl_fixed_t y_w) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    int x = wl_fixed_to_int(x_w);
    int y = wl_fixed_to_int(y_w);
    g_mouse_buttons |= 1;
    MouseEvent ev = {0};
    ev.dx = 0;
    ev.dy = 0;
    ev.buttons = g_mouse_buttons;
    ev.scroll = 0;
    input_mouse_push(ev);
    fprintf(stderr, "Wayland: touch down at %d,%d\n", x, y);
}

static void touch_up(void *data, struct wl_touch *wl_touch,
                     uint32_t serial, uint32_t time, int32_t id) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    g_mouse_buttons &= ~1;
    MouseEvent ev = {0};
    ev.dx = 0;
    ev.dy = 0;
    ev.buttons = g_mouse_buttons;
    ev.scroll = 0;
    input_mouse_push(ev);
    fprintf(stderr, "Wayland: touch up\n");
}

static void touch_motion(void *data, struct wl_touch *wl_touch,
                         uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    int x = wl_fixed_to_int(x_w);
    int y = wl_fixed_to_int(y_w);
    int dx = x - g_pointer_x;
    int dy = y - g_pointer_y;
    g_pointer_x = x;
    g_pointer_y = y;
    if (state && (dx != 0 || dy != 0)) {
        MouseEvent ev = {0};
        ev.dx = dx;
        ev.dy = dy;
        ev.buttons = g_mouse_buttons;
        ev.scroll = 0;
        input_mouse_push(ev);
    }
    fprintf(stderr, "Wayland: touch motion to %d,%d\n", x, y);
}

static void touch_frame(void *data, struct wl_touch *wl_touch) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    fprintf(stderr, "Wayland: touch frame\n");
}

static void touch_cancel(void *data, struct wl_touch *wl_touch) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    g_mouse_buttons = 0;
    MouseEvent ev = {0};
    ev.dx = 0;
    ev.dy = 0;
    ev.buttons = g_mouse_buttons;
    ev.scroll = 0;
    input_mouse_push(ev);
    fprintf(stderr, "Wayland: touch cancel\n");
}

const struct wl_touch_listener touch_listener = {
    .down = touch_down,
    .up = touch_up,
    .motion = touch_motion,
    .frame = touch_frame,
    .cancel = touch_cancel,
};
/* Attach keyboard/pointer/seat/touch listeners to the live globals. */
void wl_input_init(hosted_state_t *state) {
    if (g_wl.keyboard) wl_keyboard_add_listener(g_wl.keyboard, &keyboard_listener, state);
    if (g_wl.pointer)  wl_pointer_add_listener(g_wl.pointer, &pointer_listener, state);
    if (g_wl.seat)     wl_seat_add_listener(g_wl.seat, &seat_listener, state);
}

/* Detach + destroy input globals. */
void wl_input_term(void) {
    if (g_wl.touch)    { wl_touch_destroy(g_wl.touch);    g_wl.touch = NULL; }
    if (g_wl.keyboard) { wl_keyboard_destroy(g_wl.keyboard); g_wl.keyboard = NULL; }
    if (g_wl.pointer)  { wl_pointer_destroy(g_wl.pointer);   g_wl.pointer = NULL; }
    if (g_wl.seat)     { wl_seat_destroy(g_wl.seat);         g_wl.seat = NULL; }
}
