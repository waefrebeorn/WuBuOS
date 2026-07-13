/*
 * hosted_wayland.c -- WuBuOS hosted-mode Wayland client subsystem
 *
 * Extracted from hosted.c: SHM buffer pool, the wl_registry / xdg_wm_base /
 * xdg_surface / xdg_toplevel / keyboard / pointer / seat / output / touch
 * listeners, evdev-scancode -> WuBuOS key translation, and the Wayland frame
 * blit. C11 opaque-safe: shared globals are extern'd in hosted_internal.h;
 * the launcher core (hosted.c) drives this module via the hosted_wl_* entry
 * points declared there.
 */

#include "hosted.h"
#include "hosted_internal.h"

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
 /* ═══════════════════════════════════════════════════════════════
  * SHM Buffer Pool (double-buffered)
  * ═══════════════════════════════════════════════════════════════ */

 shm_buffer_t    g_shm_bufs[SHM_BUFFERS];
 int             g_cur_buf = 0;

 /* Wayland state instance */
 wayland_state_t g_wl;

 /* Primary selection device manager global */
 struct zwp_primary_selection_device_manager_v1 *g_primary_selection_manager = NULL;

 /* Shared Styx-namespace filesystem state (g_fs / g_nfiles / g_next_path) and
  * the styxfs_file_t type live in hosted_internal.h, defined in hosted_styxfs.c.
  * The FS submodule (hosted_styxfs.c) is compiled separately and linked into the
  * hosted binary. */

 /* Platform shutdown callback for dosgui_desktop */
 void dosgui_platform_shutdown(void) {
     if (g_hosted_state) g_hosted_state->running = false;
 }

/* ══════════════════════════════════════════════════════════════════
 * Forward Declarations
 * ══════════════════════════════════════════════════════════════════ */

static void shm_buffer_create(shm_buffer_t *buf, int w, int h);
static void shm_buffer_destroy(shm_buffer_t *buf);
static void handle_wl_keyboard_key(uint32_t key, int pressed);
static void handle_wl_pointer_button(uint32_t button, int pressed);
static void render_desktop(hosted_state_t *state);

/* find_fid() is shared with hosted_styxfs.c; declared in hosted_internal.h. */

/* ══════════════════════════════════════════════════════════════════
 * SHM Buffer Management
 * ══════════════════════════════════════════════════════════════════ */

static void shm_create_shared_memory(size_t size, int *fd, void **addr) {
    char template[] = "/tmp/wubu-shm-XXXXXX";
    *fd = mkstemp(template);
    if (*fd < 0) { perror("mkstemp"); return; }
    unlink(template);
    ftruncate(*fd, (off_t)size);
    *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (*addr == MAP_FAILED) { perror("mmap"); close(*fd); *fd = -1; }
}

