/*
 * wubu_metal.h  --  WuBuOS Bare-Metal Boot + WSL2 GUI Abstraction
 *
 * Cell 400: The bridge between hosted and bare-metal.
 *
 * ==================================================================
 *  ARCHITECTURE OVERVIEW
 * ==================================================================
 *
 *  THREE BOOT PATHS:
 *
 *  1. HOSTED (Linux binary):
 *     wubu → X11/Wayland window → VBE framebuffer → GUI shell
 *     Input: evdev → X11 events
 *     Audio: PulseAudio/PipeWire
 *     Used for: development, WSL2 guest, container mode
 *
 *  2. BARE-METAL (Linux kernel + initramfs):
 *     GRUB/systemd-boot → bzImage + initramfs → wubu binary
 *     Display: DRM/KMS direct (no X11)
 *     Input: evdev directly
 *     Audio: ALSA → JACK → PipeWire
 *     Used for: real hardware, SteamOS-like experience
 *
 *  3. WSL2 DISTRO (Windows):
 *     wsl --install WuBuOS → LxssManager → wubu
 *     Display: wslg (Wayland + RDP bridge to Windows desktop)
 *     Input: wslg input bridge
 *     Audio: wslg PulseAudio bridge
 *     Used for: Windows users, development
 *
 *  ==================================================================
 *  BARE-METAL BOOT SEQUENCE
 *  ==================================================================
 *
 *  1. BIOS/UEFI → GRUB (or systemd-boot)
 *  2. GRUB loads bzImage + initramfs.img
 *  3. Kernel boots, mounts initramfs as root
 *  4. initramfs/init → /wubu (our binary)
 *  5. wubu:
 *     a. Detects hardware (GPU, input, audio, USB)
 *     b. Loads DRM/KMS driver for GPU
 *     c. Sets display mode via drmModeSetCrtc
 *     d. Opens evdev devices for input
 *     e. Initializes ALSA/JACK for audio
 *     f. Starts GUI shell (desktop, taskbar, etc.)
 *     g. Starts 9P namespace server
 *  6. User interacts with WuBuOS desktop
 *
 *  ==================================================================
 *  WSL2 GUI ABSTRACTION
 *  ==================================================================
 *
 *  WSL2 uses Microsoft's WSLg for GUI:
 *  - Weston compositor runs inside WSL2
 *  - RDP protocol bridges to Windows desktop
 *  - Wayland apps work transparently
 *  - Weston uses DRM/KMS via /dev/dxg (paravirt GPU)
 *
 *  WuBuOS WSL2 integration:
 *  - Detect WSL2 environment (Microsoft in hypervisor bare mode)
 *  - Use /dev/dxg for GPU (WSL2 paravirt)
 *  - Connect to Weston Wayland socket
 *  - Use wslg PulseAudio socket for audio
 *  - Input via wslg input bridge (evdev → RDP)
 *
 *  This gives NATIVE PERFORMANCE for:
 *  - Wayland/Weston compositor
 *  - GPU-accelerated apps (via /dev/dxg → DXGI → GPU)
 *  - Audio (via wslg PulseAudio bridge)
 *
 *  ==================================================================
 *  DETECTION MATRIX
 *  ==================================================================
 *
 *  | Feature     | Hosted | Bare-Metal | WSL2   |
 *  |-------------|--------|------------|--------|
 *  | Display     | X11    | DRM/KMS    | Wayland|
 *  | GPU         | XGL    | libdrm     | /dev/dxg|
 *  | Input       | evdev  | evdev      | wslg   |
 *  | Audio       | Pulse  | ALSA/JACK  | wslg   |
 *  | Boot        | exec   | GRUB/init  | Lxss   |
 *  | Container   | native | native     | native |
 *  | Resolution  | XRandR | drmModeSet | wslg   |
 *
 *  ==================================================================
 *  UNIFIED DISPLAY API
 *  ==================================================================
 *
 *  wubu_display_t abstracts all three paths:
 *  - wubu_disp_init() auto-detects environment
 *  - wubu_disp_flip() presents framebuffer
 *  - wubu_disp_set_mode() changes resolution
 *  - wubu_disp_handle_input() processes events
 *
 *  The GUI shell calls wubu_display, never X11/DRM directly.
 */
#ifndef WUBU_METAL_H
#define WUBU_METAL_H

#include <stdint.h>
#include <stdbool.h>

/* -- Boot Environment Detection ----------------------------------- */

typedef enum {
    WUBU_ENV_UNKNOWN  = 0,
    WUBU_ENV_HOSTED   = 1,  /* Linux binary (X11/Wayland) */
    WUBU_ENV_METAL    = 2,  /* Bare-metal boot */
    WUBU_ENV_WSL2     = 3,  /* Windows Subsystem for Linux 2 */
    WUBU_ENV_MACOS    = 4,  /* macOS Virtualization.framework */
} WubuBootEnv;

/* -- Display Backend ---------------------------------------------- */

typedef enum {
    DISP_AUTO     = 0,   /* Auto-detect */
    DISP_X11      = 1,   /* X11 (hosted) */
    DISP_WAYLAND  = 2,   /* Wayland (WSLg, Weston) */
    DISP_DRM      = 3,   /* DRM/KMS (bare-metal, SteamOS) */
    DISP_VBE      = 4,   /* VBE (legacy BIOS) */
} WubuDispBackend;

typedef struct {
    WubuDispBackend backend;
    int             width, height;
    int             refresh_hz;
    bool            fullscreen;

    /* DRM/KMS (bare-metal) */
    int             drm_fd;
    uint32_t        crtc_id, connector_id, fb_id;
    uint32_t       *fb_map;          /* mmap'd framebuffer */

    /* WSL2 */
    char            wayland_socket[256];
    char            wslg_pulse[256];

    /* X11 */
    void           *x11_display;
    unsigned long   x11_window;
    void           *x11_gc;

    /* Common */
    uint32_t       *vbe_back;        /* Render target */
    bool            needs_flip;
} WubuDisplay;

