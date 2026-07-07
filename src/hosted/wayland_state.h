/*
 * wayland_state.h  --  Wayland client connection state (type only).
 *
 * Minimal, self-contained type definition for the global Wayland
 * connection state.  It forward-declares the libwayland / xdg-shell /
 * wlroots protocol structs so it does NOT pull in <wayland-client.h>
 * (avoids dragging Wayland protocol headers + macros into every
 * translation unit that merely needs the struct layout, e.g. the weak
 * default provider in src/gui).
 *
 * The real instance is defined in src/hosted/hosted.c; standalone GUI
 * binaries link a weak zeroed default (src/gui/wubu_wayland_stub.c) and
 * degrade gracefully when no Wayland connection exists.
 */

#ifndef WUBU_WAYLAND_STATE_H
#define WUBU_WAYLAND_STATE_H

#include <stdint.h>

/* Forward declarations — full definitions come from <wayland-client.h>,
 * <xdg-shell-client-protocol.h>, etc. only in the hosted binary that
 * actually opens the connection. */
struct wl_display;
struct wl_compositor;
struct wl_surface;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct wl_keyboard;
struct wl_pointer;
struct wl_seat;
struct wl_shm;
struct wl_shm_pool;
struct wl_buffer;
struct wl_data_device_manager;
struct wl_data_device;
struct wl_data_offer;
struct wl_data_source;
struct wl_output;
struct wl_touch;
struct zwp_primary_selection_device_manager_v1;
struct zwp_tablet_manager_v1;
struct wl_registry;

typedef struct {
    struct wl_display    *display;
    struct wl_compositor *compositor;
    struct xdg_wm_base   *xdg_wm_base;
    struct wl_surface    *surface;
    struct xdg_surface   *xdg_surface;
    struct xdg_toplevel  *xdg_toplevel;
    struct wl_keyboard   *keyboard;
    struct wl_pointer     *pointer;
    struct wl_seat       *seat;
    struct wl_shm         *shm;
    struct wl_data_device_manager *data_device_manager;
    struct wl_data_device *data_device;
    struct zwp_primary_selection_device_manager_v1 *primary_selection_manager;
    struct zwp_tablet_manager_v1 *tablet_manager;
    struct wl_output      *output;
    struct wl_touch        *touch;
    int                   drm_fd;
    int                   width;
    int                   height;
    struct wl_buffer      *shm_buffer;
    void                  *shm_data;
} wayland_state_t;

/* Global Wayland connection state.  Defined (strong) in hosted.c, or as a
 * weak zeroed default in src/gui/wubu_wayland_stub.c for standalone builds. */
extern wayland_state_t g_wl;

#endif /* WUBU_WAYLAND_STATE_H */