static void shm_buffer_create(shm_buffer_t *buf, int w, int h) {
    memset(buf, 0, sizeof(*buf));
    buf->width = w;
    buf->height = h;
    buf->stride = w * 4;
    size_t size = (size_t)buf->stride * h;

    shm_create_shared_memory(size, &buf->fd, (void**)&buf->pixels);

    struct wl_shm_pool *pool = wl_shm_create_pool(g_wl.shm, buf->fd, (int32_t)size);
    buf->wl_buf = wl_shm_pool_create_buffer(pool, 0, (int32_t)w, (int32_t)h,
                                             (int32_t)buf->stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
}

static void shm_buffer_destroy(shm_buffer_t *buf) {
    if (buf->wl_buf) { wl_buffer_destroy(buf->wl_buf); buf->wl_buf = NULL; }
    if (buf->pixels) { munmap(buf->pixels, (size_t)buf->stride * buf->height); buf->pixels = NULL; }
    if (buf->fd >= 0) { close(buf->fd); buf->fd = -1; }
}

/* ══════════════════════════════════════════════════════════════════
 * Wayland Registry Callbacks
 * ══════════════════════════════════════════════════════════════════ */

static void registry_global(void *data, struct wl_registry *registry,
                             uint32_t name, const char *interface, uint32_t version) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    fprintf(stderr, "Wayland: global added (name=%u, interface=%s, version=%u)\n", name, interface, version);
    if (strcmp(interface, "wl_compositor") == 0) {
        g_wl.compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        g_wl.xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    } else if (strcmp(interface, "zwp_tablet_manager_v1") == 0) {
        /* Tablet manager not available in stable headers - skip */
    } else if (strcmp(interface, "wl_output") == 0) {
        g_wl.output = wl_registry_bind(registry, name, &wl_output_interface, 2);
    } else if (strcmp(interface, "wl_shm") == 0) {
        g_wl.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        g_wl.seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
        g_wl.keyboard = wl_seat_get_keyboard(g_wl.seat);
        g_wl.pointer = wl_seat_get_pointer(g_wl.seat);
    } else if (strcmp(interface, "wl_data_device_manager") == 0) {
        g_wl.data_device_manager = wl_registry_bind(registry, name, &wl_data_device_manager_interface, 1);
    } else if (strcmp(interface, "zwp_primary_selection_device_manager_v1") == 0) {
        g_wl.primary_selection_manager = wl_registry_bind(registry, name, &zwp_primary_selection_device_manager_v1_interface, 1);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    fprintf(stderr, "Wayland: global removed (name=%u)\n", name);
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/* ══════════════════════════════════════════════════════════════════
 * xdg_wm_base / xdg_surface / xdg_toplevel Callbacks
 * ══════════════════════════════════════════════════════════════════ */

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    xdg_wm_base_pong(xdg_wm_base, serial);
    fprintf(stderr, "Wayland: xdg_wm_base ping (serial=%u)\n", serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                                   int32_t width, int32_t height,
                                   struct wl_array *states) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    
    /* Ignore 0x0 configure (initial) */
    if (width <= 0 || height <= 0) return;
    
    /* Resize VBE framebuffer if changed */
    VBEState *vs = vbe_state();
    if (vs && (vs->width != width || vs->height != height)) {
        fprintf(stderr, "WuBuOS: resize %dx%d -> %dx%d\n", vs->width, vs->height, width, height);
        
        /* Destroy old SHM buffers */
        for (int i = 0; i < SHM_BUFFERS; i++) shm_buffer_destroy(&g_shm_bufs[i]);
        
        /* Reinit VBE with new size */
        vbe_shutdown();
        vbe_init(width, height);
        
        /* Update WM and desktop */
        dosgui_wm_init(width, height);
        
        /* Create new SHM buffers */
        shm_buffer_create(&g_shm_bufs[0], width, height);
        shm_buffer_create(&g_shm_bufs[1], width, height);
        
        state->width = width;
        state->height = height;
        state->fb_pitch = width * 4;
        if (state->framebuffer) {
            free(state->framebuffer);
            state->framebuffer = (uint32_t*)calloc((size_t)width * height, 4);
        }
    }
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (state) state->running = false;
}

static void xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *toplevel,
                                          int32_t width, int32_t height) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    
    fprintf(stderr, "Wayland: xdg_toplevel configure_bounds width=%d height=%d\n",
            width, height);
    
    /* Store max size bounds from compositor */
    state->max_width = width;
    state->max_height = height;
}

static void xdg_toplevel_wm_capabilities(void *data, struct xdg_toplevel *toplevel,
                                         struct wl_array *capabilities) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    
    uint32_t *caps = (uint32_t*)capabilities->data;
    size_t n_caps = capabilities->size / sizeof(uint32_t);
    
    state->wm_caps = 0;
    for (size_t i = 0; i < n_caps; i++) {
        state->wm_caps |= (1u << caps[i]);
        fprintf(stderr, "Wayland: wm_capability %u\n", caps[i]);
    }
    fprintf(stderr, "Wayland: wm_capabilities bitmap=0x%x\n", state->wm_caps);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
    .configure_bounds = xdg_toplevel_configure_bounds,
    .wm_capabilities = xdg_toplevel_wm_capabilities,
};

/* ══════════════════════════════════════════════════════════════════
 * Keyboard Callbacks
 * ══════════════════════════════════════════════════════════════════ */

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

/* Global key state map (256 keys, 0=up, 1=down) */
static uint32_t g_key_map[256];

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

/* ══════════════════════════════════════════════════════════════════
 * Pointer (Mouse) Callbacks
 * ══════════════════════════════════════════════════════════════════ */

static int g_pointer_x = 0, g_pointer_y = 0;
static int g_mouse_buttons = 0;

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

/* ════════════════════════════════════════════════════════════════════════
 * Key Translation (Linux evdev scancode -> WuBuOS key code)
 * ════════════════════════════════════════════════════════════════════════ */

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

