/*
 * wubu_display.c — WuBuOS Display Backend (DRM/KMS + X11 dual)
 *
 * Cell 380: Try DRM/KMS first, fall back to X11.
 *
 * Pattern: SteamOS uses DRM/KMS directly. We do the same.
 * Fallback: WSL2, nested X11, dev environments where DRM isn't available.
 *
 * DRM path:  /dev/dri/card0 → drmModeSetCRTC → page flip
 * X11 path:  XOpenDisplay → XCreateWindow → XPutImage
 * evdev path: /dev/input/eventX → struct input_event → key/mouse
 *
 * The VBE framebuffer is the RENDER TARGET for both paths.
 * WuBuOS GUI renders to VBE back buffer. This module displays it.
 */

#include "wubu_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* ── Backend Detection ──────────────────────────────────────────── */

static int probe_drm_available(void) {
    /* Check if /dev/dri/card0 exists and is accessible */
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
        close(fd);
        return 1;
    }
    return 0;
}

static int probe_evdev_available(void) {
    /* Check if keyboard evdev exists */
    for (int i = 0; i < 10; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            close(fd);
            return 1;
        }
    }
    return 0;
}

/* ── DRM/KMS Backend ────────────────────────────────────────────── */

#ifdef WUBU_USE_DRM
#include <xf86drm.h>
#include <xf86drmMode.h>

static int drm_init(WubuDisplay *d, int width, int height) {
    d->drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (d->drm_fd < 0) {
        fprintf(stderr, "wubu_display: DRM open failed: %s\n", strerror(errno));
        return -1;
    }

    /* Check if we can become DRM master */
    if (drmSetMaster(d->drm_fd) != 0) {
        fprintf(stderr, "wubu_display: drmSetMaster failed: %s\n", strerror(errno));
        fprintf(stderr, "wubu_display: Another compositor is DRM master. Try VT switch or --x11.\n");
        close(d->drm_fd);
        return -1;
    }

    /* Find first connected connector with a CRTC */
    drmModeRes *res = drmModeGetResources(d->drm_fd);
    if (!res) {
        fprintf(stderr, "wubu_display: drmModeGetResources failed\n");
        close(d->drm_fd);
        return -1;
    }

    int found = 0;
    for (int i = 0; i < res->count_connectors && !found; i++) {
        drmModeConnector *conn = drmModeGetConnector(d->drm_fd, res->connectors[i]);
        if (!conn) continue;

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            d->connector_id = conn->connector_id;

            /* Find matching CRTC */
            for (int j = 0; j < res->count_crtcs; j++) {
                if (conn->encoder_id) {
                    drmModeEncoder *enc = drmModeGetEncoder(d->drm_fd, conn->encoder_id);
                    if (enc && (enc->possible_crtcs & (1 << j))) {
                        d->crtc_id = res->crtcs[j];
                        drmModeFreeEncoder(enc);
                        break;
                    }
                    if (enc) drmModeFreeEncoder(enc);
                }
            }

            /* Pick mode closest to requested resolution */
            drmModeModeInfo *best = &conn->modes[0];
            for (int m = 0; m < conn->count_modes; m++) {
                drmModeModeInfo *mode = &conn->modes[m];
                if (mode->hdisplay == width && mode->vdisplay == height) {
                    best = mode;
                    break;
                }
                /* Prefer closer match */
                int diff_best = abs(best->hdisplay - width) + abs(best->vdisplay - height);
                int diff_cur = abs(mode->hdisplay - width) + abs(mode->vdisplay - height);
                if (diff_cur < diff_best) best = mode;
            }

            /* Create dumb framebuffer */
            uint32_t fb;
            int ret = drmModeCreateDumbBuffer(d->drm_fd, best->hdisplay,
                                               best->vdisplay, 32, 32, &fb);
            if (ret == 0) {
                d->fb_id = fb;
                d->fb_w = best->hdisplay;
                d->fb_h = best->vdisplay;

                /* Set CRTC to display our framebuffer */
                drmModeSetCrtc(d->drm_fd, d->crtc_id, d->fb_id,
                               0, 0, &d->connector_id, 1, best);
            }

            found = 1;
        }
        drmModeFreeConnector(conn);
    }
    drmModeFreeResources(res);

    if (!found) {
        fprintf(stderr, "wubu_display: No connected DRM connector found\n");
        close(d->drm_fd);
        return -1;
    }

    fprintf(stderr, "wubu_display: DRM/KMS %dx%d initialized\n", d->fb_w, d->fb_h);
    return 0;
}

