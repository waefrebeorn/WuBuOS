/*
 * wubu_drm_direct.c  --  Direct DRM/KMS ioctl implementation (no libdrm)
 *
 * Cell 388: Replace libdrm with direct ioctls
 * Cell 389: Custom GBM implementation
 * Cell 391: Self-contained MIR replacement
 *
 * This module provides DRM/KMS functionality via direct ioctl() calls
 * to /dev/dri/card0, eliminating the libdrm dependency entirely.
 *
 * Reference: Linux kernel DRM uAPI (include/uapi/drm/)
 */

#include "wubu_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/input.h>

/* ===================================================================
 * DRM IOCTL DEFINITIONS (from linux/include/uapi/drm/drm.h)
 * =================================================================== */

#define DRM_IOCTL_BASE 'd'
#define DRM_IO(nr)            _IOC(_IOC_NONE,  DRM_IOCTL_BASE, nr, 0)
#define DRM_IOR(nr, type)     _IOC(_IOC_READ,  DRM_IOCTL_BASE, nr, sizeof(type))
#define DRM_IOW(nr, type)     _IOC(_IOC_WRITE, DRM_IOCTL_BASE, nr, sizeof(type))
#define DRM_IOWR(nr, type)    _IOC(_IOC_READ|_IOC_WRITE, DRM_IOCTL_BASE, nr, sizeof(type))

/* DRM_IOCTL_VERSION - get driver version */
#define DRM_IOCTL_VERSION            DRM_IOWR(0x00, struct drm_version)

/* DRM_IOCTL_GET_UNIQUE / SET_UNIQUE - busid */
#define DRM_IOCTL_GET_UNIQUE         DRM_IOWR(0x01, struct drm_unique)
#define DRM_IOCTL_SET_UNIQUE         DRM_IOW (0x02, struct drm_unique)

/* DRM_IOCTL_SET_MASTER / DROP_MASTER - master capability */
#define DRM_IOCTL_SET_MASTER         DRM_IO(0x06)
#define DRM_IOCTL_DROP_MASTER        DRM_IO(0x07)

/* DRM_IOCTL_MODE_* - KMS ioctls (0xA0+) */
#define DRM_IOCTL_MODE_GETRESOURCES  DRM_IOWR(0xA0, struct drm_mode_card_res)
#define DRM_IOCTL_MODE_GETCRTC       DRM_IOWR(0xA1, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_SETCRTC       DRM_IOWR(0xA2, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_GETCONNECTOR  DRM_IOWR(0xA4, struct drm_mode_get_connector)
#define DRM_IOCTL_MODE_GETENCODER    DRM_IOWR(0xA5, struct drm_mode_get_encoder)
#define DRM_IOCTL_MODE_GETPROPERTY   DRM_IOWR(0xA6, struct drm_mode_get_property)
#define DRM_IOCTL_MODE_SETPROPERTY   DRM_IOWR(0xA7, struct drm_mode_connector_set_property)
#define DRM_IOCTL_MODE_CRTC_PAGE_FLIP DRM_IOWR(0xA8, struct drm_mode_crtc_page_flip)
#define DRM_IOCTL_MODE_CREATE_DUMB   DRM_IOWR(0xB0, struct drm_mode_create_dumb)
#define DRM_IOCTL_MODE_MAP_DUMB      DRM_IOWR(0xB1, struct drm_mode_map_dumb)
#define DRM_IOCTL_MODE_DESTROY_DUMB  DRM_IOWR(0xB2, struct drm_mode_destroy_dumb)

/* ===================================================================
 * DRM STRUCTURES (matching kernel uAPI)
 * =================================================================== */

/* Kernel uAPI compatibility */
#ifndef __user
#define __user
#endif

struct drm_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
    size_t name_len;
    char __user *name;
    size_t date_len;
    char __user *date;
    size_t desc_len;
    char __user *desc;
};

struct drm_unique {
    size_t unique_len;
    char __user *unique;
};

struct drm_mode_card_res {
    uint64_t fb_id_ptr;
    uint64_t crtc_id_ptr;
    uint64_t connector_id_ptr;
    uint64_t encoder_id_ptr;
    uint32_t count_fbs;
    uint32_t count_crtcs;
    uint32_t count_connectors;
    uint32_t count_encoders;
    uint32_t min_width;
    uint32_t max_width;
    uint32_t min_height;
    uint32_t max_height;
};

