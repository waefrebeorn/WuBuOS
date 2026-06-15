/*
 * wubu_proton2.h  --  WuBuOS Proton: Real Wine/Proton Container Integration
 *
 * Cell 399: Proton runs as an Arch container with real Wine + DXVK.
 *
 * Architecture:
 *   WuBuOS hosted binary
 *     → creates Arch container (wubu_ct_arch)
 *     → container has Wine + DXVK + Vulkan + GPU passthrough
 *     → Windows .exe runs inside Wine inside the container
 *     → GPU rendered output flows back through shared X11/Wayland
 *
 * This replaces the old VSL-based Proton with a real container approach.
 * The old wubu_proton.c/h stays as the PE parser/API translator.
 * This module adds the container orchestration layer.
 *
 * GPU Passthrough:
 *   - /dev/dri → container (DRM/KMS for Vulkan)
 *   - /dev/nvidia* → container (NVDEC/NVENC)
 *   - /dev/dxg → container (WSL2 paravirt GPU)
 *
 * HID/USB:
 *   - /dev/input/event* → container (keyboard, mouse, gamepad)
 *   - /dev/hidraw* → container (raw HID devices)
 *   - /dev/bus/usb/* → container (USB peripherals: MIDI, controllers)
 *   - /dev/snd/seq → container (MIDI sequencer)
 *   - /dev/js* → container (legacy joystick)
 *
 * DXVK Configuration:
 *   - DXVK_ASYNC=1 (async shader compilation)
 *   - DXVK_HUD=fps,devinfo (debug overlay)
 *   - DXVK_CONFIG_FILE (per-game config)
 *
 * Steam Integration:
 *   - Steam Runtime inside container
 *   - Proton as Steam compatibility tool
 *   - Game library bind-mounted from host
 */
#ifndef WUBU_PROTON2_H
#define WUBU_PROTON2_H

#include <stdint.h>
#include <stdbool.h>
#include "wubu_host_exec.h"
#include "wubu_ramdisk.h"

/* -- Proton Configuration ----------------------------------------- */

#define WUBU_PROTON_MAX_APPS    64
#define WUBU_PROTON_MAX_ARGS    32
#define WUBU_PROTON_PREFIX_LEN  512

typedef enum {
    PROTON_STEAM     = 0,   /* Use Steam's Proton */
    PROTON_GE        = 1,   /* GloriousEggroll custom Proton */
    PROTON_WINE      = 2,   /* Plain Wine (no Proton) */
    PROTON_LUTRIS    = 3,   /* Lutris Wine */
} WubuProtonFlavor;

typedef struct {
    /* Wine/Proton installation */
    WubuProtonFlavor flavor;
    char             wine_path[512];     /* Path to wine binary in container */
    char             proton_path[512];   /* Path to proton script */
    char             prefix[512];        /* WINEPREFIX */
    char             arch[8];            /* win32 or win64 */

    /* DXVK */
    bool             dxvk_enabled;
    bool             dxvk_async;         /* Async shader compilation */
    char             dxvk_hud[64];       /* fps,devinfo,full or off */
    int              dxvk_frame_rate;    /* Frame rate limit (0=unlimited) */

    /* GPU */
    bool             gpu_passthrough;    /* /dev/dri + /dev/nvidia */
    bool             nvapi;              /* NVIDIA API (dxvk-nvapi) */
    int              gpu_device;        /* GPU index (0=primary) */

    /* Display */
    bool             fullscreen;
    int              width, height;      /* 0 = use desktop resolution */
    bool             virtual_desktop;    /* Wine virtual desktop mode */
    int              vd_width, vd_height;

    /* Audio */
    bool             pulseaudio;         /* PipeWire/PulseAudio passthrough */
    bool             alsa;               /* ALSA passthrough */

    /* Input */
    bool             xinput;             /* XInput controller support */
    bool             dinput;             /* DirectInput */
    bool             raw_input;          /* Raw input (evdev) */
    bool             sdl;                /* SDL2 gamepad */

    /* Performance */
    bool             esync;              /* Eventfd-based synchronization */
    bool             fsync;              /* Futex-based synchronization */
    bool             fsr;                /* AMD FidelityFX Super Resolution */
    int              cpu_affinity;       /* CPU core mask */

    /* Debug */
    bool             debug_wine;         /* WINEDEBUG output */
    bool             debug_d3d;          /* D3D debug */
    bool             debug_proxy;        /* Log API calls */
} WubuProtonConfig;

/* -- Windows App Descriptor --------------------------------------- */