/* -- Input Backend ------------------------------------------------ */

typedef enum {
    INPUT_AUTO    = 0,
    INPUT_EVDEV   = 1,   /* Direct evdev (metal, hosted) */
    INPUT_X11     = 2,   /* X11 events (hosted) */
    INPUT_WSLG    = 3,   /* WSL2 input bridge */
    INPUT_HIDRAW  = 4,   /* Raw HID (gamepads, MIDI) */
} WubuInputBackend;

typedef struct {
    WubuInputBackend backend;

    /* evdev devices */
    int              kbd_fd;
    int              mouse_fd;
    int              touch_fd;
    int              gamepad_fds[4];
    int              n_gamepads;

    /* MIDI */
    int              midi_fds[4];
    int              n_midi;

    /* USB HID */
    int              hidraw_fds[8];
    int              n_hidraw;

    /* X11 (hosted) */
    void            *x11_display;
} WubuInput;

/* -- Audio Backend ------------------------------------------------ */

typedef enum {
    AUDIO_AUTO     = 0,
    AUDIO_PULSE    = 1,   /* PulseAudio (hosted, WSLg) */
    AUDIO_ALSA     = 2,   /* ALSA direct (metal) */
    AUDIO_JACK     = 3,   /* JACK (pro audio, Ardour) */
    AUDIO_PIPEWIRE = 4,   /* PipeWire (modern Linux) */
} WubuAudioBackend;

typedef struct {
    WubuAudioBackend backend;
    int              sample_rate;
    int              channels;
    int              buffer_frames;

    /* ALSA */
    int              alsa_pcm_fd;
    void            *alsa_handle;    /* snd_pcm_t* */

    /* JACK */
    void            *jack_client;    /* jack_client_t* */
    int              jack_nports;
    void            *jack_ports[16]; /* jack_port_t** */

    /* PulseAudio / PipeWire */
    void            *pa_handle;      /* pa_context* */

    /* Ring buffer for GUI audio */
    float           *render_buf;
    int              render_buf_size;
} WubuAudio;

/* ==================================================================
 *  API: Boot Environment
 * ================================================================== */

/* Detect current boot environment */
WubuBootEnv wubu_detect_env(void);

/* Get environment name */
const char *wubu_env_name(WubuBootEnv env);

/* Is running on bare-metal? */
bool wubu_is_metal(void);

/* Is running under WSL2? */
bool wubu_is_wsl2(void);

/* ==================================================================
 *  API: Display
 * ================================================================== */

/* Initialize display (auto-detects backend) */
int  wubu_disp_init(int width, int height);
void wubu_disp_shutdown(void);

/* Get display state */
WubuDisplay *wubu_disp_state(void);

/* Set resolution */
int  wubu_disp_set_mode(int width, int height, int refresh_hz);

/* Present framebuffer (flip/swap) */
void wubu_disp_flip(void);

/* Handle display events */
void wubu_disp_poll_events(void);

/* Get current backend */
WubuDispBackend wubu_disp_current(void);

/* Force a specific backend */
int  wubu_disp_force(WubuDispBackend backend);

/* ==================================================================
 *  API: Input
 * ================================================================== */

/* Initialize input (auto-detect + open evdev devices) */
int  wubu_input_init(void);
void wubu_input_shutdown(void);

/* Get input state */
WubuInput *wubu_input_state(void);

/* Poll all input events (returns count) */
int  wubu_input_poll(void);

/* Is a specific key pressed? */
int  wubu_input_key_down(uint32_t key);

/* Get mouse position */
void wubu_input_mouse_pos(int *x, int *y);

/* Enumerate gamepads */
int  wubu_input_gamepads(char names[][64]);

/* ==================================================================
 *  API: Audio
 * ================================================================== */

/* Initialize audio */
int  wubu_audio_init(int sample_rate, int channels, int buffer_frames);
void wubu_audio_shutdown(void);

/* Get audio state */
WubuAudio *wubu_audio_state(void);

/* Submit audio buffer for playback */
void wubu_audio_submit(const float *buf, int frames);

/* Get current CPU load (0.0 - 1.0) */
double wubu_audio_cpu_load(void);

/* ==================================================================
 *  API: WSL2 Specific
 * ================================================================== */

/* Initialize WSL2 display (connect to Weston) */
int  wubu_wsl2_disp_init(void);

/* Initialize WSL2 PulseAudio */
int  wubu_wsl2_audio_init(void);

/* Get WSLg socket paths */
const char *wubu_wsl2_wayland_path(void);
const char *wubu_wsl2_pulse_path(void);

/* ==================================================================
 *  API: Bare-Metal Boot
 * ================================================================== */

/* Initialize from bare-metal (DRM + evdev + ALSA) */
int  wubu_metal_init(int width, int height);

/* Main loop for bare-metal */
void wubu_metal_run(void);

/* Shutdown bare-metal */
void wubu_metal_shutdown(void);

/* ==================================================================
 *  Resolution Scaling (GAAD)
 * ==================================================================
 *
 *  The display module uses GAAD for resolution scaling:
 *  - 640×480 → any resolution via golden subdivision
 *  - φ-based zoom levels for UI scaling
 *  - Per-backend mode setting
 */

/* Get supported modes */
int  wubu_disp_get_modes(int *widths, int *heights, int max);

/* Find nearest GAAD-aligned resolution */
void wubu_disp_gaad_nearest(int w, int h, int *out_w, int *out_h);

#endif /* WUBU_METAL_H */