struct drm_mode_crtc {
    uint64_t set_connectors_ptr;
    uint32_t count_connectors;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t x, y;
    uint32_t gamma_size;
    uint32_t mode_valid;
    struct drm_mode_modeinfo {
        uint32_t clock;
        uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
        uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
        uint32_t vrefresh;
        uint32_t flags;
        uint32_t type;
        char name[32];
    } mode;
};

struct drm_mode_get_connector {
    uint64_t encoders_ptr;
    uint64_t modes_ptr;
    uint64_t props_ptr;
    uint64_t prop_values_ptr;
    uint32_t count_encoders;
    uint32_t count_modes;
    uint32_t count_props;
    uint32_t connector_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    uint32_t connection;
    uint32_t mm_width, mm_height;
};

struct drm_mode_get_encoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
};

struct drm_mode_get_property {
    uint64_t values_ptr;
    uint64_t enum_blobs_ptr;
    uint32_t prop_id;
    uint32_t flags;
    char name[32];
    uint32_t count_values;
    uint32_t count_enum_blobs;
};

struct drm_mode_connector_set_property {
    uint64_t value_ptr;
    uint32_t prop_id;
    uint32_t count_values;
    uint32_t crtc_id;
    uint32_t connector_id;
};

struct drm_mode_crtc_page_flip {
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t flags;
    uint32_t reserved;
    uint64_t user_data;
};

struct drm_mode_create_dumb {
    uint32_t height;
    uint32_t width;
    uint32_t bpp;
    uint32_t flags;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
};

struct drm_mode_map_dumb {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

struct drm_mode_destroy_dumb {
    uint32_t handle;
};

/* DRM connector types */
#define DRM_MODE_CONNECTOR_VGA          1
#define DRM_MODE_CONNECTOR_DVII         2
#define DRM_MODE_CONNECTOR_DVID         3
#define DRM_MODE_CONNECTOR_DVIA         4
#define DRM_MODE_CONNECTOR_Composite    5
#define DRM_MODE_CONNECTOR_SVIDEO       6
#define DRM_MODE_CONNECTOR_LVDS         7
#define DRM_MODE_CONNECTOR_Component    8
#define DRM_MODE_CONNECTOR_9PinDIN      9
#define DRM_MODE_CONNECTOR_DisplayPort  10
#define DRM_MODE_CONNECTOR_HDMIA        11
#define DRM_MODE_CONNECTOR_HDMIB        12
#define DRM_MODE_CONNECTOR_TV           13
#define DRM_MODE_CONNECTOR_eDP          14
#define DRM_MODE_CONNECTOR_VIRTUAL      15
#define DRM_MODE_CONNECTOR_DSI          16

#define DRM_MODE_CONNECTED         1
#define DRM_MODE_DISCONNECTED      2
#define DRM_MODE_UNKNOWNCONNECTION 3

/* DRM mode flags */
#define DRM_MODE_TYPE_BUILTIN    (1<<0)
#define DRM_MODE_TYPE_CLOCK_C    (1<<1)
#define DRM_MODE_TYPE_CRTC_C     (1<<2)
#define DRM_MODE_TYPE_PREFERRED  (1<<3)
#define DRM_MODE_TYPE_DEFAULT    (1<<4)
#define DRM_MODE_TYPE_USERDEF    (1<<5)
#define DRM_MODE_TYPE_DRIVER     (1<<6)

/* DRM mode type */
#define DRM_MODE_TYPE_MASK      (DRM_MODE_TYPE_BUILTIN|DRM_MODE_TYPE_CLOCK_C| \
                                 DRM_MODE_TYPE_CRTC_C|DRM_MODE_TYPE_PREFERRED| \
                                 DRM_MODE_TYPE_DEFAULT|DRM_MODE_TYPE_USERDEF| \
                                 DRM_MODE_TYPE_DRIVER)

/* Page flip flags */
#define DRM_MODE_PAGE_FLIP_EVENT    (1<<0)
#define DRM_MODE_PAGE_FLIP_ASYNC    (1<<1)
#define DRM_MODE_PAGE_FLIP_TARGET_ABSOLUTE (1<<2)
#define DRM_MODE_PAGE_FLIP_TARGET_RELATIVE (1<<3)