static void drm_swap(WubuDisplay *d) {
    /* DRM page flip — non-blocking */
    if (d->drm_fd >= 0 && d->crtc_id > 0 && d->fb_id > 0) {
        drmModePageFlip(d->drm_fd, d->crtc_id, d->fb_id,
                        DRM_MODE_PAGE_FLIP_EVENT, NULL);
    }
}

static void drm_shutdown(WubuDisplay *d) {
    if (d->drm_fd >= 0) {
        /* Restore original CRTC mode? For now, just drop master */
        drmDropMaster(d->drm_fd);
        close(d->drm_fd);
        d->drm_fd = -1;
    }
}
#endif /* WUBU_USE_DRM */

/* ── evdev Input Backend ─────────────────────────────────────────── */

#ifdef WUBU_USE_EVDEV
#include <linux/input.h>

static int evdev_open_keyboard(void) {
    /* Try common keyboard event devices */
    for (int i = 0; i < 10; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        /* Check if this device is a keyboard */
        unsigned long evbits = 0;
        ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits);
        if (evbits & (1 << EV_KEY)) {
            return fd;
        }
        close(fd);
    }
    return -1;
}

static int evdev_open_mouse(void) {
    for (int i = 0; i < 10; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        unsigned long evbits = 0;
        ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits);
        /* Mouse has EV_REL (relative movement) */
        if (evbits & (1 << EV_REL)) {
            return fd;
        }
        close(fd);
    }
    return -1;
}
#endif /* WUBU_USE_EVDEV */

/* ── Public API ─────────────────────────────────────────────────── */

int wubu_display_init(WubuDisplay *d, int width, int height) {
    memset(d, 0, sizeof(*d));

    /* Strategy: try DRM/KMS first, fall back gracefully */
    int have_drm = probe_drm_available();
    int have_evdev = probe_evdev_available();

    fprintf(stderr, "wubu_display: probing backends...\n");
    fprintf(stderr, "  DRM/KMS:  %s\n", have_drm ? "available" : "not found");
    fprintf(stderr, "  evdev:    %s\n", have_evdev ? "available" : "not found");

#ifdef WUBU_USE_DRM
    if (have_drm) {
        if (drm_init(d, width, height) == 0) {
            fprintf(stderr, "wubu_display: using DRM/KMS backend\n");

#ifdef WUBU_USE_EVDEV
            if (have_evdev) {
                d->kbd_fd = evdev_open_keyboard();
                d->mouse_fd = evdev_open_mouse();
                if (d->kbd_fd >= 0)
                    fprintf(stderr, "  keyboard: evdev (fd=%d)\n", d->kbd_fd);
                if (d->mouse_fd >= 0)
                    fprintf(stderr, "  mouse: evdev (fd=%d)\n", d->mouse_fd);
            }
#endif
            return 0;
        }
        /* DRM init failed — fall through to X11 */
        fprintf(stderr, "wubu_display: DRM init failed, falling back to X11\n");
    }
#endif

    /* X11 fallback — hosted.c uses X11 directly, this just signals the caller */
    fprintf(stderr, "wubu_display: using X11 backend (via hosted.c)\n");
    fprintf(stderr, "  (No DRM/KMS — X11 managed by hosted.c event loop)\n");
    return 0;  /* X11 init handled by hosted.c */
}

void wubu_display_swap(WubuDisplay *d) {
#ifdef WUBU_USE_DRM
    if (d->drm_fd >= 0) {
        drm_swap(d);
        return;
    }
#endif
    /* X11 swap handled by hosted.c via XPutImage */
}

int wubu_display_poll_input(WubuDisplay *d) {
    int events = 0;

#ifdef WUBU_USE_EVDEV
    if (d->kbd_fd >= 0) {
        struct input_event ev;
        while (read(d->kbd_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_KEY) {
                /* Key event: ev.code = key code, ev.value = 0(up) 1(down) 2(repeat) */
                events++;
            }
        }
    }
    if (d->mouse_fd >= 0) {
        struct input_event ev;
        while (read(d->mouse_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_REL || ev.type == EV_KEY) {
                /* Relative movement or button */
                events++;
            }
        }
    }
#endif

    return events;
}

void wubu_display_shutdown(WubuDisplay *d) {
#ifdef WUBU_USE_DRM
    drm_shutdown(d);
#endif

    if (d->kbd_fd >= 0) { close(d->kbd_fd); d->kbd_fd = -1; }
    if (d->mouse_fd >= 0) { close(d->mouse_fd); d->mouse_fd = -1; }

    if (d->fb_map) {
        /* munmap handled by caller or DRM cleanup */
        d->fb_map = NULL;
    }
}
