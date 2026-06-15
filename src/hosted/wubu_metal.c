/*
 * wubu_metal.c  --  WuBuOS Bare-Metal Boot + WSL2 GUI Abstraction
 *
 * Cell 400: Implementation of unified display/input/audio across three boot paths.
 *
 * Build: make all (add to Makefile)
 */

#include "wubu_metal.h"
#include "../shell/wubu_shell.h"
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

/* Kernel subsystems for bare-metal */
#include "../kernel/interrupt.h"
#include "../kernel/tasking.h"
#include "../kernel/memory.h"
#include "../kernel/vbe.h"

/* ------------------------------------------------------------------
 *  GLOBAL STATE
 * ------------------------------------------------------------------ */

static WubuBootEnv      g_env        = WUBU_ENV_UNKNOWN;
static WubuDisplay      g_display    = {0};
static WubuInput        g_input      = {0};
static WubuAudio        g_audio      = {0};
static bool             g_initialized = false;

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

static int wubu_evdev_find_device(const char *type, int *out_fd) {
    DIR *d = opendir("/dev/input");
    if (!d) return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) && *out_fd < 0) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;

        char path[256];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        uint8_t evbit[EV_MAX / 8 + 1] = {0};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) {
            close(fd);
            continue;
        }

        bool match = false;
        if (strcmp(type, "keyboard") == 0) {
            if (evbit[EV_KEY / 8] & (1 << (EV_KEY % 8))) {
                /* Check for keyboard keys */
                uint8_t keybit[KEY_MAX / 8 + 1] = {0};
                ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
                if (keybit[KEY_A / 8] & (1 << (KEY_A % 8))) match = true;
            }
        } else if (strcmp(type, "mouse") == 0) {
            if ((evbit[EV_REL / 8] & (1 << (EV_REL % 8))) &&
                (evbit[EV_KEY / 8] & (1 << (EV_KEY % 8)))) {
                uint8_t relbit[REL_MAX / 8 + 1] = {0};
                ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relbit)), relbit);
                if ((relbit[REL_X / 8] & (1 << (REL_X % 8))) &&
                    (relbit[REL_Y / 8] & (1 << (REL_Y % 8)))) match = true;
            }
        } else if (strcmp(type, "touch") == 0) {
            if (evbit[EV_ABS / 8] & (1 << (EV_ABS % 8))) match = true;
        } else if (strcmp(type, "gamepad") == 0) {
            uint8_t keybit[KEY_MAX / 8 + 1] = {0};
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
            if (keybit[BTN_GAMEPAD / 8] & (1 << (BTN_GAMEPAD % 8))) match = true;
        } else if (strcmp(type, "midi") == 0) {
            /* MIDI devices often appear as HID */
            uint8_t keybit[KEY_MAX / 8 + 1] = {0};
            ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
            /* Check for generic MIDI/HID keys - BTN_MISC is more common */
            if (keybit[BTN_MISC / 8] & (1 << (BTN_MISC % 8))) match = true;
        }

        if (match) {
            *out_fd = fd;
        } else {
            close(fd);
        }
    }

    closedir(d);
    return *out_fd >= 0 ? 0 : -1;
}

static void wubu_evdev_init_all(void) {
    g_input.backend = INPUT_EVDEV;

    /* Keyboard */
    g_input.kbd_fd = -1;
    wubu_evdev_find_device("keyboard", &g_input.kbd_fd);
    if (g_input.kbd_fd >= 0) printf("[metal] Keyboard: /dev/input/event%d\n", g_input.kbd_fd);

    /* Mouse */
    g_input.mouse_fd = -1;
    wubu_evdev_find_device("mouse", &g_input.mouse_fd);
    if (g_input.mouse_fd >= 0) printf("[metal] Mouse: /dev/input/event%d\n", g_input.mouse_fd);

    /* Touch */
    g_input.touch_fd = -1;
    wubu_evdev_find_device("touch", &g_input.touch_fd);

    /* Gamepads */
    g_input.n_gamepads = 0;
    for (int i = 0; i < 4; i++) {
        g_input.gamepad_fds[i] = -1;
        if (wubu_evdev_find_device("gamepad", &g_input.gamepad_fds[i]) == 0) {
            g_input.n_gamepads++;
        }
    }

    /* MIDI HID */
    g_input.n_midi = 0;
    for (int i = 0; i < 4; i++) {
        g_input.midi_fds[i] = -1;
        if (wubu_evdev_find_device("midi", &g_input.midi_fds[i]) == 0) {
            g_input.n_midi++;
        }
    }

    /* USB HID Raw */
    g_input.n_hidraw = 0;
    DIR *d = opendir("/dev/hidraw");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) && g_input.n_hidraw < 8) {
            if (strncmp(ent->d_name, "hidraw", 6) != 0) continue;
            char path[256];
            snprintf(path, sizeof(path), "/dev/hidraw/%s", ent->d_name);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                g_input.hidraw_fds[g_input.n_hidraw++] = fd;
            }
        }
        closedir(d);
    }
}

