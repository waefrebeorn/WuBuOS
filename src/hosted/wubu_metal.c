/*
 * wubu_metal.c  --  WuBuOS Bare-Metal Boot + WSL2 GUI Abstraction
 *
 * Cell 400: Implementation of unified display/input/audio across three boot paths.
 *
 * Build: make all (add to Makefile)
 */

#include "wubu_metal.h"
#include "wubu_metal_audio.h"
#include "../audio/wubu_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <dlfcn.h>

/* Kernel subsystems for bare-metal */
#include "../kernel/interrupt.h"
#include "../kernel/tasking.h"
#include "../kernel/memory.h"
#include "../kernel/vbe.h"
#include "../kernel/wubu_gaad.h"

/* Forward-declare wubu_shell_run. Metal build links metal_main.c's signature
 * (void *arg). Test build uses a weak stub with (int,int) signature. Marking
 * this weak keeps both resolvable - the linker prefers the strong symbol. */
__attribute__((weak)) int wubu_shell_run(int width, int height);

/* ------------------------------------------------------------------
 *  GLOBAL STATE
 * ------------------------------------------------------------------ */

WubuBootEnv      g_env        = WUBU_ENV_UNKNOWN;
WubuDisplay      g_display    = {0};
WubuInput        g_input      = {0};
WubuAudio        g_audio      = {0};
bool             g_initialized = false;

/* ------------------------------------------------------------------
 *  DRM ATOMIC COMMIT (for modern KMS)
 * ------------------------------------------------------------------ */

#ifdef WUBU_USE_DRM
#include <xf86drm.h>
#include <xf86drmMode.h>

/* DRM Atomic ioctl definitions */
#define DRM_IOCTL_MODE_ATOMIC          DRM_IOWR(0xAE, struct drm_mode_atomic)
#define DRM_IOCTL_MODE_CREATE_PROP     DRM_IOWR(0xA3, struct drm_mode_create_prop)
#define DRM_IOCTL_MODE_GET_PROP        DRM_IOWR(0xA6, struct drm_mode_get_property)

struct drm_mode_atomic {
    uint64_t flags;
    uint64_t count_objs;
    uint64_t objs_ptr;
    uint64_t count_props;
    uint64_t props_ptr;
    uint64_t prop_values_ptr;
    uint64_t reserved;
    uint64_t user_data;
};

struct drm_mode_object_properties {
    uint64_t obj_id;
    uint64_t count_props;
    uint64_t props_ptr;
    uint64_t prop_values_ptr;
};

struct drm_mode_create_prop {
    uint64_t flags;
    uint64_t name_ptr;
    uint32_t count_values;
    uint64_t values_ptr;
    uint32_t prop_id;
};

static int drm_atomic_commit(int fd, uint32_t crtc_id, uint32_t fb_id, uint32_t connector_id, 
                             drmModeModeInfo *mode, int x, int y) {
    /* For now, use legacy SetCrtc - atomic requires property enumeration first */
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
 *  BOOT ENVIRONMENT DETECTION
 * ------------------------------------------------------------------ */

static WubuBootEnv wubu_detect_env_impl(void) {
    /* Check /proc/cpuinfo for hypervisor */
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "hypervisor") || strstr(line, "Microsoft") || strstr(line, "WSL")) {
                fclose(f);
                return WUBU_ENV_WSL2;
            }
        }
        fclose(f);
    }

    /* Check for WSL2 specific */
    if (access("/proc/sys/kernel/osrelease", R_OK) == 0) {
        f = fopen("/proc/sys/kernel/osrelease", "r");
        if (f) {
            char buf[256] = {0};
            if (fgets(buf, sizeof(buf), f)) {
                if (strstr(buf, "microsoft") || strstr(buf, "WSL")) {
                    fclose(f);
                    return WUBU_ENV_WSL2;
                }
            }
            fclose(f);
        }
    }

    /* Check for /dev/dxg (WSL2 paravirt GPU) */
    if (access("/dev/dxg", R_OK) == 0) {
        return WUBU_ENV_WSL2;
    }

    /* Check if we have /dev/dri (bare metal or hosted) */
    if (access("/dev/dri/card0", R_OK) == 0) {
        /* Check if running as init (PID 1) → bare metal */
        if (getpid() == 1 || getppid() == 1) {
            return WUBU_ENV_METAL;
        }
        return WUBU_ENV_HOSTED;
    }

    /* Check for X11 DISPLAY */
    if (getenv("DISPLAY")) {
        return WUBU_ENV_HOSTED;
    }

    /* Check for Wayland */
    if (getenv("WAYLAND_DISPLAY") || getenv("XDG_SESSION_TYPE")) {
        if (access("/dev/dri/card0", R_OK) == 0) return WUBU_ENV_METAL;
        return WUBU_ENV_HOSTED;
    }

    return WUBU_ENV_UNKNOWN;
}

