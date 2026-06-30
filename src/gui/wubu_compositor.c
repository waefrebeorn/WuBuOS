/*
 * wubu_compositor.c  --  WuBuOS Wayland-Native Compositor
 *
 * Phase 1: Wayland Compositor Core
 * - DRM/KMS + GBM backend (bare metal)
 * - Vulkan renderer via VSL GPU drivers
 * - xdg_shell + xdg_decoration + fractional_scale
 * - Damage tracking + triple-buffer + vsync
 * - zwp_linux_dmabuf_v1 + explicit_sync
 * - 9P IPC (no D-Bus)
 *
 * Historical mistakes we avoid:
 * - No X11 legacy
 * - No Clutter/Cogl scene graph duplication
 * - No GObject boilerplate
 * - No CSS theming engine
 * - No AT-SPI2/D-Bus accessibility
 * - No IBus daemon
 * - No Flatpak portals
 *
 * We use: wlroots (proven Wayland protocols), VSL (GPU), 9P (IPC)
 */

#define _GNU_SOURCE
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/drm.h>
#include <wlr/render/vulkan.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/util/log.h>
#include <wlr/util/box.h>

#include <vulkan/vulkan.h>
#include <gbm.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <libudev.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <assert.h>

#include "wubu_compositor.h"
#include "wubu_vsl.h"

/* ================================================================
 * WuBuObject - Better than GObject
 * ================================================================ */

typedef void (*WuBuObjectFini)(void *obj);

typedef struct {
    const char *type_name;
    WuBuObjectFini fini;
} WuBuObjectHeader;

#define WUBU_OBJECT_HEADER \
    WuBuObjectHeader _header;

#define WUBU_OBJECT_NEW(type, name, ...) \
    ({ \
        struct { WuBuObjectHeader _header; type inst; } *obj = calloc(1, sizeof(*obj)); \
        obj->_header.type_name = #type; \
        obj->_header.fini = (WuBuObjectFini)type##_fini; \
        type##_init(&obj->inst, ##__VA_ARGS__); \
        &obj->inst; \
    })

#define WUBU_OBJECT_FINI(obj) \
    do { \
        WuBuObjectHeader *hdr = (WuBuObjectHeader *)((char *)(obj) - offsetof(typeof(*hdr), _header)); \
        if (hdr->fini) hdr->fini(obj); \
        free(hdr); \
    } while (0)

/* ================================================================
 * Core Types
 * ================================================================ */

typedef struct WuBuWindow WuBuWindow;
typedef struct WuBuOutput WuBuOutput;
typedef struct WuBuCompositor WuBuCompositor;

struct WuBuWindow {
    WUBU_OBJECT_HEADER

    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_surface *surface;
    struct wlr_buffer *buffer;

    struct {
        double x, y;
        double width, height;
        double scale;        /* fractional scale */
        float opacity;       /* 0-1 */
        float transform[9];  /* 3x3 matrix */
    } geometry;

    struct wl_list link;     /* Z-order in compositor.windows */
    struct wl_listener surface_commit;
    struct wl_listener surface_destroy;
    struct wl_listener xdg_toplevel_map;
    struct wl_listener xdg_toplevel_unmap;
    struct wl_listener xdg_toplevel_destroy;
};

struct WuBuOutput {
    WUBU_OBJECT_HEADER

    struct wlr_output *wlr_output;
    struct wl_list link;     /* in compositor.outputs */
    struct wl_listener frame;
    struct wl_listener destroy;
};

struct WuBuCompositor {
    WUBU_OBJECT_HEADER

    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;

    struct wlr_compositor *compositor;
    struct wlr_xdg_shell *xdg_shell;
    struct wlr_xdg_decoration_manager_v1 *decoration_mgr;
    struct wlr_fractional_scale_manager_v1 *scale_mgr;
    struct wlr_linux_dmabuf_v1 *dmabuf;
    struct wlr_presentation *presentation;
    struct wlr_output_layout *output_layout;
    struct wlr_seat *seat;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *xcursor_mgr;
    struct wlr_data_device_manager *data_device_mgr;
    struct wlr_primary_selection_v1_device_manager *primary_sel_mgr;
    struct wlr_layer_shell_v1 *layer_shell;
    struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_mgr;

