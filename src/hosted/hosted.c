/*
 * hosted.c — WuBuOS Hosted Mode Launcher (Inferno emu-style)
 *
 * WuBuOS as a clickable Linux binary — the "blob OS".
 * Runs as a regular Linux program via Wayland window.
 * Full OS environment: VBE framebuffer, kernel services,
 * Styx/9P namespace on Unix socket.
 *
 * Cell 200: ZealOS kernel runs in-process.
 *   - Kernel subsystems: mem_init, vbe_init, tasking
 *   - GUI shell: WM, desktop, taskbar, start menu
 *   - Input routing: Wayland -> WM -> focused window
 *   - Render pipeline: desktop + windows + taskbar -> vbe_swap -> Wayland blit
 *
 * Build: make hosted  ->  src/hosted/wubu
 *
 * Wayland protocol: xdg-shell for window management.
 * Input: wl_keyboard + wl_pointer -> KeyEvent/MouseEvent -> kernel queue.
 * Render: SHM buffers -> wl_surface attach/commit.
 */

#include "hosted.h"
#include "hosted_internal.h"

#include "../kernel/vbe.h"
#include "../kernel/memory.h"
#include "../kernel/tasking.h"
#include "../kernel/input.h"
#include "../gui/wm.h"
#include "../gui/dosgui_startmenu.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_desktop.h"
#include "../gui/dosgui_startmenu.h"
#include "../runtime/styx.h"
#include "../bridge/bridge.h"
#include "../apps/repl.h"
#include "../gui/wubu_screenshot.h"
#include "../gui/wubu_welcome.h"
#include "../gui/wubu_clipboard.h"
#include "../gui/wubu_notify.h"
#include "../gui/wubu_session.h"
#include "../gui/wubu_settings.h"
#include "../gui/wubu_proton.h"
#include "../runtime/wubu_container.h"
#include "../runtime/wubu_ct_isolate.h"

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <poll.h>
#include <math.h>

#include "../gui/wubu_theme.h"

#include "xdg-shell-client.header"
#include "primary-selection-client.header"

/* Launcher-core global: the live hosted state, set in hosted_init and
 * read by the Wayland subsystem (hosted_wayland.c). */
hosted_state_t *g_hosted_state = NULL;

/* ═══════════════════════════════════════════════════════════════
 * Filesystem helpers
 * ═══════════════════════════════════════════════════════════════ */


/* ══════════════════════════════════════════════════════════════════
 * REPL Callback
 * ══════════════════════════════════════════════════════════════════ */