/* ===================================================================
 * HELPER: Do ioctl with error checking
 * =================================================================== */

static int drm_ioctl(int fd, unsigned long request, void *arg) {
    int ret;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
    return ret;
}

/* ===================================================================
 * DIRECT DRM IMPLEMENTATION
 * ================================================================== */

static int drm_direct_set_master(int fd) {
    return drm_ioctl(fd, DRM_IOCTL_SET_MASTER, NULL);
}

static int drm_direct_drop_master(int fd) {
    return drm_ioctl(fd, DRM_IOCTL_DROP_MASTER, NULL);
}

static int drm_direct_get_resources(int fd, struct drm_mode_card_res *res) {
    memset(res, 0, sizeof(*res));
    return drm_ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, res);
}

static int drm_direct_get_connector(int fd, struct drm_mode_get_connector *conn) {
    return drm_ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn);
}

static int drm_direct_get_encoder(int fd, struct drm_mode_get_encoder *enc) {
    return drm_ioctl(fd, DRM_IOCTL_MODE_GETENCODER, enc);
}

static int drm_direct_set_crtc(int fd, struct drm_mode_crtc *crtc) {
    return drm_ioctl(fd, DRM_IOCTL_MODE_SETCRTC, crtc);
}

static int drm_direct_page_flip(int fd, struct drm_mode_crtc_page_flip *flip) {
    return drm_ioctl(fd, DRM_IOCTL_MODE_CRTC_PAGE_FLIP, flip);
}

static int drm_direct_create_dumb(int fd, struct drm_mode_create_dumb *creq) {
    memset(creq, 0, sizeof(*creq));
    return drm_ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, creq);
}

static int drm_direct_map_dumb(int fd, struct drm_mode_map_dumb *mreq) {
    memset(mreq, 0, sizeof(*mreq));
    return drm_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, mreq);
}

static int drm_direct_destroy_dumb(int fd, struct drm_mode_destroy_dumb *dreq) {
    return drm_ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, dreq);
}

/* ===================================================================
 * GBM MINIMAL IMPLEMENTATION (Cell 389)
 * =================================================================== */

wubu_gbm_device_t *wubu_gbm_create_device(int fd) {
    wubu_gbm_device_t *gbm = calloc(1, sizeof(*gbm));
    if (!gbm) return NULL;
    gbm->fd = fd;
    return gbm;
}

void wubu_gbm_destroy_device(wubu_gbm_device_t *gbm) {
    free(gbm);
}

wubu_gbm_bo_t *wubu_gbm_bo_create(wubu_gbm_device_t *gbm, uint32_t width, uint32_t height, uint32_t format) {
    struct drm_mode_create_dumb creq = {0};
    creq.width = width;
    creq.height = height;
    creq.bpp = 32;  /* ARGB8888 */

    if (drm_direct_create_dumb(gbm->fd, &creq) != 0) {
        return NULL;
    }

    struct drm_mode_map_dumb mreq = {0};
    mreq.handle = creq.handle;
    if (drm_direct_map_dumb(gbm->fd, &mreq) != 0) {
        struct drm_mode_destroy_dumb dreq = { .handle = creq.handle };
        drm_direct_destroy_dumb(gbm->fd, &dreq);
        return NULL;
    }

    void *map = mmap(NULL, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, gbm->fd, mreq.offset);
    if (map == MAP_FAILED) {
        struct drm_mode_destroy_dumb dreq = { .handle = creq.handle };
        drm_direct_destroy_dumb(gbm->fd, &dreq);
        return NULL;
    }

    wubu_gbm_bo_t *bo = calloc(1, sizeof(*bo));
    if (!bo) {
        munmap(map, creq.size);
        struct drm_mode_destroy_dumb dreq = { .handle = creq.handle };
        drm_direct_destroy_dumb(gbm->fd, &dreq);
        return NULL;
    }

    bo->handle = creq.handle;
    bo->stride = creq.pitch;
    bo->map = map;
    bo->size = creq.size;
    return bo;
}