typedef struct {
    char     name[128];
    char     exe_path[512];      /* Path inside container */
    char     work_dir[512];      /* Working directory */
    char     args[1024];         /* Command line arguments */
    char     icon_path[512];     /* Icon for taskbar/launcher */
    uint32_t app_id;             /* Steam AppID (0 = non-Steam) */

    /* Overrides */
    WubuProtonConfig config;     /* Per-app config (overrides global) */
    bool             use_global_config;

    /* State */
    bool     installed;
    bool     running;
    int      pid;                /* Container PID */
    int      exit_code;
} WubuProtonApp;

/* -- Proton Manager ----------------------------------------------- */

typedef struct {
    WubuProtonConfig   global;
    WubuProtonApp      apps[WUBU_PROTON_MAX_APPS];
    int                n_apps;

    /* Container */
    WubuRamdisk       *ramdisk;          /* Arch root mount */
    WubuCt            *container;        /* Running container */
    bool               container_running;

    /* GPU */
    int                drm_fd;           /* DRM file descriptor */
    char               gpu_pci[32];      /* PCI address of GPU */

    /* Input */
    int                input_fds[16];    /* /dev/input/event* fds */
    int                n_input_fds;
    int                hidraw_fds[8];    /* /dev/hidraw* fds */
    int                n_hidraw_fds;
    int                js_fds[4];        /* /dev/js* fds */
    int                n_js_fds;
    int                midi_fds[4];      /* /dev/snd/seq, /dev/midi* */
    int                n_midi_fds;
    int                usb_fds[8];       /* /dev/bus/usb/* fds */
    int                n_usb_fds;

    /* Stats */
    uint64_t apps_launched;
    uint64_t apps_exited;
    uint64_t frames_rendered;
} WubuProtonManager;

/* ==================================================================
 *  API: Proton Manager Lifecycle
 * ================================================================== */

/* Initialize Proton manager (creates Arch container with Wine) */
WubuProtonManager *wubu_proton_mgr_create(const WubuProtonConfig *config);
void               wubu_proton_mgr_destroy(WubuProtonManager *mgr);

/* Start the Proton container (Arch + Wine + DXVK) */
int  wubu_proton_start(WubuProtonManager *mgr);

/* Stop the Proton container */
void wubu_proton_stop(WubuProtonManager *mgr);

/* Check if Proton is running */
bool wubu_proton_is_running(const WubuProtonManager *mgr);

/* ==================================================================
 *  API: App Management
 * ================================================================== */

/* Register a Windows app */
int  wubu_proton_add_app(WubuProtonManager *mgr, const WubuProtonApp *app);

/* Launch a Windows app */
int  wubu_proton_launch(WubuProtonManager *mgr, int app_idx);

/* Launch by name */
int  wubu_proton_launch_name(WubuProtonManager *mgr, const char *name);

/* Terminate a running app */
int  wubu_proton_terminate(WubuProtonManager *mgr, int app_idx);

/* Wait for app to exit */
int  wubu_proton_wait(WubuProtonManager *mgr, int app_idx);

/* ==================================================================
 *  API: GPU Passthrough
 * ================================================================== */

/* Detect available GPUs */
int  wubu_gpu_detect(char *name, int name_len, char *pci, int pci_len);

/* Open GPU device for passthrough */
int  wubu_gpu_open(const char *pci);

/* Configure DXVK for a specific app */
int  wubu_proton_dxvk_config(WubuProtonManager *mgr, int app_idx,
                               const char *config_str);

/* ==================================================================
 *  API: HID/USB Input
 * ================================================================== */

/* Enumerate input devices (keyboards, mice, gamepads) */
int  wubu_hid_enumerate(char names[][64], int *types, int max);

/* Open an input device for passthrough */
int  wubu_hid_open(const char *path);

/* Enumerate USB devices */
int  wubu_usb_enumerate(char paths[][256], char names[][64], int max);

/* Open a USB device for passthrough */
int  wubu_usb_open(const char *path);

/* Enumerate MIDI devices */
int  wubu_midi_enumerate(char names[][64], int max);

/* Open a MIDI device */
int  wubu_midi_open(const char *path);

/* ==================================================================
 *  API: Container Integration
 * ================================================================== */

/* Get the container (for advanced configuration) */
WubuCt *wubu_proton_container(WubuProtonManager *mgr);

/* Get the ramdisk (for file operations) */
WubuRamdisk *wubu_proton_ramdisk(WubuProtonManager *mgr);

/* Mount a host directory into the Proton prefix */
int  wubu_proton_mount(WubuProtonManager *mgr,
                        const char *host_path, const char *guest_path);

/* Run a command inside the Proton container */
int  wubu_proton_exec(WubuProtonManager *mgr, const char *cmd);

/* ==================================================================
 *  API: Diagnostics
 * ================================================================== */

void wubu_proton_dump(const WubuProtonManager *mgr);
int  wubu_proton_verify_installation(const WubuProtonManager *mgr);

#endif /* WUBU_PROTON2_H */