WubuBootEnv wubu_detect_env(void) {
    if (g_env == WUBU_ENV_UNKNOWN) {
        g_env = wubu_detect_env_impl();
    }
    return g_env;
}

const char *wubu_env_name(WubuBootEnv env) {
    switch (env) {
        case WUBU_ENV_HOSTED:   return "hosted";
        case WUBU_ENV_METAL:    return "metal";
        case WUBU_ENV_WSL2:     return "wsl2";
        case WUBU_ENV_MACOS:    return "macos";
        default:                return "unknown";
    }
}

bool wubu_is_metal(void) { return wubu_detect_env() == WUBU_ENV_METAL; }
bool wubu_is_wsl2(void)  { return wubu_detect_env() == WUBU_ENV_WSL2; }

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

static int wubu_drm_init(int width, int height) {
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

static void wubu_drm_shutdown(void) {
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

static void wubu_drm_flip(void) {
    /* For dumb buffer, content is directly in fb_map - no flip needed */
    g_display.needs_flip = false;
}

static int wubu_drm_set_mode(int width, int height, int refresh_hz) {
    /* Recreate framebuffer with new mode */
    wubu_drm_shutdown();
    return wubu_drm_init(width, height);
}

static int wubu_drm_get_modes(int *widths, int *heights, int max) {
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
static int wubu_drm_init(int width, int height) {
    (void)width; (void)height; return -1; }
static void wubu_drm_shutdown(void) {}
static void wubu_drm_flip(void) {}
static int wubu_drm_set_mode(int width, int height, int refresh_hz) {
    (void)width; (void)height; (void)refresh_hz; return -1; }
static int wubu_drm_get_modes(int *widths, int *heights, int max) {
    (void)widths; (void)heights; (void)max; return 0; }
#endif

/* ------------------------------------------------------------------
 *  EVDEV INPUT BACKEND
 * ------------------------------------------------------------------ */


/* Audio backends (ALSA/Pulse/PipeWire, real + dlopen-stub) extracted ->
 * src/hosted/wubu_metal_audio.c (separate TU, HOSTED_OBJS_LIST).
 * Dispatched by wubu_audio_* below. */

/* ------------------------------------------------------------------
 *  WSL2 SPECIFIC
 * ------------------------------------------------------------------ */

int wubu_wsl2_disp_init(void) {
    /* WSL2 uses Weston/WSLg via Wayland */
    const char *wayland = getenv("WAYLAND_DISPLAY");
    const char *xdg_runtime = getenv("XDG_RUNTIME_DIR");

    if (wayland && xdg_runtime) {
        snprintf(g_display.wayland_socket, sizeof(g_display.wayland_socket), "%s/%s", xdg_runtime, wayland);
        g_display.backend = DISP_WAYLAND;
        g_display.width = 1280;
        g_display.height = 720;
        printf("[metal] WSL2 Wayland: %s\n", g_display.wayland_socket);
        return 0;
    }

    /* Fallback: try X11 if DISPLAY set (WSLg X11 bridge) */
    if (getenv("DISPLAY")) {
        g_display.backend = DISP_X11;
        return 0;
    }

    return -1;
}

int wubu_wsl2_audio_init(void) {
    /* WSL2 uses wslg PulseAudio bridge */
    const char *pulse = getenv("PULSE_SERVER");
    if (pulse) {
        snprintf(g_display.wslg_pulse, sizeof(g_display.wslg_pulse), "%s", pulse);
        return wubu_pulse_init(48000, 2, 256);
    }
    /* Default to localhost PulseAudio */
    return wubu_pulse_init(48000, 2, 256);
}

const char *wubu_wsl2_wayland_path(void) { return g_display.wayland_socket; }
const char *wubu_wsl2_pulse_path(void)   { return g_display.wslg_pulse; }

/* ------------------------------------------------------------------
 *  X11 DISPLAY BACKEND (HOSTED)
 * ------------------------------------------------------------------ */


/* ------------------------------------------------------------------
 *  VBE (LEGACY/BIOS) DISPLAY BACKEND
 * ------------------------------------------------------------------ */

static void vbe_init_fb(int width, int height) {
    if (g_display.vbe_back) free(g_display.vbe_back);
    g_display.vbe_back = calloc(width * height, sizeof(uint32_t));
    g_display.width = width; g_display.height = height;
}

static void vbe_shutdown_fb(void) {
    if (g_display.vbe_back) { free(g_display.vbe_back); g_display.vbe_back = NULL; }
}

/* ------------------------------------------------------------------
 *  UNIFIED DISPLAY API
 * ------------------------------------------------------------------ */

int wubu_disp_init(int width, int height) {
    if (g_initialized) return 0;

    WubuBootEnv env = wubu_detect_env();

    switch (env) {
        case WUBU_ENV_METAL:
            if (wubu_drm_init(width, height) == 0) break;
            /* Fallback to VBE */
            g_display.backend = DISP_VBE;
            vbe_init_fb(width, height);
            break;

        case WUBU_ENV_WSL2:
            if (wubu_wsl2_disp_init() == 0) break;
            if (wubu_x11_init(width, height) == 0) break;
            g_display.backend = DISP_VBE;
            vbe_init_fb(width, height);
            break;

        case WUBU_ENV_HOSTED:
        default:
            if (wubu_x11_init(width, height) == 0) break;
            if (wubu_drm_init(width, height) == 0) break;
            g_display.backend = DISP_VBE;
            vbe_init_fb(width, height);
            break;
    }

    wubu_evdev_init_all();

    /* Initialize audio based on environment */
    int sr = 48000, ch = 2, buf = 256;
    if (env == WUBU_ENV_WSL2) {
        wubu_wsl2_audio_init();
    } else {
        wubu_audio_init(sr, ch, buf);
    }

    g_initialized = true;
    printf("[metal] Display backend: %d, Input backend: %d, Audio backend: %d\n",
           g_display.backend, g_input.backend, g_audio.backend);
    return 0;
}

void wubu_disp_shutdown(void) {
    if (!g_initialized) return;

    switch (g_display.backend) {
        case DISP_DRM:       wubu_drm_shutdown(); break;
        case DISP_X11:       wubu_x11_shutdown(); break;
        case DISP_WAYLAND:   /* Wayland shutdown handled by compositor */ break;
        case DISP_VBE:       vbe_shutdown_fb(); break;
        case DISP_AUTO:      /* Auto-detected - already handled */ break;
    }

    wubu_evdev_shutdown();
    wubu_alsa_shutdown();
    wubu_pulse_shutdown();
    wubu_pipewire_shutdown();

    memset(&g_display, 0, sizeof(g_display));
    memset(&g_input, 0, sizeof(g_input));
    memset(&g_audio, 0, sizeof(g_audio));
    g_initialized = false;
}

WubuDisplay *wubu_disp_state(void) { return &g_display; }

int wubu_disp_set_mode(int width, int height, int refresh_hz) {
    switch (g_display.backend) {
        case DISP_DRM:       return wubu_drm_set_mode(width, height, refresh_hz);
        case DISP_X11:       return wubu_x11_set_mode(width, height, refresh_hz);
        case DISP_WAYLAND:   /* Handled by compositor */ return 0;
        case DISP_VBE:       vbe_init_fb(width, height); return 0;
        default: return -1;
    }
}

void wubu_disp_flip(void) {
    switch (g_display.backend) {
        case DISP_DRM:       wubu_drm_flip(); break;
        case DISP_X11:       wubu_x11_flip(); break;
        case DISP_WAYLAND:   /* Wayland handles flip */ break;
        case DISP_VBE:       /* VBE - direct write */ break;
        case DISP_AUTO:      /* Auto-detected - already handled */ break;
    }
    g_display.needs_flip = false;
}

void wubu_disp_poll_events(void) {
    wubu_input_poll();
    /* Backend-specific event polling could go here */
}

WubuDispBackend wubu_disp_current(void) { return g_display.backend; }

int wubu_disp_force(WubuDispBackend backend) {
    if (g_initialized) wubu_disp_shutdown();
    g_display.backend = backend;
    return wubu_disp_init(g_display.width, g_display.height);
}

/* ------------------------------------------------------------------
 *  UNIFIED INPUT API
 * ------------------------------------------------------------------ */

int wubu_input_init(void) {
    wubu_evdev_init_all();
    return 0;
}

void wubu_input_shutdown(void) { wubu_evdev_shutdown(); }

WubuInput *wubu_input_state(void) { return &g_input; }

int wubu_input_poll(void) { return wubu_evdev_poll(); }

int wubu_input_key_down(uint32_t key) { return wubu_evdev_key_down(key); }

void wubu_input_mouse_pos(int *x, int *y) { wubu_evdev_mouse_pos(x, y); }

int wubu_input_gamepads(char names[][64]) {
    int count = 0;
    for (int i = 0; i < g_input.n_gamepads && count < 4; i++) {
        snprintf(names[count], 64, "Gamepad %d", i);
        count++;
    }
    return count;
}

/* ------------------------------------------------------------------
 *  UNIFIED AUDIO API
 * ------------------------------------------------------------------ */

int wubu_audio_init(int sample_rate, int channels, int buffer_frames) {
    WubuBootEnv env = wubu_detect_env();
    int ret = -1;
    
    if (env == WUBU_ENV_METAL) {
        if (wubu_alsa_init(sample_rate, channels, buffer_frames) == 0) ret = 0;
        else if (wubu_pipewire_init(sample_rate, channels, buffer_frames) == 0) ret = 0;
        else if (wubu_pulse_init(sample_rate, channels, buffer_frames) == 0) ret = 0;
    } else {
        /* Try PipeWire first (modern audio first, then PulseAudio) */
        if (wubu_pipewire_init(sample_rate, channels, buffer_frames) == 0) ret = 0;
        else if (wubu_pulse_init(sample_rate, channels, buffer_frames) == 0) ret = 0;
    }
    
    return ret;
}

void wubu_audio_shutdown(void) {
    wubu_alsa_shutdown();
    wubu_pulse_shutdown();
    wubu_pipewire_shutdown();
}

WubuAudio *wubu_audio_state(void) { return &g_audio; }

void wubu_audio_submit(const float *buf, int frames) {
    switch (g_audio.backend) {
        case AUDIO_ALSA:      wubu_alsa_submit(buf, frames); break;
        case AUDIO_PULSE:     wubu_pulse_submit(buf, frames); break;
        case AUDIO_JACK:      /* JACK callback handles this */ break;
        case AUDIO_PIPEWIRE:  wubu_pipewire_submit(buf, frames); break;
        case AUDIO_AUTO:      /* Auto-detected - already handled */ break;
    }
}

double wubu_audio_cpu_load(void) {
    switch (g_audio.backend) {
        case AUDIO_ALSA:      return wubu_alsa_cpu_load();
        case AUDIO_PULSE:     return wubu_pulse_cpu_load();
        case AUDIO_PIPEWIRE:  return wubu_pipewire_cpu_load();
        default: return 0.0;
    }
}

/* ------------------------------------------------------------------
 *  BARE-METAL BOOT ENTRY POINTS
 * ------------------------------------------------------------------ */

int wubu_metal_init(int width, int height) {
    /* Force metal environment */
    g_env = WUBU_ENV_METAL;

    /* Initialize kernel subsystems */
    mem_init(1024 * 1024 * 4);
    vbe_init(width, height);

#ifndef WUBU_HOSTED_TEST
    interrupt_init();

    /* Initialize PIT timer for preemptive multitasking (100 Hz) */
    /* Only enable in real bare-metal environment (CAP_SYS_RAWIO) */
    if (pit_init(100) == 0) {
        task_preempt_enable();
        printf("[metal] PIT timer initialized at 100 Hz, preemption enabled\n");
    } else {
        printf("[metal] PIT init failed (no I/O privilege), running cooperative\n");
    }

    /* Initialize tasking */
    tasking_init();
#endif

    return wubu_disp_init(width, height);
}

void wubu_metal_run(void) {
    /* Run the unified GUI shell (Cell 207: integration) */
    wubu_shell_run(g_display.width, g_display.height);
}

void wubu_metal_shutdown(void) {
#ifndef WUBU_HOSTED_TEST
    pit_shutdown();
    interrupt_shutdown();
#endif
    vbe_shutdown();
    wubu_disp_shutdown();
}

/* ------------------------------------------------------------------
 *  RESOLUTION / GAAD INTEGRATION
 * ------------------------------------------------------------------ */

int wubu_disp_get_modes(int *widths, int *heights, int max) {
    if (g_display.backend == DISP_DRM) {
        return wubu_drm_get_modes(widths, heights, max);
    }
    /* Common modes */
    static const int common_w[] = { 640, 800, 1024, 1280, 1366, 1440, 1600, 1920, 2560, 3840 };
    static const int common_h[] = { 480, 600,  768,  720,  768,  900,  900, 1080, 1440, 2160 };
    int count = max < 10 ? max : 10;
    for (int i = 0; i < count; i++) {
        widths[i] = common_w[i];
        heights[i] = common_h[i];
    }
    return count;
}

void wubu_disp_gaad_nearest(int w, int h, int *out_w, int *out_h) {
    /* Use GAAD golden ratio subdivision to find nearest supported mode */
    int widths[16], heights[16];
    int count = wubu_disp_get_modes(widths, heights, 16);
    
    if (count > 0) {
        /* Find closest mode by area */
        int best_idx = 0;
        int target_area = w * h;
        int best_diff = abs(widths[0] * heights[0] - target_area);
        
        for (int i = 1; i < count; i++) {
            int diff = abs(widths[i] * heights[i] - target_area);
            if (diff < best_diff) {
                best_diff = diff;
                best_idx = i;
            }
        }
        *out_w = widths[best_idx];
        *out_h = heights[best_idx];
    } else {
        /* Fallback: use golden ratio to scale */
        *out_w = w;
        *out_h = h;
    }
}