/* ════════════════════════════════════════════════════════════════════════
 * wl_seat Capabilities Callback
 * ═══════════════════════════════════════════════════════════════════════ */

static void seat_capabilities(void *data, struct wl_seat *seat,
                              uint32_t capabilities);

static void seat_name(void *data, struct wl_seat *seat, const char *name);

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

/* ═══════════════════════════════════════════════════════════════════════
 * wl_output Callbacks
 * ═══════════════════════════════════════════════════════════════════════ */

static void output_geometry(void *data, struct wl_output *wl_output,
                            int32_t x, int32_t y, int32_t physical_width,
                            int32_t physical_height, int32_t subpixel,
                            const char *make, const char *model,
                            int32_t transform);

static void output_mode(void *data, struct wl_output *wl_output,
                        uint32_t flags, int32_t width, int32_t height,
                        int32_t refresh);

static void output_done(void *data, struct wl_output *wl_output);

static void output_scale(void *data, struct wl_output *wl_output,
                         int32_t factor);

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
};

/* ═══════════════════════════════════════════════════════════════════════
 * wl_touch Callbacks
 * ═══════════════════════════════════════════════════════════════════════ */

static void touch_down(void *data, struct wl_touch *wl_touch,
                       uint32_t serial, uint32_t time, struct wl_surface *surface,
                       int32_t id, wl_fixed_t x_w, wl_fixed_t y_w);

static void touch_up(void *data, struct wl_touch *wl_touch,
                     uint32_t serial, uint32_t time, int32_t id);

static void touch_motion(void *data, struct wl_touch *wl_touch,
                         uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w);

static void touch_frame(void *data, struct wl_touch *wl_touch);

static void touch_cancel(void *data, struct wl_touch *wl_touch);

static const struct wl_touch_listener touch_listener = {
    .down = touch_down,
    .up = touch_up,
    .motion = touch_motion,
    .frame = touch_frame,
    .cancel = touch_cancel,
};

/* ═════════════════════════════════════════════════════════════════════════
 * wl_seat Capabilities Callback Implementation
 * ═══════════════════════════════════════════════════════════════════════ */

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

/* ════════════════════════════════════════════════════════════════════════
 * wl_output Callbacks Implementation
 * ════════════════════════════════════════════════════════════════════════ */

static void output_geometry(void *data, struct wl_output *wl_output,
                            int32_t x, int32_t y, int32_t physical_width,
                            int32_t physical_height, int32_t subpixel,
                            const char *make, const char *model,
                            int32_t transform) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    fprintf(stderr, "Wayland: output geometry %dx%d at %d,%d\n",
            physical_width, physical_height, x, y);
}

static void output_mode(void *data, struct wl_output *wl_output,
                        uint32_t flags, int32_t width, int32_t height,
                        int32_t refresh) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        fprintf(stderr, "Wayland: output mode %dx%d@%dHz\n", width, height, refresh);
    }
}

static void output_done(void *data, struct wl_output *wl_output) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    fprintf(stderr, "Wayland: output done\n");
}

static void output_scale(void *data, struct wl_output *wl_output,
                         int32_t factor) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    fprintf(stderr, "Wayland: output scale %d\n", factor);
}

/* ════════════════════════════════════════════════════════════════════════
 * wl_touch Callbacks Implementation
 * ════════════════════════════════════════════════════════════════════════ */

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


/* ── Launcher-core linkage ───────────────────────────────────────
 * g_hosted_state is defined in hosted.c; this module reads it. */
extern hosted_state_t *g_hosted_state;

/* Connect to Wayland, bind globals, create the surface + SHM buffers.
 * Returns 0 on success, -1 if no display is available. */