static void repl_launch_callback(void) {
    if (g_hosted_state) {
        repl_start(g_hosted_state->width, g_hosted_state->height);
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Render desktop to VBE back buffer
 * ═══════════════════════════════════════════════════════════════ */

static void render_desktop(hosted_state_t *state) {
    dosgui_desktop_render(NULL, state->width, state->height);
    if (dosgui_startmenu_is_open()) {
        dosgui_startmenu_render(NULL, state->width, state->height);
    }
    dosgui_desktop_tick();
}

/* ══════════════════════════════════════════════════════════════════
 * Kernel input dispatch (Cell 202)
 * ══════════════════════════════════════════════════════════════════ */

static void input_dispatch(void) {
    KeyEvent kev;
    while (input_key_poll(&kev)) {
        dosgui_wm_handle_key(kev.keycode, kev.modifiers);
    }
    MouseEvent mev;
    while (input_mouse_poll(&mev)) {
        int kind = mev.buttons ? 1 : 2; /* 1=down, 2=up */
        if (mev.buttons & 2) kind = 0; /* 0=move while dragging */
        dosgui_wm_handle_mouse(mev.x, mev.y, mev.buttons, kind);
    }
}

/* ══════════════════════════════════════════════════════════════════
 * Public API Implementation
 * ══════════════════════════════════════════════════════════════════ */

/* Forward decl: PE executor registered with the launch layer (defined below). */
static int hosted_pe_executor(const void *data, size_t size, const char *cmdline);

int hosted_init(hosted_state_t *state, int argc, char **argv) {
    g_hosted_state = state;
    /* Register the real Proton PE loader with the launch layer (SteamOS
     * strategy: Windows runs in a container, never an NT-kernel reimpl). */
    wubu_launch_set_pe_executor(hosted_pe_executor);
    memset(state, 0, sizeof(*state));
    state->width = HOSTED_DEFAULT_W;
    state->height = HOSTED_DEFAULT_H;
    state->mode = HMODE_GUI;
    state->running = true;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 && i + 2 < argc) {
            state->width = atoi(argv[++i]); state->height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0) state->mode = HMODE_TEMPLE;
        else if (strcmp(argv[i], "-c") == 0) state->mode = HMODE_CONSOLE;
        else if (strcmp(argv[i], "-h") == 0) state->mode = HMODE_HEADLESS;
        else if (strcmp(argv[i], "-f") == 0) state->fullscreen = true;
        else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            state->screenshot_path = argv[++i];
            state->mode = HMODE_HEADLESS;
        }
    }

    state->depth = 32;
    state->fb_pitch = state->width * 4;
    state->framebuffer = (uint32_t*)calloc((size_t)state->width * state->height, 4);
    if (!state->framebuffer) { fprintf(stderr, "OOM\n"); return -1; }

    mem_init(1024 * 1024);
    fprintf(stderr, "DEBUG: mem_init done\n");
    vbe_init(state->width, state->height);
    fprintf(stderr, "DEBUG: vbe_init done\n");
    input_init();
    fprintf(stderr, "DEBUG: input_init done\n");

    /* Cell 400+401+402: DosGui — Fable windowing agent + desktop + start menu */
    dosgui_wm_init(state->width, state->height);
    fprintf(stderr, "DEBUG: dosgui_wm_init done\n");
    dosgui_desktop_init();
    fprintf(stderr, "DEBUG: dosgui_desktop_init done\n");
    dosgui_startmenu_init();
    fprintf(stderr, "DEBUG: dosgui_startmenu_init done\n");

    /* First-run welcome dialog (shown once, creates marker file) */
    wubu_welcome_init();

    /* Launch initial windows */
    fprintf(stderr, "DEBUG: About to launch initial windows\n");
    dosgui_launch_app("My Computer");
    fprintf(stderr, "DEBUG: Launched My Computer\n");
    dosgui_launch_app("HolyC REPL");
    fprintf(stderr, "DEBUG: Launched HolyC REPL\n");

    fprintf(stderr, "WuBuOS: kernel + GUI shell initialized\n");

    /* Only connect to Wayland if not in headless mode */
    if (state->mode != HMODE_HEADLESS) {
        if (hosted_wl_connect(state) != 0) {
            free(state->framebuffer);
            return -1;
        }
    }

    /* Initialize clipboard manager */
    if (g_wl.seat && g_wl.data_device_manager) {
        wubu_clipboard_init(g_wl.seat);
    }

    /* Initialize screenshot subsystem */
    wubu_screenshot_init();

    fs_add_dir("wubu");
    fs_add_dir("dev");
    fs_add_dir("prog");
    fs_add_file("cons", (const uint8_t*)"WuBuOS blob OS -- Styx namespace\n", 33);

    uint8_t demo_wubu[64];
    memset(demo_wubu, 0, sizeof(demo_wubu));
    memcpy(demo_wubu, "WUBU!\0\x01\x02", 8);
    demo_wubu[8] = 1;
    fs_add_file("hello.wubu", demo_wubu, sizeof(demo_wubu));

    fprintf(stderr, "WuBuOS: Styx namespace built\n");
    return 0;
}