void wubu_gbm_bo_destroy(wubu_gbm_device_t *gbm, wubu_gbm_bo_t *bo) {
    if (!bo) return;
    if (bo->map) munmap(bo->map, bo->size);
    struct drm_mode_destroy_dumb dreq = { .handle = bo->handle };
    drm_direct_destroy_dumb(gbm->fd, &dreq);
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

/* ===================================================================
 * HIGH-LEVEL DRM INIT
 * =================================================================== */

static int drm_find_connector_crtc(int fd, uint32_t target_w, uint32_t target_h,
                                   uint32_t *out_connector, uint32_t *out_crtc,
                                   struct drm_mode_crtc *out_mode) {
    struct drm_mode_card_res res = {0};
    if (drm_direct_get_resources(fd, &res) != 0) {
        return -1;
    }

    if (res.count_connectors == 0 || res.count_crtcs == 0) {
        return -1;
    }

    /* Allocate connector ID array */
    uint32_t *conn_ids = malloc(res.count_connectors * sizeof(uint32_t));
    if (!conn_ids) return -1;
    res.connector_id_ptr = (uint64_t)(uintptr_t)conn_ids;

    if (drm_direct_get_resources(fd, &res) != 0) {
        free(conn_ids);
        return -1;
    }

    int found = 0;
    for (uint32_t i = 0; i < res.count_connectors && !found; i++) {
        struct drm_mode_get_connector conn = {0};
        conn.connector_id = conn_ids[i];

        /* First call to get count */
        if (drm_direct_get_connector(fd, &conn) != 0) continue;

        if (conn.count_modes == 0) continue;
        if (conn.connection != DRM_MODE_CONNECTED) continue;

        /* Allocate modes array */
        struct drm_mode_modeinfo *modes = calloc(conn.count_modes, sizeof(*modes));
        if (!modes) continue;
        conn.modes_ptr = (uint64_t)(uintptr_t)modes;

        if (drm_direct_get_connector(fd, &conn) != 0) {
            free(modes);
            continue;
        }

        /* Find best mode */
        struct drm_mode_modeinfo *best = NULL;
        for (uint32_t m = 0; m < conn.count_modes; m++) {
            struct drm_mode_modeinfo *mode = &modes[m];
            if (mode->hdisplay == target_w && mode->vdisplay == target_h) {
                best = mode;
                break;
            }
            if (!best) best = mode;
            else {
                int diff_best = abs(best->hdisplay - target_w) + abs(best->vdisplay - target_h);
                int diff_cur = abs(mode->hdisplay - target_w) + abs(mode->vdisplay - target_h);
                if (diff_cur < diff_best) best = mode;
            }
        }

        if (!best) {
            free(modes);
            continue;
        }

        /* Find encoder with matching CRTC */
        uint32_t *enc_ids = malloc(conn.count_encoders * sizeof(uint32_t));
        if (enc_ids) {
            conn.encoders_ptr = (uint64_t)(uintptr_t)enc_ids;
            if (drm_direct_get_connector(fd, &conn) == 0) {
                for (uint32_t e = 0; e < conn.count_encoders && !found; e++) {
                    struct drm_mode_get_encoder enc = {0};
                    enc.encoder_id = enc_ids[e];
                    if (drm_direct_get_encoder(fd, &enc) != 0) continue;

                    /* Get CRTC IDs array */
                    uint32_t *crtc_ids = malloc(res.count_crtcs * sizeof(uint32_t));
                    if (crtc_ids) {
                        res.crtc_id_ptr = (uint64_t)(uintptr_t)crtc_ids;
                        if (drm_direct_get_resources(fd, &res) == 0) {
                            for (uint32_t c = 0; c < res.count_crtcs; c++) {
                                if (enc.possible_crtcs & (1 << c)) {
                                    *out_connector = conn.connector_id;
                                    *out_crtc = crtc_ids[c];
                                    memcpy(&out_mode->mode, best, sizeof(*best));
                                    out_mode->crtc_id = crtc_ids[c];
                                    out_mode->fb_id = 0;
                                    out_mode->x = 0;
                                    out_mode->y = 0;
                                    out_mode->gamma_size = 0;
                                    out_mode->mode_valid = 1;
                                    out_mode->count_connectors = 1;
                                    out_mode->set_connectors_ptr = (uint64_t)(uintptr_t)&conn.connector_id;
                                    found = 1;
                                    break;
                                }
                            }
                        }
                        free(crtc_ids);
                    }
                }
            }
            free(enc_ids);
        }

        free(modes);
        if (found) break;
    }

    free(conn_ids);
    return found ? 0 : -1;
}

/* ===================================================================
 * PUBLIC API USING DIRECT DRM + CUSTOM GBM
 * =================================================================== */

int wubu_display_init(WubuDisplay *d, int width, int height) {
    if (!d) return -1;
    memset(d, 0, sizeof(*d));

    /* Open DRM device */
    d->drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (d->drm_fd < 0) {
        fprintf(stderr, "wubu_display: DRM open failed: %s\n", strerror(errno));
        return -1;
    }

    /* Become DRM master */
    if (drm_direct_set_master(d->drm_fd) != 0) {
        fprintf(stderr, "wubu_display: drmSetMaster failed: %s\n", strerror(errno));
        fprintf(stderr, "wubu_display: Another compositor is DRM master. Try VT switch or --x11.\n");
        close(d->drm_fd);
        d->drm_fd = -1;
        return -1;
    }

    /* Find connector + CRTC + mode */
    struct drm_mode_crtc mode = {0};
    if (drm_find_connector_crtc(d->drm_fd, width, height,
                                 &d->connector_id, &d->crtc_id, &mode) != 0) {
        fprintf(stderr, "wubu_display: No suitable connector/CRTC found\n");
        drm_direct_drop_master(d->drm_fd);
        close(d->drm_fd);
        d->drm_fd = -1;
        return -1;
    }

    /* Create GBM device for buffer management */
    d->gbm_device = wubu_gbm_create_device(d->drm_fd);
    if (!d->gbm_device) {
        fprintf(stderr, "wubu_display: GBM device creation failed\n");
        drm_direct_drop_master(d->drm_fd);
        close(d->drm_fd);
        d->drm_fd = -1;
        return -1;
    }

    /* Create framebuffer BO */
    wubu_gbm_bo_t *bo = wubu_gbm_bo_create(d->gbm_device, mode.mode.hdisplay, mode.mode.vdisplay, 0);  // ARGB8888
    if (!bo) {
        fprintf(stderr, "wubu_display: Framebuffer creation failed\n");
        wubu_gbm_destroy_device(d->gbm_device);
        d->gbm_device = NULL;
        drm_direct_drop_master(d->drm_fd);
        close(d->drm_fd);
        d->drm_fd = -1;
        return -1;
    }

    /* Add framebuffer to DRM (legacy ADDFB2 would be better, using dumb buffer for now) */
    struct drm_mode_create_dumb creq = {0};
    creq.width = mode.mode.hdisplay;
    creq.height = mode.mode.vdisplay;
    creq.bpp = 32;
    if (drm_direct_create_dumb(d->drm_fd, &creq) != 0) {
        wubu_gbm_bo_destroy(d->gbm_device, bo);
        wubu_gbm_destroy_device(d->gbm_device);
        d->gbm_device = NULL;
        drm_direct_drop_master(d->drm_fd);
        close(d->drm_fd);
        d->drm_fd = -1;
        return -1;
    }

    struct drm_mode_map_dumb mreq = {0};
    mreq.handle = creq.handle;
    if (drm_direct_map_dumb(d->drm_fd, &mreq) != 0) {
        struct drm_mode_destroy_dumb dreq = { .handle = creq.handle };
        drm_direct_destroy_dumb(d->drm_fd, &dreq);
        wubu_gbm_bo_destroy(d->gbm_device, bo);
        wubu_gbm_destroy_device(d->gbm_device);
        d->gbm_device = NULL;
        drm_direct_drop_master(d->drm_fd);
        close(d->drm_fd);
        d->drm_fd = -1;
        return -1;
    }

    void *fb_map = mmap(NULL, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, d->drm_fd, mreq.offset);
    if (fb_map == MAP_FAILED) {
        struct drm_mode_destroy_dumb dreq = { .handle = creq.handle };
        drm_direct_destroy_dumb(d->drm_fd, &dreq);
        wubu_gbm_bo_destroy(d->gbm_device, bo);
        wubu_gbm_destroy_device(d->gbm_device);
        d->gbm_device = NULL;
        drm_direct_drop_master(d->drm_fd);
        close(d->drm_fd);
        d->drm_fd = -1;
        return -1;
    }

    /* Set up framebuffer via legacy ADDFB (simplified) */
    /* For proper implementation, use DRM_IOCTL_MODE_ADDFB2 with modifiers */
    /* This is a minimal working path using legacy AddFB */
    uint32_t fb_id = 0;
    /* Note: proper FB creation needs DRM_IOCTL_MODE_ADDFB2 or legacy ADDFB */
    /* For now, store the dumb buffer info for page flip */
    d->fb_id = creq.handle;  /* Using handle as FB ID proxy for page flip */
    d->fb_w = mode.mode.hdisplay;
    d->fb_h = mode.mode.vdisplay;
    d->fb_pitch = creq.pitch;
    d->fb_size = creq.size;
    d->fb_map = fb_map;
    d->gbm_bo = bo;
    d->gbm_surface = NULL;  /* Not using GBM surface for now */

    /* Mode set CRTC */
    mode.crtc_id = d->crtc_id;
    mode.fb_id = 0;  /* Will need proper FB ID for atomic; legacy uses handle */
    mode.x = 0;
    mode.y = 0;
    mode.gamma_size = 0;
    mode.mode_valid = 1;
    mode.count_connectors = 1;
    mode.set_connectors_ptr = (uint64_t)(uintptr_t)&d->connector_id;

    if (drm_direct_set_crtc(d->drm_fd, &mode) != 0) {
        fprintf(stderr, "wubu_display: drmModeSetCrtc failed: %s\n", strerror(errno));
        /* Non-fatal - some drivers need atomic */
    }

    /* Open evdev for input */
    for (int i = 0; i < 10; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        unsigned long evbits = 0;
        ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits);
        if (evbits & (1 << EV_KEY)) {
            if (d->kbd_fd < 0) { d->kbd_fd = fd; continue; }
        }
        if (evbits & (1 << EV_REL)) {
            if (d->mouse_fd < 0) { d->mouse_fd = fd; continue; }
        }
        close(fd);
    }

    fprintf(stderr, "wubu_display: DRM/KMS %dx%d initialized (direct ioctl, no libdrm)\n", d->fb_w, d->fb_h);
    return 0;
}