static void wubu_evdev_shutdown(void) {
    if (g_input.kbd_fd >= 0) close(g_input.kbd_fd);
    if (g_input.mouse_fd >= 0) close(g_input.mouse_fd);
    if (g_input.touch_fd >= 0) close(g_input.touch_fd);
    for (int i = 0; i < g_input.n_gamepads; i++) if (g_input.gamepad_fds[i] >= 0) close(g_input.gamepad_fds[i]);
    for (int i = 0; i < g_input.n_midi; i++) if (g_input.midi_fds[i] >= 0) close(g_input.midi_fds[i]);
    for (int i = 0; i < g_input.n_hidraw; i++) if (g_input.hidraw_fds[i] >= 0) close(g_input.hidraw_fds[i]);
    memset(&g_input, 0, sizeof(g_input));
}

static int wubu_evdev_poll(void) {
    int events = 0;
    struct input_event ev[64];

    /* Keyboard */
    if (g_input.kbd_fd >= 0) {
        int n = read(g_input.kbd_fd, ev, sizeof(ev));
        if (n > 0) events += n / sizeof(struct input_event);
    }

    /* Mouse */
    if (g_input.mouse_fd >= 0) {
        int n = read(g_input.mouse_fd, ev, sizeof(ev));
        if (n > 0) events += n / sizeof(struct input_event);
    }

    /* Touch */
    if (g_input.touch_fd >= 0) {
        int n = read(g_input.touch_fd, ev, sizeof(ev));
        if (n > 0) events += n / sizeof(struct input_event);
    }

    /* Gamepads */
    for (int i = 0; i < g_input.n_gamepads; i++) {
        if (g_input.gamepad_fds[i] >= 0) {
            int n = read(g_input.gamepad_fds[i], ev, sizeof(ev));
            if (n > 0) events += n / sizeof(struct input_event);
        }
    }

    return events;
}

static int wubu_evdev_key_down(uint32_t key) {
    /* Simple state tracking - in production would maintain key state array */
    struct input_event ev[32];
    if (g_input.kbd_fd >= 0) {
        int n = read(g_input.kbd_fd, ev, sizeof(ev));
        for (int i = 0; i < n / (int)sizeof(struct input_event); i++) {
            if (ev[i].type == EV_KEY && ev[i].code == key && ev[i].value == 1) return 1;
        }
    }
    return 0;
}

static void wubu_evdev_mouse_pos(int *x, int *y) {
    static int mx = 0, my = 0;
    struct input_event ev[32];
    if (g_input.mouse_fd >= 0) {
        int n = read(g_input.mouse_fd, ev, sizeof(ev));
        for (int i = 0; i < n / (int)sizeof(struct input_event); i++) {
            if (ev[i].type == EV_REL) {
                if (ev[i].code == REL_X) mx += ev[i].value;
                else if (ev[i].code == REL_Y) my += ev[i].value;
            }
        }
    }
    *x = mx;
    *y = my;
}

/* ------------------------------------------------------------------
 *  ALSA AUDIO BACKEND (BARE-METAL)
 * ------------------------------------------------------------------ */

#ifdef WUBU_USE_ALSA
#include <alsa/asoundlib.h>

static int wubu_alsa_init(int sample_rate, int channels, int buffer_frames) {
    snd_pcm_t *handle;
    int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "ALSA open failed: %s\n", snd_strerror(err));
        return -1;
    }

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_FLOAT_LE);
    snd_pcm_hw_params_set_channels(handle, params, channels);
    unsigned int rate = sample_rate;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    snd_pcm_uframes_t frames = buffer_frames;
    snd_pcm_hw_params_set_buffer_size_near(handle, params, &frames);

    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        fprintf(stderr, "ALSA hw_params failed: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        return -1;
    }

    snd_pcm_prepare(handle);

    g_audio.backend       = AUDIO_ALSA;
    g_audio.sample_rate   = sample_rate;
    g_audio.channels      = channels;
    g_audio.buffer_frames = buffer_frames;
    g_audio.alsa_pcm_fd   = snd_pcm_file_descriptor(handle);
    g_audio.alsa_handle   = handle;
    g_audio.render_buf    = calloc(buffer_frames * channels, sizeof(float));
    g_audio.render_buf_size = buffer_frames * channels;

    printf("[metal] ALSA initialized: %dHz %dch %d frames\n", sample_rate, channels, buffer_frames);
    return 0;
}

