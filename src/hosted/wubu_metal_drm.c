/*
 * wubu_metal_drm.c -- WuBuOS Metal DRM/KMS display backend (split from
 * wubu_metal.c).
 *
 * Self-contained: the bare-metal KMS display engine -- atomic-commit
 * helpers, connector/CRTC/plane property enumeration, dumb-fb creation,
 * and the public wubu_drm_init/shutdown/flip/set_mode/get_modes entry
 * points. Dispatched by wubu_disp_* in wubu_metal.c.
 *
 * C11 opaque-struct pattern: backend fns declared non-static in
 * wubu_metal_drm.h; the only shared state is the extern WubuDisplay
 * g_display the facade owns. libdrm types are confined to this TU behind
 * WUBU_USE_DRM -- no god header, no header leakage.
 */

#include "wubu_metal_drm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* The facade owns the display state; this backend only writes into it. */
extern WubuDisplay g_display;

/* ------------------------------------------------------------------
 *  DRM ATOMIC-COMMIT HELPERS (internal TU linkage)
 * ------------------------------------------------------------------ */

#ifdef WUBU_USE_DRM
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

/* ------------------------------------------------------------------
 *  DRM ATOMIC-COMMIT HELPERS (internal TU linkage)
 *
 *  The legacy SetCrtc path is used for dumb-buffer presentation; full
 *  atomic modesetting would enumerate property objects via the helpers
 *  below. libdrm owns the ioctl structs, so we do NOT redefine them.
 * ------------------------------------------------------------------ */

static int drm_atomic_commit(int fd, uint32_t crtc_id, uint32_t fb_id, uint32_t connector_id,
                             drmModeModeInfo *mode, int x, int y) {
    /* legacy SetCrtc - atomic requires property enumeration first */
    return drmModeSetCrtc(fd, crtc_id, fb_id, x, y, &connector_id, 1, mode);
}

static int drm_get_connector_props(int fd, uint32_t connector_id,
                                   uint32_t **out_props, uint64_t **out_values, int *out_count) {
    drmModeObjectProperties *obj_props = drmModeObjectGetProperties(fd, connector_id, DRM_MODE_OBJECT_CONNECTOR);
    if (!obj_props) return -1;

    *out_count = obj_props->count_props;
    *out_props = malloc(obj_props->count_props * sizeof(uint32_t));
    *out_values = malloc(obj_props->count_props * sizeof(uint64_t));

    if (!*out_props || !*out_values) {
        free(*out_props);
        free(*out_values);
        drmModeFreeObjectProperties(obj_props);
        return -1;
    }

    memcpy(*out_props, obj_props->props, obj_props->count_props * sizeof(uint32_t));
    memcpy(*out_values, obj_props->prop_values, obj_props->count_props * sizeof(uint64_t));

    drmModeFreeObjectProperties(obj_props);
    return 0;
}

static int drm_get_crtc_props(int fd, uint32_t crtc_id,
                              uint32_t **out_props, uint64_t **out_values, int *out_count) {
    drmModeObjectProperties *obj_props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
    if (!obj_props) return -1;

    *out_count = obj_props->count_props;
    *out_props = malloc(obj_props->count_props * sizeof(uint32_t));
    *out_values = malloc(obj_props->count_props * sizeof(uint64_t));

    if (!*out_props || !*out_values) {
        free(*out_props);
        free(*out_values);
        drmModeFreeObjectProperties(obj_props);
        return -1;
    }

    memcpy(*out_props, obj_props->props, obj_props->count_props * sizeof(uint32_t));
    memcpy(*out_values, obj_props->prop_values, obj_props->count_props * sizeof(uint64_t));

    drmModeFreeObjectProperties(obj_props);
    return 0;
}

static int drm_get_plane_props(int fd, uint32_t plane_id,
                               uint32_t **out_props, uint64_t **out_values, int *out_count) {
    drmModeObjectProperties *obj_props = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!obj_props) return -1;

    *out_count = obj_props->count_props;
    *out_props = malloc(obj_props->count_props * sizeof(uint32_t));
    *out_values = malloc(obj_props->count_props * sizeof(uint64_t));

    if (!*out_props || !*out_values) {
        free(*out_props);
        free(*out_values);
        drmModeFreeObjectProperties(obj_props);
        return -1;
    }

    memcpy(*out_props, obj_props->props, obj_props->count_props * sizeof(uint32_t));
    memcpy(*out_values, obj_props->prop_values, obj_props->count_props * sizeof(uint64_t));

    drmModeFreeObjectProperties(obj_props);
    return 0;
}
#else
/* Forward declare DRM types for when libdrm headers not available */
typedef struct _drmModeModeInfo drmModeModeInfo;
typedef struct _drmModeObjectProperties drmModeObjectProperties;