int hosted_wl_connect(hosted_state_t *state) {
    memset(&g_wl, 0, sizeof(g_wl));

    struct wl_display *dpy = wl_display_connect(NULL);
    if (!dpy) {
        fprintf(stderr, "No Wayland display. Use -h for headless.\n");
        return -1;
    }
    g_wl.display = dpy;

    struct wl_registry *registry = wl_display_get_registry(dpy);
    wl_registry_add_listener(registry, &registry_listener, state);
    wl_display_roundtrip(dpy);

    if (g_wl.xdg_wm_base) {
        xdg_wm_base_add_listener(g_wl.xdg_wm_base, &xdg_wm_base_listener, state);
    }

    g_wl.surface = wl_compositor_create_surface(g_wl.compositor);
    if (g_wl.xdg_wm_base) {
        g_wl.xdg_surface = xdg_wm_base_get_xdg_surface(g_wl.xdg_wm_base, g_wl.surface);
        xdg_surface_add_listener(g_wl.xdg_surface, &xdg_surface_listener, state);
        g_wl.xdg_toplevel = xdg_surface_get_toplevel(g_wl.xdg_surface);
        xdg_toplevel_add_listener(g_wl.xdg_toplevel, &xdg_toplevel_listener, state);
        xdg_toplevel_set_title(g_wl.xdg_toplevel, "WuBuOS");
        wl_surface_commit(g_wl.surface);
    }

    if (g_wl.keyboard) {
        wl_keyboard_add_listener(g_wl.keyboard, &keyboard_listener, state);
    }
    if (g_wl.pointer) {
        wl_pointer_add_listener(g_wl.pointer, &pointer_listener, state);
    }
    if (g_wl.seat) {
        wl_seat_add_listener(g_wl.seat, &seat_listener, state);
    }
    if (g_wl.output) {
        wl_output_add_listener(g_wl.output, &output_listener, state);
    }

    shm_buffer_create(&g_shm_bufs[0], state->width, state->height);
    shm_buffer_create(&g_shm_bufs[1], state->width, state->height);

    g_wl.width  = state->width;
    g_wl.height = state->height;
    g_wl.shm_buffer = g_shm_bufs[0].wl_buf;
    g_wl.shm_data   = g_shm_bufs[0].pixels;

    fprintf(stderr, "WuBuOS: Wayland %dx%d window\n", state->width, state->height);
    return 0;
}

/* Pump pending Wayland events. */
void hosted_wl_dispatch(void) {
    if (g_wl.display) {
        wl_display_dispatch_pending(g_wl.display);
        wl_display_flush(g_wl.display);
    }
}

/* Blit the VBE back-buffer into the current SHM buffer + commit. */
void hosted_wl_frame_render(void) {
    if (!g_wl.surface || !g_wl.compositor) return;
    shm_buffer_t *buf = &g_shm_bufs[g_cur_buf];
    if (!buf->pixels) return;

    VBEState *vs = vbe_state();
    if (vs && vs->fb) {
        memcpy(buf->pixels, vs->fb,
               (size_t)g_hosted_state->width * g_hosted_state->height * 4);
    }

    if (wubu_screenshot_has_active_region_selector()) {
        wubu_screenshot_render_region_selector(buf->pixels, buf->width, buf->height);
    }

    wl_surface_attach(g_wl.surface, buf->wl_buf, 0, 0);
    wl_surface_damage_buffer(g_wl.surface, 0, 0, buf->width, buf->height);
    wl_surface_commit(g_wl.surface);

    g_cur_buf = 1 - g_cur_buf;
}

/* Tear down the Wayland connection (destroy globals, disconnect). */
void hosted_wl_disconnect(void) {
    for (int i = 0; i < SHM_BUFFERS; i++) shm_buffer_destroy(&g_shm_bufs[i]);
    if (g_wl.xdg_toplevel)            xdg_toplevel_destroy(g_wl.xdg_toplevel);
    if (g_wl.xdg_surface)             xdg_surface_destroy(g_wl.xdg_surface);
    if (g_wl.surface)                 wl_surface_destroy(g_wl.surface);
    if (g_wl.xdg_wm_base)            xdg_wm_base_destroy(g_wl.xdg_wm_base);
    if (g_wl.pointer)                 wl_pointer_destroy(g_wl.pointer);
    if (g_wl.keyboard)                wl_keyboard_destroy(g_wl.keyboard);
    if (g_wl.seat)                    wl_seat_destroy(g_wl.seat);
    if (g_wl.shm)                     wl_shm_destroy(g_wl.shm);
    if (g_wl.data_device_manager)      wl_data_device_manager_destroy(g_wl.data_device_manager);
    if (g_wl.primary_selection_manager) zwp_primary_selection_device_manager_v1_destroy(g_wl.primary_selection_manager);
    if (g_wl.compositor)             wl_compositor_destroy(g_wl.compositor);
    if (g_wl.output)                  wl_output_destroy(g_wl.output);
    if (g_wl.touch)                   wl_touch_destroy(g_wl.touch);
    if (g_wl.display) { wl_display_flush(g_wl.display); wl_display_disconnect(g_wl.display); }
    memset(&g_wl, 0, sizeof(g_wl));
}