    struct wl_list windows;        /* WuBuWindow - Z-order */
    struct wl_list outputs;        /* WuBuOutput */
    struct wl_listener new_output;
    struct wl_listener new_xdg_toplevel;
    struct wl_listener new_input;

    /* Damage tracking */
    pixman_region32_t damage;

    /* Frame timing */
    uint64_t last_frame_ns;
    uint64_t frame_interval_ns;    /* 16666667 for 60Hz */

    /* VSL integration */
    VSL_DRV *gpu_driver;
    VkInstance vk_instance;
    VkDevice vk_device;
};

/* ================================================================
 * Window Implementation
 * ================================================================ */

static void window_init(WuBuWindow *win, struct wlr_xdg_toplevel *toplevel) {
    win->xdg_toplevel = toplevel;
    win->surface = toplevel->base->surface;
    win->geometry.x = 0;
    win->geometry.y = 0;
    win->geometry.width = 800;
    win->geometry.height = 600;
    win->geometry.scale = 1.0;
    win->geometry.opacity = 1.0;
    memset(win->geometry.transform, 0, sizeof(win->geometry.transform));
    win->geometry.transform[0] = win->geometry.transform[4] = win->geometry.transform[8] = 1.0;

    wl_signal_add(&toplevel->base->surface->events.commit, &win->surface_commit);
    win->surface_commit.notify = window_handle_commit;

    wl_signal_add(&toplevel->base->surface->events.destroy, &win->surface_destroy);
    win->surface_destroy.notify = window_handle_destroy;

    wl_signal_add(&toplevel->events.map, &win->xdg_toplevel_map);
    win->xdg_toplevel_map.notify = window_handle_map;

    wl_signal_add(&toplevel->events.unmap, &win->xdg_toplevel_unmap);
    win->xdg_toplevel_unmap.notify = window_handle_unmap;

    wl_signal_add(&toplevel->events.destroy, &win->xdg_toplevel_destroy);
    win->xdg_toplevel_destroy.notify = window_handle_destroy;
}

static void window_fini(WuBuWindow *win) {
    wl_list_remove(&win->surface_commit.link);
    wl_list_remove(&win->surface_destroy.link);
    wl_list_remove(&win->xdg_toplevel_map.link);
    wl_list_remove(&win->xdg_toplevel_unmap.link);
    wl_list_remove(&win->xdg_toplevel_destroy.link);
    wl_list_remove(&win->link);
}

static void window_handle_commit(struct wl_listener *listener, void *data) {
    WuBuWindow *win = wl_container_of(listener, win, surface_commit);
    /* Surface committed - new buffer available */
    struct wlr_buffer *buffer = wlr_surface_get_buffer(win->surface);
    if (buffer) {
        win->buffer = buffer;
        /* Damage whole window for now - optimize later */
        pixman_region32_t damage;
        pixman_region32_init_rect(&damage, 0, 0, win->geometry.width, win->geometry.height);
        /* Add to compositor damage */
        WuBuCompositor *comp = wubu_window_get_compositor(win);
        if (comp) {
            pixman_region32_union(&comp->damage, &comp->damage, &damage);
        }
        pixman_region32_fini(&damage);
    }
}

static void window_handle_destroy(struct wl_listener *listener, void *data) {
    WuBuWindow *win = wl_container_of(listener, win, surface_destroy);
    WUBU_OBJECT_FINI(win);
}

static void window_handle_map(struct wl_listener *listener, void *data) {
    WuBuWindow *win = wl_container_of(listener, win, xdg_toplevel_map);
    WuBuCompositor *comp = wubu_window_get_compositor(win);
    if (comp) {
        /* Add to top of Z-order */
        wl_list_insert(&comp->windows, &win->link);
        /* Focus this window */
        wlr_seat_keyboard_notify_enter(comp->seat, win->surface, NULL, 0);
    }
}

static void window_handle_unmap(struct wl_listener *listener, void *data) {
    WuBuWindow *win = wl_container_of(listener, win, xdg_toplevel_unmap);
    wl_list_remove(&win->link);
}

