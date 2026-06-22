/*
 * wubu_gbm.c  --  WuBuOS Custom GBM (Generic Buffer Management)
 *
 * Cell 389: Pure C GBM implementation without libgbm dependency.
 * Uses Linux DRM/KMS dumb buffers directly via ioctls.
 * Replaces libgbm for SteamOS/Steam Deck compatibility.
 */

#include "wubu_gbm.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <errno.h>
#include <stdint.h>

/* Full struct definition for opaque type */
struct wubu_gbm_device {
    int fd;
};

/* -- FourCC Format Helpers ---------------------------------------- */

static uint32_t fourcc_argb8888(void) { return 0x34325241; }  /* AR24 */
static uint32_t fourcc_xrgb8888(void) { return 0x34325258; }  /* XR24 */
static uint32_t fourcc_abgr8888(void) { return 0x34324241; }  /* AB24 */
static uint32_t fourcc_xbgr8888(void) { return 0x34324258; }  /* XB24 */
static uint32_t fourcc_rgba8888(void) { return 0x34324752; }  /* RG24 */
static uint32_t fourcc_bgra8888(void) { return 0x34324742; }  /* BG24 */

/* Default to ARGB8888 for display */
static uint32_t g_default_format = 0x34325241;

/* -- DRM Helper Functions ----------------------------------------- */

static int drm_create_dumb(int fd, struct drm_mode_create_dumb *creq) {
    memset(creq, 0, sizeof(*creq));
    if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, creq) != 0)
        return -errno;
    return 0;
}

static int drm_destroy_dumb(int fd, struct drm_mode_destroy_dumb *dreq) {
    if (ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, dreq) != 0)
        return -errno;
    return 0;
}

static int drm_map_dumb(int fd, struct drm_mode_map_dumb *mreq) {
    if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, mreq) != 0)
        return -errno;
    return 0;
}

static uint32_t drm_add_fb(int fd, uint32_t width, uint32_t height,
                           uint32_t pitch, uint32_t handle, uint32_t format) {
    struct drm_mode_fb_cmd2 fb_cmd = {0};
    fb_cmd.width = width;
    fb_cmd.height = height;
    fb_cmd.pitches[0] = pitch;
    fb_cmd.handles[0] = handle;
    fb_cmd.pixel_format = format;
    if (ioctl(fd, DRM_IOCTL_MODE_ADDFB2, &fb_cmd) != 0)
        return 0;
    return fb_cmd.fb_id;
}

static int drm_rm_fb(int fd, uint32_t fb_id) {
    if (ioctl(fd, DRM_IOCTL_MODE_RMFB, fb_id) != 0)
        return -errno;
    return 0;
}

/* -- GBM Public API ------------------------------------------------ */

wubu_gbm_device_t *wubu_gbm_create_device(int fd) {
    if (fd < 0) return NULL;
    wubu_gbm_device_t *gbm = calloc(1, sizeof(*gbm));
    if (!gbm) return NULL;
    gbm->fd = fd;
    return gbm;
}

void wubu_gbm_destroy_device(wubu_gbm_device_t *gbm) {
    if (gbm) free(gbm);
}

wubu_gbm_bo_t *wubu_gbm_bo_create(wubu_gbm_device_t *gbm,
                                   uint32_t width, uint32_t height,
                                   uint32_t format) {
    if (!gbm || width == 0 || height == 0) return NULL;

    if (format == 0) format = g_default_format;

    struct drm_mode_create_dumb creq = {0};
    creq.width = width;
    creq.height = height;
    creq.bpp = 32;

    if (drm_create_dumb(gbm->fd, &creq) != 0)
        return NULL;

    struct drm_mode_map_dumb mreq = {0};
    mreq.handle = creq.handle;
    if (drm_map_dumb(gbm->fd, &mreq) != 0) {
        drm_destroy_dumb(gbm->fd, &(struct drm_mode_destroy_dumb){.handle = creq.handle});
        return NULL;
    }

    void *map = mmap(NULL, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, gbm->fd, mreq.offset);
    if (map == MAP_FAILED) {
        drm_destroy_dumb(gbm->fd, &(struct drm_mode_destroy_dumb){.handle = creq.handle});
        return NULL;
    }

    wubu_gbm_bo_t *bo = calloc(1, sizeof(*bo));
    if (!bo) {
        munmap(map, creq.size);
        drm_destroy_dumb(gbm->fd, &(struct drm_mode_destroy_dumb){.handle = creq.handle});
        return NULL;
    }

    bo->handle = creq.handle;
    bo->stride = creq.pitch;
    bo->map = map;
    bo->size = creq.size;
    bo->width = width;
    bo->height = height;
    bo->format = format;
    bo->fb_id = drm_add_fb(gbm->fd, width, height, creq.pitch, creq.handle, format);

    return bo;
}

void wubu_gbm_bo_destroy(wubu_gbm_device_t *gbm, wubu_gbm_bo_t *bo) {
    if (!gbm || !bo) return;

    if (bo->fb_id) drm_rm_fb(gbm->fd, bo->fb_id);
    if (bo->map && bo->size) munmap(bo->map, bo->size);
    if (bo->handle) {
        struct drm_mode_destroy_dumb dreq = { .handle = bo->handle };
        drm_destroy_dumb(gbm->fd, &dreq);
    }
    free(bo);
}

void *wubu_gbm_bo_get_map(wubu_gbm_bo_t *bo) {
    return bo ? bo->map : NULL;
}

uint32_t wubu_gbm_bo_get_stride(wubu_gbm_bo_t *bo) {
    return bo ? bo->stride : 0;
}

uint32_t wubu_gbm_bo_get_handle(wubu_gbm_bo_t *bo) {
    return bo ? bo->handle : 0;
}

uint32_t wubu_gbm_bo_get_width(wubu_gbm_bo_t *bo) {
    return bo ? bo->width : 0;
}

uint32_t wubu_gbm_bo_get_height(wubu_gbm_bo_t *bo) {
    return bo ? bo->height : 0;
}

uint32_t wubu_gbm_bo_get_format(wubu_gbm_bo_t *bo) {
    return bo ? bo->format : 0;
}

uint32_t wubu_gbm_bo_get_fb_id(wubu_gbm_bo_t *bo) {
    return bo ? bo->fb_id : 0;
}

void wubu_gbm_set_default_format(uint32_t format) {
    g_default_format = format;
}

int wubu_gbm_fd(wubu_gbm_device_t *gbm) {
    return gbm ? gbm->fd : -1;
}