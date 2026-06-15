/*
 * wubu_display.h  --  WuBuOS Display Backend (DRM/KMS  --  X11-free)
 *
 * Roadmap: Replace X11 with Linux DRM/KMS for direct mode setting.
 * This is the SteamOS path  --  game engines use DRM directly.
 * Zero X11 dependency. Raw kernel mode setting.
 *
 * Status: Direct DRM ioctls + custom GBM (Cells 388/389)
 * Implementation: wubu_drm_direct.c  --  no libdrm, no libgbm
 */
#ifndef WUBU_DISPLAY_H
#define WUBU_DISPLAY_H

#include <stdint.h>
#include <stddef.h>

/* ===================================================================
 * CUSTOM GBM TYPES (Cell 389)
 * =================================================================== */

typedef struct wubu_gbm_device {
    int fd;
} wubu_gbm_device_t;

typedef struct wubu_gbm_bo {
    uint32_t handle;
    uint32_t stride;
    void *map;
    size_t size;
} wubu_gbm_bo_t;

wubu_gbm_device_t *wubu_gbm_create_device(int fd);
void wubu_gbm_destroy_device(wubu_gbm_device_t *gbm);
wubu_gbm_bo_t *wubu_gbm_bo_create(wubu_gbm_device_t *gbm, uint32_t width, uint32_t height, uint32_t format);
void wubu_gbm_bo_destroy(wubu_gbm_device_t *gbm, wubu_gbm_bo_t *bo);
void *wubu_gbm_bo_get_map(wubu_gbm_bo_t *bo);
uint32_t wubu_gbm_bo_get_stride(wubu_gbm_bo_t *bo);
uint32_t wubu_gbm_bo_get_handle(wubu_gbm_bo_t *bo);

/* ===================================================================
 * DRM/KMS Display State
 * =================================================================== */

typedef struct {
    int      drm_fd;            /* /dev/dri/card0 fd */
    uint32_t connector_id;
    uint32_t crtc_id;
    uint32_t mode_blob_id;

    /* Framebuffer (dumb buffer) */
    uint32_t fb_id;
    uint32_t *fb_map;          /* mmap'd framebuffer */
    size_t   fb_size;
    int      fb_w, fb_h;
    int      fb_pitch;         /* bytes per row */

    /* GBM (for buffer swapping) */
    wubu_gbm_device_t *gbm_device;
    void    *gbm_surface;      /* reserved for future */
    wubu_gbm_bo_t *gbm_bo;     /* current back buffer */

    /* Input (evdev) */
    int      kbd_fd;           /* /dev/input/eventX for keyboard */
    int      mouse_fd;         /* /dev/input/eventX for mouse */

    /* State */
    int      running;
} WubuDisplay;

/* ===================================================================
 * API
 * =================================================================== */

/* Open DRM device, find connector+CRTC, set mode */
int  wubu_display_init(WubuDisplay *d, int width, int height);

/* Swap buffers (DRM page flip) */
void wubu_display_swap(WubuDisplay *d);

/* Read input events from evdev */
int  wubu_display_poll_input(WubuDisplay *d);

/* Cleanup */
void wubu_display_shutdown(WubuDisplay *d);

/* ===================================================================
 * Fallback: if no DRM available, use X11
 * The hosted binary will try DRM first, fall back to X11.
 * This keeps WuBuOS working on systems without DRM (WSL, etc.)
 * =================================================================== */

#endif /* WUBU_DISPLAY_H */