static int drm_atomic_commit(int fd, uint32_t crtc_id, uint32_t fb_id, uint32_t connector_id,
                             drmModeModeInfo *mode, int x, int y) {
    (void)fd; (void)crtc_id; (void)fb_id; (void)connector_id; (void)mode; (void)x; (void)y;
    return -1;
}
static int drm_get_connector_props(int fd, uint32_t connector_id,
                                   uint32_t **out_props, uint64_t **out_values, int *out_count) {
    (void)fd; (void)connector_id; (void)out_props; (void)out_values; (void)out_count;
    return -1;
}
static int drm_get_crtc_props(int fd, uint32_t crtc_id,
                              uint32_t **out_props, uint64_t **out_values, int *out_count) {
    (void)fd; (void)crtc_id; (void)out_props; (void)out_values; (void)out_count;
    return -1;
}
static int drm_get_plane_props(int fd, uint32_t plane_id,
                               uint32_t **out_props, uint64_t **out_values, int *out_count) {
    (void)fd; (void)plane_id; (void)out_props; (void)out_values; (void)out_count;
    return -1;
}
#endif

/* ------------------------------------------------------------------
 *  DRM/KMS DISPLAY BACKEND (BARE-METAL)
 * ------------------------------------------------------------------ */

#ifdef WUBU_USE_DRM
#include <xf86drm.h>
#include <xf86drmMode.h>

static int drm_find_connector(int fd, drmModeConnector **out_conn, drmModeEncoder **out_enc, drmModeCrtc **out_crtc) {
    drmModeRes *res = drmModeGetResources(fd);
    if (!res) return -1;

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) continue;

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            for (int j = 0; j < conn->count_encoders; j++) {
                drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[j]);
                if (!enc) continue;

                for (int k = 0; k < res->count_crtcs; k++) {
                    if (enc->possible_crtcs & (1 << k)) {
                        drmModeCrtc *crtc = drmModeGetCrtc(fd, res->crtcs[k]);
                        if (crtc) {
                            *out_conn = conn;
                            *out_enc = enc;
                            *out_crtc = crtc;
                            drmModeFreeResources(res);
                            return 0;
                        }
                    }
                }
                drmModeFreeEncoder(enc);
            }
        }
        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
    return -1;
}

static int drm_create_fb(int fd, int width, int height, uint32_t *fb_id, uint32_t **fb_map) {
    struct drm_mode_create_dumb create = {0};
    create.width = width;
    create.height = height;
    create.bpp = 32;
    create.flags = 0;

    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        return -1;
    }

    struct drm_mode_map_dumb map = {0};
    map.handle = create.handle;

    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        struct drm_mode_destroy_dumb destroy = { .handle = create.handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        return -1;
    }

    *fb_map = mmap(0, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);
    if (*fb_map == MAP_FAILED) {
        struct drm_mode_destroy_dumb destroy = { .handle = create.handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        return -1;
    }

    uint32_t handles[4] = { create.handle, 0, 0, 0 };
    uint32_t pitches[4] = { create.pitch, 0, 0, 0 };
    uint32_t offsets[4] = { 0, 0, 0, 0 };

    if (drmModeAddFB2(fd, width, height, DRM_FORMAT_XRGB8888, handles, pitches, offsets, fb_id, 0) < 0) {
        munmap(*fb_map, create.size);
        struct drm_mode_destroy_dumb destroy = { .handle = create.handle };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        return -1;
    }

    return 0;
}