static void window_handle_destroy(struct wl_listener *listener, void *data) {
    WuBuWindow *win = wl_container_of(listener, win, xdg_toplevel_destroy);
    WUBU_OBJECT_FINI(win);
}

/* ================================================================
 * Output Implementation
 * ================================================================ */

static void output_init(WuBuOutput *out, struct wlr_output *wlr_output) {
    out->wlr_output = wlr_output;
    wlr_output_init_render(wlr_output, wlr_output->compositor->renderer, NULL);

    wl_signal_add(&wlr_output->events.frame, &out->frame);
    out->frame.notify = output_handle_frame;

    wl_signal_add(&wlr_output->events.destroy, &out->destroy);
    out->destroy.notify = output_handle_destroy;

    wlr_output_enable(wlr_output, true);
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_mode(&state, wlr_output_preferred_mode(wlr_output));
    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);
}

static void output_fini(WuBuOutput *out) {
    wl_list_remove(&out->frame.link);
    wl_list_remove(&out->destroy.link);
    wl_list_remove(&out->link);
}

static void output_handle_frame(struct wl_listener *listener, void *data) {
    WuBuOutput *out = wl_container_of(listener, out, frame);
    WuBuCompositor *comp = wubu_output_get_compositor(out);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t now_ns = now.tv_sec * 1000000000ULL + now.tv_nsec;

    /* Throttle to frame interval */
    if (comp->last_frame_ns > 0) {
        uint64_t elapsed = now_ns - comp->last_frame_ns;
        if (elapsed < comp->frame_interval_ns) {
            return; /* Skip frame */
        }
    }
    comp->last_frame_ns = now_ns;

    /* Render all outputs */
    wubu_compositor_render(comp);
}

static void output_handle_destroy(struct wl_listener *listener, void *data) {
    WuBuOutput *out = wl_container_of(listener, out, destroy);
    WUBU_OBJECT_FINI(out);
}

/* ================================================================
 * Compositor Implementation
 * ================================================================ */

static void compositor_init(WuBuCompositor *comp) {
    wl_list_init(&comp->windows);
    wl_list_init(&comp->outputs);
    pixman_region32_init(&comp->damage);
    comp->frame_interval_ns = 16666667; /* 60Hz */
    comp->last_frame_ns = 0;

    /* Wayland display */
    comp->display = wl_display_create();
    if (!comp->display) {
        fprintf(stderr, "Failed to create Wayland display\n");
        abort();
    }

    /* DRM/KMS backend via wlroots */
    comp->backend = wlr_backend_autocreate(comp->display, NULL);
    if (!comp->backend) {
        fprintf(stderr, "Failed to create wlroots backend\n");
        abort();
    }

    /* Vulkan renderer via VSL */
    comp->renderer = wlr_vulkan_renderer_create(comp->display, NULL);
    if (!comp->renderer) {
        fprintf(stderr, "Failed to create Vulkan renderer\n");
        /* Fallback to GLES2 if Vulkan unavailable */
        comp->renderer = wlr_gles2_renderer_create(comp->display, NULL);
        if (!comp->renderer) {
            fprintf(stderr, "Failed to create any renderer\n");
            abort();
        }
    }

    comp->allocator = wlr_allocator_autocreate(comp->backend, comp->renderer);
    if (!comp->allocator) {
        fprintf(stderr, "Failed to create allocator\n");
        abort();
    }

    wlr_renderer_init_wl_display(comp->renderer, comp->display);

    /* Core Wayland protocols */
    comp->compositor = wlr_compositor_create(comp->display, 5, comp->renderer);
    comp->xdg_shell = wlr_xdg_shell_create(comp->display, 3);
    comp->decoration_mgr = wlr_xdg_decoration_manager_v1_create(comp->display);
    comp->scale_mgr = wlr_fractional_scale_manager_v1_create(comp->display);
    comp->dmabuf = wlr_linux_dmabuf_v1_create(comp->display, comp->renderer);
    comp->presentation = wlr_presentation_create(comp->display, comp->backend);
    comp->output_layout = wlr_output_layout_create(comp->display);
    comp->seat = wlr_seat_create(comp->display, "default");
    comp->cursor = wlr_cursor_create();
    comp->xcursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    comp->data_device_mgr = wlr_data_device_manager_create(comp->display);
    comp->primary_sel_mgr = wlr_primary_selection_v1_device_manager_create(comp->display);
    comp->layer_shell = wlr_layer_shell_v1_create(comp->display, 4);
    comp->foreign_toplevel_mgr = wlr_foreign_toplevel_management_v1_create(comp->display);

    /* Set up listeners */
    wl_signal_add(&comp->backend->events.new_output, &comp->new_output);
    comp->new_output.notify = compositor_handle_new_output;

    wl_signal_add(&comp->xdg_shell->events.new_toplevel, &comp->new_xdg_toplevel);
    comp->new_xdg_toplevel.notify = compositor_handle_new_xdg_toplevel;

    wl_signal_add(&comp->backend->events.new_input, &comp->new_input);
    comp->new_input.notify = compositor_handle_new_input;

    /* Start backend */
    if (!wlr_backend_start(comp->backend)) {
        fprintf(stderr, "Failed to start backend\n");
        abort();
    }
}