static void wubu_alsa_shutdown(void) {
    if (g_audio.alsa_handle) {
        snd_pcm_drain((snd_pcm_t*)g_audio.alsa_handle);
        snd_pcm_close((snd_pcm_t*)g_audio.alsa_handle);
        g_audio.alsa_handle = NULL;
    }
    if (g_audio.render_buf) {
        free(g_audio.render_buf);
        g_audio.render_buf = NULL;
    }
}

static void wubu_alsa_submit(const float *buf, int frames) {
    if (!g_audio.alsa_handle || !buf) return;
    snd_pcm_writei((snd_pcm_t*)g_audio.alsa_handle, buf, frames);
}

static double wubu_alsa_cpu_load(void) {
    return 0.0; /* Would need ALSA timing info */
}
#else
static int wubu_alsa_init(int sr, int ch, int buf) { (void)sr; (void)ch; (void)buf; return -1; }
static void wubu_alsa_shutdown(void) {}
static void wubu_alsa_submit(const float *buf, int frames) { (void)buf; (void)frames; }
static double wubu_alsa_cpu_load(void) { return 0.0; }
#endif

/* ------------------------------------------------------------------
 *  PULSEAUDIO BACKEND (HOSTED/WSL2)
 * ------------------------------------------------------------------ */

#ifdef WUBU_USE_PULSE
#include <pulse/simple.h>
#include <pulse/error.h>

static int wubu_pulse_init(int sample_rate, int channels, int buffer_frames) {
    pa_sample_spec ss = { .format = PA_SAMPLE_FLOAT32LE, .rate = sample_rate, .channels = channels };
    pa_buffer_attr attr = { .maxlength = (uint32_t)-1, .tlength = buffer_frames * channels * sizeof(float),
                            .prebuf = (uint32_t)-1, .minreq = (uint32_t)-1, .fragsize = (uint32_t)-1 };
    int error;
    pa_simple *s = pa_simple_new(NULL, "WuBuOS", PA_STREAM_PLAYBACK, NULL, "WuBuOS Audio", &ss, NULL, &attr, &error);
    if (!s) {
        fprintf(stderr, "PulseAudio connect failed: %s\n", pa_strerror(error));
        return -1;
    }

    g_audio.backend       = AUDIO_PULSE;
    g_audio.sample_rate   = sample_rate;
    g_audio.channels      = channels;
    g_audio.buffer_frames = buffer_frames;
    g_audio.pa_handle     = s;

    printf("[metal] PulseAudio initialized: %dHz %dch\n", sample_rate, channels);
    return 0;
}

static void wubu_pulse_shutdown(void) {
    if (g_audio.pa_handle) {
        pa_simple_drain((pa_simple*)g_audio.pa_handle, NULL);
        pa_simple_free((pa_simple*)g_audio.pa_handle);
        g_audio.pa_handle = NULL;
    }
}

static void wubu_pulse_submit(const float *buf, int frames) {
    if (!g_audio.pa_handle || !buf) return;
    int error;
    pa_simple_write((pa_simple*)g_audio.pa_handle, buf, frames * g_audio.channels * sizeof(float), &error);
}

static double wubu_pulse_cpu_load(void) { return 0.0; }
#else
static int wubu_pulse_init(int sr, int ch, int buf) { (void)sr; (void)ch; (void)buf; return -1; }
static void wubu_pulse_shutdown(void) {}
static void wubu_pulse_submit(const float *buf, int frames) { (void)buf; (void)frames; }
static double wubu_pulse_cpu_load(void) { return 0.0; }
#endif

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

#ifdef WUBU_USE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Display *g_x11_dpy = NULL;
static Window   g_x11_win = 0;
static GC       g_x11_gc = 0;

