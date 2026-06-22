/*
 * wubu_gbm.h  --  WuBuOS GBM (Generic Buffer Management) API
 *
 * Cell 389: Pure C GBM implementation without libgbm dependency.
 * Replaces libgbm for SteamOS/Steam Deck compatibility.
 * Uses Linux DRM/KMS dumb buffers directly via ioctls.
 */

#ifndef WUBU_GBM_H
#define WUBU_GBM_H

#include <stdint.h>
#include <stddef.h>

/* -- Formats ------------------------------------------------------- */

#define GBM_FORMAT_ARGB8888  0x34325241u  /* 'AR24' */
#define GBM_FORMAT_XRGB8888  0x34325258u  /* 'XR24' */
#define GBM_FORMAT_ABGR8888  0x34324241u  /* 'AB24' */
#define GBM_FORMAT_XBGR8888  0x34324258u  /* 'XB24' */
#define GBM_FORMAT_RGBA8888  0x34324752u  /* 'RG24' */
#define GBM_FORMAT_BGRA8888  0x34324742u  /* 'BG24' */

/* -- Opaque Types -------------------------------------------------- */

typedef struct wubu_gbm_device wubu_gbm_device_t;
typedef struct wubu_gbm_bo wubu_gbm_bo_t;

struct wubu_gbm_bo {
    uint32_t handle;      /* DRM handle */
    uint32_t stride;      /* Bytes per row */
    void *map;            /* CPU mmap pointer */
    size_t size;          /* Total size in bytes */
    uint32_t width;       /* Buffer width */
    uint32_t height;      /* Buffer height */
    uint32_t format;      /* FourCC format */
    uint32_t fb_id;       /* DRM framebuffer ID */
};

/* -- Device Lifecycle ---------------------------------------------- */

/* Create GBM device from DRM fd */
wubu_gbm_device_t *wubu_gbm_create_device(int fd);

/* Destroy GBM device */
void wubu_gbm_destroy_device(wubu_gbm_device_t *gbm);

/* Get underlying DRM fd */
int wubu_gbm_fd(wubu_gbm_device_t *gbm);

/* -- Buffer Object (BO) Lifecycle ---------------------------------- */

/* Create buffer object */
wubu_gbm_bo_t *wubu_gbm_bo_create(wubu_gbm_device_t *gbm,
                                   uint32_t width, uint32_t height,
                                   uint32_t format);  /* 0 = ARGB8888 default */

/* Destroy buffer object */
void wubu_gbm_bo_destroy(wubu_gbm_device_t *gbm, wubu_gbm_bo_t *bo);

/* -- BO Properties ------------------------------------------------- */

void *wubu_gbm_bo_get_map(wubu_gbm_bo_t *bo);
uint32_t wubu_gbm_bo_get_stride(wubu_gbm_bo_t *bo);
uint32_t wubu_gbm_bo_get_handle(wubu_gbm_bo_t *bo);
uint32_t wubu_gbm_bo_get_width(wubu_gbm_bo_t *bo);
uint32_t wubu_gbm_bo_get_height(wubu_gbm_bo_t *bo);
uint32_t wubu_gbm_bo_get_format(wubu_gbm_bo_t *bo);
uint32_t wubu_gbm_bo_get_fb_id(wubu_gbm_bo_t *bo);

/* -- Format Configuration ------------------------------------------ */

/* Set default format for bo_create when format=0 */
void wubu_gbm_set_default_format(uint32_t format);

#endif /* WUBU_GBM_H */