int hosted_run(hosted_state_t *state) {
    fprintf(stderr, "WuBuOS running. Mode: %s\n",
            state->mode == HMODE_GUI ? "GUI" :
            state->mode == HMODE_TEMPLE ? "Temple" :
            state->mode == HMODE_CONSOLE ? "Console" : "Headless");

    struct timespec last;
    clock_gettime(CLOCK_MONOTONIC, &last);
    const long frame_ns = 1000000000L / 30;

    bool screenshot_requested = (state->screenshot_path != NULL);
    int frame_count = 0;
    fprintf(stderr, "DEBUG: Entering run loop, screenshot_requested=%d, mode=%d\n", screenshot_requested, state->mode);

    while (state->running) {
        fprintf(stderr, "DEBUG: run loop iteration, running=%d, frame_count=%d\n", state->running, frame_count);
        hosted_wl_dispatch();

        input_dispatch();

        if (state->framebuffer) {
            if (state->mode == HMODE_GUI) {
                render_desktop(state);
                vbe_swap();
                VBEState *vs = vbe_state();
                if (vs && vs->fb) {
                    memcpy(state->framebuffer, vs->fb,
                           (size_t)state->width * state->height * 4);
                }
            } else if (state->mode == HMODE_TEMPLE) {
                for (int i = 0; i < state->width * state->height; i++)
                    state->framebuffer[i] = 0x00000000;
            } else {
                /* HMODE_HEADLESS or HMODE_CONSOLE - render desktop for screenshot */
                render_desktop(state);
                vbe_swap();
                VBEState *vs = vbe_state();
                if (vs && vs->fb) {
                    memcpy(state->framebuffer, vs->fb,
                           (size_t)state->width * state->height * 4);
                }
            }
        }

        if (g_wl.surface) {
            hosted_wl_frame_render();
        }

        /* If screenshot requested in headless mode, render a couple frames and exit */
        if (screenshot_requested && state->mode == HMODE_HEADLESS) {
            frame_count++;
            fprintf(stderr, "DEBUG: frame_count=%d, will exit at 2\n", frame_count);
            if (frame_count >= 2) {  /* Render a couple frames to ensure desktop is drawn */
                state->running = false;
                break;
            }
        }

        /* If headless mode without screenshot, just render once and exit */
        if (state->mode == HMODE_HEADLESS && !screenshot_requested) {
            frame_count++;
            fprintf(stderr, "DEBUG: headless no screenshot, frame_count=%d\n", frame_count);
            if (frame_count >= 1) {
                state->running = false;
                break;
            }
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - last.tv_sec) * 1000000000L +
                       (now.tv_nsec - last.tv_nsec);
        if (elapsed < frame_ns) {
            struct timespec slp = {0, frame_ns - elapsed};
            nanosleep(&slp, NULL);
        }
        clock_gettime(CLOCK_MONOTONIC, &last);
    }
    return 0;
}

void hosted_shutdown(hosted_state_t *state) {
    fprintf(stderr, "WuBuOS shutdown...\n");

    /* Save screenshot if requested */
    if (state->screenshot_path && state->framebuffer) {
        FILE *f = fopen(state->screenshot_path, "wb");
        if (f) {
            /* Write simple PPM format */
            fprintf(f, "P6\n%d %d\n255\n", state->width, state->height);
            uint32_t *fb = state->framebuffer;
            for (int y = 0; y < state->height; y++) {
                for (int x = 0; x < state->width; x++) {
                    uint32_t c = fb[y * state->width + x];
                    fputc((c >> 16) & 0xFF, f);  /* R */
                    fputc((c >> 8) & 0xFF, f);   /* G */
                    fputc(c & 0xFF, f);          /* B */
                }
            }
            fclose(f);
            fprintf(stderr, "Screenshot saved to %s\n", state->screenshot_path);
        }
    }

    dosgui_wm_shutdown();
    dosgui_desktop_shutdown();
    dosgui_startmenu_shutdown();
    wubu_screenshot_shutdown();
    wubu_clipboard_shutdown();
    wubu_notify_shutdown();
    wubu_session_shutdown();
    wubu_settings_shutdown();
    vbe_shutdown();
    input_shutdown();

    hosted_wl_disconnect();

    if (state->framebuffer) free(state->framebuffer);
    memset(state, 0, sizeof(*state));
}