int wubu_drm_init(int width, int height) {
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open /dev/dri/card0");
        return -1;
    }

    if (drmSetMaster(fd) < 0) {
        perror("drmSetMaster");
        close(fd);
        return -1;
    }

    drmModeConnector *conn = NULL;
    drmModeEncoder *enc = NULL;
    drmModeCrtc *crtc = NULL;

    if (drm_find_connector(fd, &conn, &enc, &crtc) < 0) {
        fprintf(stderr, "No suitable connector found\n");
        drmDropMaster(fd);
        close(fd);
        return -1;
    }

    drmModeModeInfo *mode = &conn->modes[0];
    if (width > 0 && height > 0) {
        mode->hdisplay = width;
        mode->vdisplay = height;
        mode->vrefresh = 60;
    }

    uint32_t fb_id = 0;
    uint32_t *fb_map = NULL;
    if (drm_create_fb(fd, mode->hdisplay, mode->vdisplay, &fb_id, &fb_map) < 0) {
        fprintf(stderr, "Failed to create framebuffer\n");
        drmModeFreeConnector(conn);
        drmModeFreeEncoder(enc);
        drmModeFreeCrtc(crtc);
        drmDropMaster(fd);
        close(fd);
        return -1;
    }

    if (drmModeSetCrtc(fd, crtc->crtc_id, fb_id, 0, 0, &conn->connector_id, 1, mode) < 0) {
        perror("drmModeSetCrtc");
        munmap(fb_map, mode->hdisplay * mode->vdisplay * 4);
        drmModeRmFB(fd, fb_id);
        drmModeFreeConnector(conn);
        drmModeFreeEncoder(enc);
        drmModeFreeCrtc(crtc);
        drmDropMaster(fd);
        close(fd);
        return -1;
    }

    g_display.backend      = DISP_DRM;
    g_display.width        = mode->hdisplay;
    g_display.height       = mode->vdisplay;
    g_display.refresh_hz   = mode->vrefresh;
    g_display.drm_fd       = fd;
    g_display.crtc_id      = crtc->crtc_id;
    g_display.connector_id = conn->connector_id;
    g_display.fb_id        = fb_id;
    g_display.fb_map       = fb_map;
    g_display.vbe_back     = fb_map;
    g_display.needs_flip   = true;

    drmModeFreeConnector(conn);
    drmModeFreeEncoder(enc);
    drmModeFreeCrtc(crtc);

    printf("[metal] DRM/KMS initialized: %dx%d@%dHz\n",
           g_display.width, g_display.height, g_display.refresh_hz);
    return 0;
}

void wubu_drm_shutdown(void) {
    if (g_display.drm_fd >= 0) {
        if (g_display.fb_map) {
            munmap(g_display.fb_map, g_display.width * g_display.height * 4);
            g_display.fb_map = NULL;
        }
        if (g_display.fb_id) {
            drmModeRmFB(g_display.drm_fd, g_display.fb_id);
            g_display.fb_id = 0;
        }
        drmDropMaster(g_display.drm_fd);
        close(g_display.drm_fd);
        g_display.drm_fd = -1;
    }
}

void wubu_drm_flip(void) {
    /* For dumb buffer, content is directly in fb_map - no flip needed */
    g_display.needs_flip = false;
}

int wubu_drm_set_mode(int width, int height, int refresh_hz) {
    (void)refresh_hz;
    /* Recreate framebuffer with new mode */
    wubu_drm_shutdown();
    return wubu_drm_init(width, height);
}

int wubu_drm_get_modes(int *widths, int *heights, int max) {
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) return 0;

    drmModeRes *res = drmModeGetResources(fd);
    if (!res) { close(fd); return 0; }

    int count = 0;
    for (int i = 0; i < res->count_connectors && count < max; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) continue;
        if (conn->connection == DRM_MODE_CONNECTED) {
            for (int j = 0; j < conn->count_modes && count < max; j++) {
                widths[count]  = conn->modes[j].hdisplay;
                heights[count] = conn->modes[j].vdisplay;
                count++;
            }
        }
        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
    close(fd);
    return count;
}
#else
int wubu_drm_init(int width, int height) {
    (void)width; (void)height; return -1; }
void wubu_drm_shutdown(void) {}
void wubu_drm_flip(void) {}
int wubu_drm_set_mode(int width, int height, int refresh_hz) {
    (void)width; (void)height; (void)refresh_hz; return -1; }
int wubu_drm_get_modes(int *widths, int *heights, int max) {
    (void)widths; (void)heights; (void)max; return 0; }
#endif
