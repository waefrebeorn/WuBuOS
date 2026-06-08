/*
 * wubu_display.h — WuBuOS Display Backend (DRM/KMS — X11-free)
 *
 * Roadmap: Replace X11 with Linux DRM/KMS for direct mode setting.
 * This is the SteamOS path — game engines use DRM directly.
 * Zero X11 dependency. Raw kernel mode setting.
 *
 * Status: HEADER ONLY — Cell 380 in roadmap.
 * Implementation requires: libdrm, kernel DRM driver.
 */
#ifndef WUBU_DISPLAY_H
#define WUBU_DISPLAY_H

#include <stdint.h>
#include <stddef.h>

/* ── DRM/KMS Display State ──────────────────────────────────────── */

typedef struct {
    int      drm_fd;           /* /dev/dri/card0 fd */
    uint32_t connector_id;
    uint32_t crtc_id;
    uint32_t mode_blob_id;
    
    /* Framebuffer */
    uint32_t fb_id;
    uint32_t *fb_map;         /* mmap'd framebuffer */
    size_t   fb_size;
    int      fb_w, fb_h;
    int      fb_pitch;        /* bytes per row */
    
    /* GBM (for buffer swapping) */
    void    *gbm_device;      /* struct gbm_device* */
    void    *gbm_surface;     /* struct gbm_surface* */
    void    *gbm_bo;          /* struct gbm_bo* (current back buffer) */
    
    /* Input (evdev) */
    int      kbd_fd;          /* /dev/input/eventX for keyboard */
    int      mouse_fd;        /* /dev/input/eventX for mouse */
    
    /* State */
    int      running;
} WubuDisplay;

/* ── API ────────────────────────────────────────────────────────── */

/* Open DRM device, find connector+CRTC, set mode */
int  wubu_display_init(WubuDisplay *d, int width, int height);

/* Swap buffers (DRM page flip) */
void wubu_display_swap(WubuDisplay *d);

/* Read input events from evdev */
int  wubu_display_poll_input(WubuDisplay *d);

/* Cleanup */
void wubu_display_shutdown(WubuDisplay *d);

/* ── Fallback: if no DRM available, use X11 ────────────────────── */
/* The hosted binary will try DRM first, fall back to X11.
 * This keeps WuBuOS working on systems without DRM (WSL, etc.) */

#endif /* WUBU_DISPLAY_H */