void hosted_blit(hosted_state_t *state) {
    if (!state) return;
    hosted_wl_frame_render();
}

void hosted_set_mode(hosted_state_t *state, hosted_mode_t mode) {
    state->mode = mode;
    fprintf(stderr, "Mode: %s\n", mode == HMODE_GUI ? "GUI" :
            mode == HMODE_TEMPLE ? "Temple" : "Other");
}

hosted_state_t *dosgui_wm_get_hosted_state(void) {
    return g_hosted_state;
}

/* PE executor registered with wubu_launch_windows (dependency inversion).
 * Writes the PE bytes to a temp file and launches via the GUI Proton manager
 * (real Windows-compat path through the SteamOS-style container). Returns a
 * process handle or -1. */
static int hosted_pe_executor(const void *data, size_t size, const char *cmdline) {
    char tmppath[512];
    snprintf(tmppath, sizeof(tmppath), "/tmp/wubu-pe-%d.bin", (int)getpid());
    FILE *f = fopen(tmppath, "wb");
    if (!f) return -1;
    fwrite(data, 1, size, f);
    fclose(f);

    /* Isolate the foreign process in a cgroup v2 sandbox (Pressure-Vessel
     * analog). Best-effort: if cgroup setup fails (e.g. no privileges in
     * WSL), we still proceed -- the launch is the critical path. */
    char cg_path[256] = {0};
    wubu_ct_cgroup_create("wubu-foreign", cg_path, sizeof(cg_path));
    if (cg_path[0]) {
        wubu_ct_cgroup_set_memory(cg_path, 2048);   /* 2 GB cap */
        wubu_ct_cgroup_set_pids(cg_path, 4096);     /* process cap */
    }

    /* Launch via the Proton manager (prefix_id NULL -> default prefix). */
    char *argv[2] = { (char *)cmdline, NULL };
    int rc = wubu_proton_launch_with_prefix(tmppath, NULL, cmdline ? argv : NULL);
    unlink(tmppath);
    return rc;
}

int hosted_styx_init(hosted_state_t *state, const char *socket_path) {
    /* socket_path is reserved for future use with local Styx sockets */
    styx_server_t srv;
    styx_init(&srv);
    srv.attach = styx_attach_cb;
    srv.walk = styx_walk_cb;
    srv.open = styx_open_cb;
    srv.read = styx_read_cb;
    srv.stat = styx_stat_cb;
    (void)state;
    return 0;
}

int hosted_styx_register_wubu(hosted_state_t *state,
                               const char *name,
                               const uint8_t *data, uint32_t size) {
    /* state is available for future per-state registration tracking */
    return fs_add_file(name, data, size);
}

void hosted_fs_reset(void) { fs_reset(); }

int hosted_kernel_ready(void) {
    VBEState *vs = vbe_state();
    return (vs && vs->fb && vs->back && vs->width > 0) ? 1 : 0;
}

int hosted_wm_has_windows(void) {
    return dosgui_wm_window_count() > 0 ? 1 : 0;
}

#ifndef WUBU_HOSTED_TEST
int main(int argc, char **argv) {
    hosted_state_t state;
    fprintf(stderr, "DEBUG: Starting main\n");
    int init_ret = hosted_init(&state, argc, argv);
    fprintf(stderr, "DEBUG: hosted_init returned %d\n", init_ret);
    if (init_ret != 0) return 1;
    int ret = hosted_run(&state);
    fprintf(stderr, "DEBUG: hosted_run returned %d\n", ret);
    hosted_shutdown(&state);
    return ret;
}
#endif