static void compositor_fini(WuBuCompositor *comp) {
    pixman_region32_fini(&comp->damage);
    wlr_backend_destroy(comp->backend);
    wl_display_destroy(comp->display);
}

static void compositor_handle_new_output(struct wl_listener *listener, void *data) {
    WuBuCompositor *comp = wl_container_of(listener, comp, new_output);
    struct wlr_output *wlr_output = data;

    WuBuOutput *out = WUBU_OBJECT_NEW(WuBuOutput, out, wlr_output);
    wl_list_insert(&comp->outputs, &out->link);
    wlr_output_layout_add_auto(comp->output_layout, wlr_output);
}

static void compositor_handle_new_xdg_toplevel(struct wl_listener *listener, void *data) {
    WuBuCompositor *comp = wl_container_of(listener, comp, new_xdg_toplevel);
    struct wlr_xdg_toplevel *toplevel = data;

    WuBuWindow *win = WUBU_OBJECT_NEW(WuBuWindow, win, toplevel);
    /* Window added to Z-order on map event */
}

static void compositor_handle_new_input(struct wl_listener *listener, void *data) {
    WuBuCompositor *comp = wl_container_of(listener, comp, new_input);
    struct wlr_input_device *device = data;

    switch (device->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            wlr_seat_set_keyboard(comp->seat, device);
            break;
        case WLR_INPUT_DEVICE_POINTER:
            wlr_cursor_attach_input_device(comp->cursor, device);
            break;
        default:
            break;
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    struct wlr_input_device *kb = wlr_seat_get_keyboard(comp->seat);
    if (kb) caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities(comp->seat, caps);
}

static void compositor_render(WuBuCompositor *comp) {
    struct wlr_output *wlr_output;
    wl_list_for_each(wlr_output, &comp->outputs, link) {
        WuBuOutput *out = wl_container_of(wlr_output, out, wlr_output);
        struct wlr_output_state state;
        wlr_output_state_init(&state);

        /* Begin frame */
        if (!wlr_output_begin_render_pass(wlr_output, NULL)) {
            wlr_output_state_finish(&state);
            continue;
        }

        struct wlr_renderer *renderer = wlr_output->compositor->renderer;
        wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

        /* Clear */
        float color[4] = {0.1f, 0.1f, 0.1f, 1.0f};
        wlr_renderer_clear(renderer, color);

        /* Render windows in Z-order (bottom to top) */
        WuBuWindow *win;
        wl_list_for_each(win, &comp->windows, link) {
            if (!win->buffer) continue;

            struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(renderer, win->buffer, NULL);
            if (pass) {
                /* Apply transform matrix */
                struct wlr_box box = {
                    .x = win->geometry.x,
                    .y = win->geometry.y,
                    .width = win->geometry.width,
                    .height = win->geometry.height,
                };
                wlr_render_pass_add_texture(pass, &wlr_texture, &box, win->geometry.transform);
                wlr_render_pass_submit(pass);
            }
        }

        wlr_renderer_end(renderer);
        wlr_output_end_render_pass(wlr_output);
        wlr_output_state_finish(&state);
    }

    /* Send presentation feedback */
    wlr_presentation_destroy(comp->presentation); /* TODO: proper feedback */

    /* Clear damage */
    pixman_region32_clear(&comp->damage);
}

/* ================================================================
 * VSL GPU Driver Integration
 * ================================================================ */

static bool compositor_init_gpu(WuBuCompositor *comp) {
    /* Get VSL GPU driver (Vulkan) */
    comp->gpu_driver = vsl_get_driver(VSL_DRV_GPU_VULKAN);
    if (!comp->gpu_driver || !comp->gpu_driver->active) {
        fprintf(stderr, "Vulkan driver not active\n");
        return false;
    }

    comp->vk_instance = comp->gpu_driver->priv;
    /* VkDevice would be stored in driver private data */

    /* Configure wlroots Vulkan renderer with our instance/device */
    struct wlr_vulkan_renderer_options opts = {
        .instance = comp->vk_instance,
        .physical_device = VK_NULL_HANDLE, /* TODO: select */
        .device = VK_NULL_HANDLE,          /* TODO: get from driver */
    };

    /* Re-create renderer with our Vulkan objects */
    wlr_renderer_destroy(comp->renderer);
    comp->renderer = wlr_vulkan_renderer_create_with_options(comp->display, &opts);
    if (!comp->renderer) {
        fprintf(stderr, "Failed to create Vulkan renderer with VSL device\n");
        return false;
    }

    wlr_renderer_init_wl_display(comp->renderer, comp->display);
    return true;
}

/* ================================================================
 * Public API
 * ================================================================ */

WuBuCompositor *wubu_compositor_create(void) {
    WuBuCompositor *comp = WUBU_OBJECT_NEW(WuBuCompositor, comp);
    compositor_init(comp);

    /* Initialize GPU via VSL */
    if (!compositor_init_gpu(comp)) {
        fprintf(stderr, "GPU init failed, continuing with software fallback\n");
    }

    return comp;
}

void wubu_compositor_destroy(WuBuCompositor *comp) {
    WUBU_OBJECT_FINI(comp);
}

int wubu_compositor_run(WuBuCompositor *comp) {
    const char *socket = wl_display_add_socket_auto(comp->display);
    if (!socket) {
        fprintf(stderr, "Failed to create Wayland socket\n");
        return -1;
    }
    setenv("WAYLAND_DISPLAY", socket, true);

    fprintf(stderr, "WuBuOS Compositor running on wayland-%s\n", socket);
    wl_display_run(comp->display);
    return 0;
}

WuBuWindow *wubu_window_get_compositor(WuBuWindow *win) {
    /* Walk up to compositor via surface -> display -> user_data */
    struct wl_display *display = win->surface->resource->client->display;
    return wl_display_get_user_data(display);
}

WuBuOutput *wubu_output_get_compositor(WuBuOutput *out) {
    return wl_container_of(out->wlr_output->compositor, NULL, compositor);
}

/* ================================================================
 * 9P IPC Integration (for shell communication)
 * ================================================================ */

static void compositor_9p_serve(WuBuCompositor *comp) {
    /* Export compositor capabilities via 9P:
     * /mnt/compositor/windows     - list of window IDs
     * /mnt/compositor/outputs     - list of outputs
     * /mnt/compositor/screenshots - screenshot capability
     * /mnt/compositor/clipboard   - clipboard (shared with shell)
     */
    /* TODO: Implement 9P server in separate thread */
}

/* ================================================================
 * Entry Point
 * ================================================================ */

int main(int argc, char *argv[]) {
    wlr_log_init(WLR_DEBUG, NULL);

    /* Initialize VSL first */
    if (vsl_init() != 0) {
        fprintf(stderr, "VSL init failed\n");
        return 1;
    }

    /* Activate GPU drivers */
    int vk_drv = vsl_register_driver(VSL_DRV_GPU_VULKAN, 0, 0, 0, 0);
    if (vk_drv >= 0) vsl_activate_driver(vk_drv);

    WuBuCompositor *comp = wubu_compositor_create();
    int ret = wubu_compositor_run(comp);
    wubu_compositor_destroy(comp);

    vsl_shutdown();
    return ret;
}