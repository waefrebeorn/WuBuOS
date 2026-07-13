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

/* === hosted_wayland_surface.c -- registry/xdg/output listeners + surface lifecycle === */
/* Primary selection device manager global (owned by this module). */
struct zwp_primary_selection_device_manager_v1 *g_primary_selection_manager = NULL;


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
    if (g_wl.output)
        wl_output_add_listener(g_wl.output, &output_listener, state);
}

/* Destroy surface/xdg/output/data-device globals (not input/SHM -- those
 * have their own term fns). */
void wl_surface_term(void) {
    if (g_wl.xdg_toplevel)               xdg_toplevel_destroy(g_wl.xdg_toplevel);
    if (g_wl.xdg_surface)                xdg_surface_destroy(g_wl.xdg_surface);
    if (g_wl.surface)                    wl_surface_destroy(g_wl.surface);
    if (g_wl.xdg_wm_base)                xdg_wm_base_destroy(g_wl.xdg_wm_base);
    if (g_wl.output)                     wl_output_destroy(g_wl.output);
    if (g_wl.data_device_manager)        wl_data_device_manager_destroy(g_wl.data_device_manager);
    if (g_primary_selection_manager)     zwp_primary_selection_device_manager_v1_destroy(g_primary_selection_manager);
    g_primary_selection_manager = NULL;
}