static int wubu_x11_init(int width, int height) {
    g_x11_dpy = XOpenDisplay(NULL);
    if (!g_x11_dpy) {
        fprintf(stderr, "XOpenDisplay failed\n");
        return -1;
    }

    int screen = DefaultScreen(g_x11_dpy);
    g_x11_win = XCreateSimpleWindow(g_x11_dpy, RootWindow(g_x11_dpy, screen),
                                     0, 0, width, height, 0,
                                     BlackPixel(g_x11_dpy, screen),
                                     BlackPixel(g_x11_dpy, screen));

    XSelectInput(g_x11_dpy, g_x11_win,
                 ExposureMask | KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                 StructureNotifyMask);

    XMapWindow(g_x11_dpy, g_x11_win);
    XFlush(g_x11_dpy);

    g_x11_gc = XCreateGC(g_x11_dpy, g_x11_win, 0, NULL);

    g_display.backend      = DISP_X11;
    g_display.width        = width;
    g_display.height       = height;
    g_display.x11_display  = g_x11_dpy;
    g_display.x11_window   = g_x11_win;
    g_display.x11_gc       = g_x11_gc;
    g_display.vbe_back     = calloc(width * height, sizeof(uint32_t));

    printf("[metal] X11 initialized: %dx%d\n", width, height);
    return 0;
}

static void wubu_x11_shutdown(void) {
    if (g_x11_gc) XFreeGC(g_x11_dpy, g_x11_gc);
    if (g_x11_win) XDestroyWindow(g_x11_dpy, g_x11_win);
    if (g_x11_dpy) XCloseDisplay(g_x11_dpy);
    if (g_display.vbe_back) free(g_display.vbe_back);
    g_x11_dpy = NULL; g_x11_win = 0; g_x11_gc = 0; g_display.vbe_back = NULL;
}

static void wubu_x11_flip(void) {
    if (g_x11_dpy && g_x11_win && g_display.vbe_back) {
        XImage *img = XCreateImage(g_x11_dpy, DefaultVisual(g_x11_dpy, DefaultScreen(g_x11_dpy)),
                                    24, ZPixmap, 0, (char*)g_display.vbe_back,
                                    g_display.width, g_display.height, 32, 0);
        if (img) {
            XPutImage(g_x11_dpy, g_x11_win, g_x11_gc, img, 0, 0, 0, 0, g_display.width, g_display.height);
            XFlush(g_x11_dpy);
            XDestroyImage(img);
        }
    }
}

static int wubu_x11_set_mode(int width, int height, int refresh_hz) {
    (void)refresh_hz;
    if (g_x11_win && g_x11_dpy) {
        XResizeWindow(g_x11_dpy, g_x11_win, width, height);
        if (g_display.vbe_back) {
            free(g_display.vbe_back);
            g_display.vbe_back = calloc(width * height, sizeof(uint32_t));
        }
        g_display.width = width; g_display.height = height;
    }
    return 0;
}
#else
static int wubu_x11_init(int w, int h) { (void)w; (void)h; return -1; }
static void wubu_x11_shutdown(void) {}
static void wubu_x11_flip(void) {}
static int wubu_x11_set_mode(int w, int h, int r) { (void)w; (void)h; (void)r; return -1; }
#endif

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
    } else if (env == WUBU_ENV_METAL) {
        if (wubu_alsa_init(sr, ch, buf) != 0) {
            wubu_pulse_init(sr, ch, buf);  /* Fallback to PulseAudio */
        }
    } else {
        wubu_pulse_init(sr, ch, buf);
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
    if (env == WUBU_ENV_METAL) {
        return wubu_alsa_init(sample_rate, channels, buffer_frames);
    } else {
        return wubu_pulse_init(sample_rate, channels, buffer_frames);
    }
}

void wubu_audio_shutdown(void) {
    wubu_alsa_shutdown();
    wubu_pulse_shutdown();
}

WubuAudio *wubu_audio_state(void) { return &g_audio; }

void wubu_audio_submit(const float *buf, int frames) {
    switch (g_audio.backend) {
        case AUDIO_ALSA:    wubu_alsa_submit(buf, frames); break;
        case AUDIO_PULSE:   wubu_pulse_submit(buf, frames); break;
        case AUDIO_JACK:    /* JACK callback handles this */ break;
        case AUDIO_PIPEWIRE:/* PipeWire callback handles this */ break;
        case AUDIO_AUTO:    /* Auto-detected - already handled */ break;
    }
}

double wubu_audio_cpu_load(void) {
    switch (g_audio.backend) {
        case AUDIO_ALSA:    return wubu_alsa_cpu_load();
        case AUDIO_PULSE:   return wubu_pulse_cpu_load();
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
    mem_init(1024 * 1024);
    vbe_init(width, height);
    interrupt_init();

    /* Initialize PIT timer for preemptive multitasking (100 Hz) */
    /* Only enable in real bare-metal environment (CAP_SYS_RAWIO) */
    #ifndef WUBU_HOSTED_TEST
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
    pit_shutdown();
    interrupt_shutdown();
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
    /* Simple stub - in future use GAAD translate_init + translate_pixel for nearest mode */
    *out_w = w;
    *out_h = h;
}