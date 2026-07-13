/*
 * wubu_metal.c  --  WuBuOS Bare-Metal Boot + WSL2 GUI Abstraction
 *
 * Cell 400: Implementation of unified display/input/audio across three boot paths.
 *
 * Build: make all (add to Makefile)
 */

#include "wubu_metal.h"
#include "wubu_metal_audio.h"
#include "wubu_metal_drm.h"
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
 *  DRM ATOMIC COMMIT HELPERS
 *  Extracted to wubu_metal_drm.c (self-contained backend TU). The
 *  wubu_disp_* dispatch below calls wubu_drm_* declared in
 *  wubu_metal_drm.h. No libdrm types live in this facade.
 * ------------------------------------------------------------------ */

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

/* Extracted to wubu_metal_drm.c (self-contained backend TU). The
 * wubu_disp_* dispatch below calls the wubu_drm_* entry points
 * declared in wubu_metal_drm.h. No libdrm types live in this facade. */


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