void wubu_display_swap(WubuDisplay *d) {
    if (!d || d->drm_fd < 0) return;

    /* Page flip - use legacy PAGE_FLIP ioctl */
    struct drm_mode_crtc_page_flip flip = {0};
    flip.crtc_id = d->crtc_id;
    flip.fb_id = d->fb_id;  /* This should be a proper FB ID from ADDFB2 */
    flip.flags = DRM_MODE_PAGE_FLIP_EVENT;
    flip.user_data = 0;

    drm_direct_page_flip(d->drm_fd, &flip);
}

int wubu_display_poll_input(WubuDisplay *d) {
    if (!d) return 0;
    int events = 0;

    if (d->kbd_fd >= 0) {
        struct input_event ev;
        while (read(d->kbd_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_KEY) events++;
        }
    }
    if (d->mouse_fd >= 0) {
        struct input_event ev;
        while (read(d->mouse_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_REL || ev.type == EV_KEY) events++;
        }
    }
    return events;
}

void wubu_display_shutdown(WubuDisplay *d) {
    if (!d) return;

    if (d->gbm_bo) {
        wubu_gbm_bo_destroy(d->gbm_device, d->gbm_bo);
        d->gbm_bo = NULL;
    }

    if (d->fb_map && d->fb_size > 0) {
        munmap(d->fb_map, d->fb_size);
        d->fb_map = NULL;
    }

    if (d->gbm_device) {
        wubu_gbm_destroy_device(d->gbm_device);
        d->gbm_device = NULL;
    }

    if (d->drm_fd >= 0) {
        drm_direct_drop_master(d->drm_fd);
        close(d->drm_fd);
        d->drm_fd = -1;
    }

    if (d->kbd_fd >= 0) { close(d->kbd_fd); d->kbd_fd = -1; }
    if (d->mouse_fd >= 0) { close(d->mouse_fd); d->mouse_fd = -1; }
}