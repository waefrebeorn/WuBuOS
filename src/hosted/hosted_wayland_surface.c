#include "../egojar.h"
#include "../egojar_net.h"
#include "hosted.h"
#include "hosted_internal.h"
#include "wayland_state.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"
#include "../gui/wm.h"
#include "../gui/shell/desktop.h"
#include "../gui/shell/startmenu.h"
#include "../gui/wubu_screenshot.h"
#include "../gui/wubu_theme.h"
#include "../bridge/bridge.h"
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "xdg-shell-client.header"
#include "primary-selection-client.header"

/* === hosted_wayland_surface.c -- registry/xdg/output listeners + surface lifecycle === */
/* Primary selection device manager global (owned by this module). */
struct zwp_primary_selection_device_manager_v1 *g_primary_selection_manager = NULL;


/* Defined below with the other output listeners; referenced here because an
 * output is bound -- and must be listened to -- during registry enumeration. */
static const struct wl_output_listener output_listener;

static void registry_global(void *data, struct wl_registry *registry,
                             uint32_t name, const char *interface, uint32_t version) {
    hosted_state_t *state = (hosted_state_t*)data;
    if (!state) return;
    fprintf(stderr, "Wayland: global added (name=%u, interface=%s, version=%u)\n", name, interface, version);
    if (strcmp(interface, "wl_compositor") == 0) {
        /* Bind the highest version we can USE, not always 1: damage_buffer()
         * is a wl_surface method since version 5, and calling it on a v1
         * surface is a protocol error that makes the compositor abort the
         * client (weston: "error in client communication"). */
        uint32_t want = version < 5 ? version : 5;
        g_wl.compositor = wl_registry_bind(registry, name, &wl_compositor_interface, want);
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        g_wl.xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    } else if (strcmp(interface, "zwp_tablet_manager_v1") == 0) {
        /* Tablet manager not available in stable headers - skip */
    } else if (strcmp(interface, "wl_output") == 0) {
        /* Bind EVERY output, not just one.  The old code overwrote a single pointer,
         * so on a two-monitor host only the last-advertised display was ever heard
         * from -- and its events were printed to stderr and thrown away anyway. */
        if (g_wl.n_outputs < HOSTED_MAX_DISPLAYS) {
            hosted_display_t *d = hosted_display_slot(&g_wl.displays, name);
            struct wl_output *res =
                wl_registry_bind(registry, name, &wl_output_interface, 2);
            g_wl.outputs[g_wl.n_outputs++] = res;
            if (d) wl_output_add_listener(res, &output_listener, d);
        }
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
    /* From here on the surface is configured and a buffer may be attached. */
    g_wl.configured = 1;
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

    VBEState *vs = vbe_state();
    if (!vs || (vs->width == width && vs->height == height)) return;

    fprintf(stderr, "WuBuOS: resize %dx%d -> %dx%d\n",
            vs->width, vs->height, width, height);

    /* Detach the host surface BEFORE the shm buffers go away.  The VBE's
     * fb/back point at g_shm_bufs[0].pixels; vbe_shutdown() would free()
     * those pointers, but they belong to mmap (already munmap'd by
     * shm_buffer_destroy) -- an instant segfault. */
    vbe_backend_t *be = (vbe_backend_t *)vs->backend;
    vs->backend = NULL;
    vs->fb = NULL;
    vs->back = NULL;

    for (int i = 0; i < SHM_BUFFERS; i++) shm_buffer_destroy(&g_shm_bufs[i]);
    vbe_shutdown();

    state->width = width;
    state->height = height;
    state->fb_pitch = width * 4;
    if (state->framebuffer) {
        free(state->framebuffer);
        state->framebuffer = (uint32_t*)calloc((size_t)width * height, 4);
    }

    vbe_init(width, height);
    shm_buffer_create(&g_shm_bufs[0], width, height);
    shm_buffer_create(&g_shm_bufs[1], width, height);
    if (be)
        vbe_set_backend(VBE_BACKEND_WAYLAND, g_shm_bufs[0].pixels,
                        width, height, be);
    /* Fresh buffers: start rendering into buffer 0. */
    g_shm_render_idx = 0;

    /* The WM and desktop are sized to the screen; resize in place so the
     * open windows and desktop icons survive. */
    wm_set_screen_size(width, height);
    g_wl.configured = 1;
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

static void output_geometry(void *data, struct wl_output *wl_output,
                            int32_t x, int32_t y, int32_t physical_width,
                            int32_t physical_height, int32_t subpixel,
                            const char *make, const char *model,
                            int32_t transform) {
    (void)wl_output; (void)physical_width; (void)physical_height;
    (void)subpixel; (void)transform;
    hosted_display_t *d = (hosted_display_t *)data;
    if (!d) return;

    d->x = x;                       /* virtual-desktop position */
    d->y = y;
    snprintf(d->make,  sizeof(d->make),  "%s", make  ? make  : "");
    snprintf(d->model, sizeof(d->model), "%s", model ? model : "");
}

static void output_mode(void *data, struct wl_output *wl_output,
                        uint32_t flags, int32_t width, int32_t height,
                        int32_t refresh) {
    (void)wl_output;
    hosted_display_t *d = (hosted_display_t *)data;
    if (!d) return;
    if (!(flags & WL_OUTPUT_MODE_CURRENT)) return;   /* stale mode, not live */

    d->w = width;
    d->h = height;
    d->refresh_mhz = refresh / 1000;
}

static void output_done(void *data, struct wl_output *wl_output) {
    (void)wl_output;
    /* Geometry and mode arrive as separate events; `done` marks the burst complete
     * and makes the display usable.  Before this it is not counted as present, so a
     * half-described output can never be tiled onto. */
    hosted_display_commit((hosted_display_t *)data);
}

static void output_scale(void *data, struct wl_output *wl_output,
                         int32_t factor) {
    (void)wl_output;
    hosted_display_t *d = (hosted_display_t *)data;
    if (!d || factor < 1) return;
    d->scale = factor;
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
};
/* Bind Wayland globals, create the xdg surface + toplevel, attach output. */
void wl_surface_init(hosted_state_t *state) {
    struct wl_registry *registry = wl_display_get_registry(g_wl.display);
    wl_registry_add_listener(registry, &registry_listener, state);
    wl_display_roundtrip(g_wl.display);

    if (g_wl.xdg_wm_base)
        xdg_wm_base_add_listener(g_wl.xdg_wm_base, &xdg_wm_base_listener, state);

    g_wl.surface = wl_compositor_create_surface(g_wl.compositor);
    if (g_wl.xdg_wm_base) {
        g_wl.xdg_surface = xdg_wm_base_get_xdg_surface(g_wl.xdg_wm_base, g_wl.surface);
        xdg_surface_add_listener(g_wl.xdg_surface, &xdg_surface_listener, state);
        g_wl.xdg_toplevel = xdg_surface_get_toplevel(g_wl.xdg_surface);
        xdg_toplevel_add_listener(g_wl.xdg_toplevel, &xdg_toplevel_listener, state);
        xdg_toplevel_set_title(g_wl.xdg_toplevel, "WuBuOS");
        wl_surface_commit(g_wl.surface);
    }
}

/* Destroy surface/xdg/output/data-device globals (not input/SHM -- those
 * have their own term fns). */
void wl_surface_term(void) {
    if (g_wl.xdg_toplevel)               xdg_toplevel_destroy(g_wl.xdg_toplevel);
    if (g_wl.xdg_surface)                xdg_surface_destroy(g_wl.xdg_surface);
    if (g_wl.surface)                    wl_surface_destroy(g_wl.surface);
    if (g_wl.xdg_wm_base)                xdg_wm_base_destroy(g_wl.xdg_wm_base);
    for (int i = 0; i < g_wl.n_outputs; i++)
        if (g_wl.outputs[i]) wl_output_destroy(g_wl.outputs[i]);
    g_wl.n_outputs = 0;
    hosted_display_reset(&g_wl.displays);
    if (g_wl.data_device_manager)        wl_data_device_manager_destroy(g_wl.data_device_manager);
    if (g_primary_selection_manager)     zwp_primary_selection_device_manager_v1_destroy(g_primary_selection_manager);
    g_primary_selection_manager = NULL;
}
