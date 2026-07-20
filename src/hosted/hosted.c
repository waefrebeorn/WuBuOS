/*
 * hosted.c — WuBuOS Hosted Mode Launcher (Inferno emu-style) — facade
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
 * This file is the thin orchestration facade. The heavy concerns are split
 * into self-contained modules (C11 opaque-safe, no god headers):
 *   hosted_render.c  -- frame composition + input routing
 *   hosted_pe.c      -- Windows/PE launch executor (Proton + cgroup)
 *   hosted_run.c     -- run loop, shutdown, blit, mode + accessors
 *   hosted_styxfs.c  -- in-memory Styx namespace filesystem
 *   hosted_wayland*.c -- Wayland client sub-systems (SHM/input/surface)
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
#include "../gui/dosgui_wm_holyc_term.h"
#include "../runtime/wubu_holyc_agi.h"
#include "../gui/dosgui_desktop.h"
#include "../runtime/styx.h"
#include "../bridge/bridge.h"
#include "../apps/repl.h"
#include "../gui/wubu_screenshot.h"
#include "../gui/wubu_welcome.h"
#include "../gui/wubu_clipboard.h"
#include "../gui/wubu_notify.h"
#include "../gui/wubu_session.h"
#include "../gui/wubu_settings.h"

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

#include "xdg-shell-client.header"
#include "primary-selection-client.header"

/* Launcher-core global: the live hosted state, set in hosted_init and
 * read by the Wayland subsystem (hosted_wayland.c). */
hosted_state_t *g_hosted_state = NULL;

/* ═══════════════════════════════════════════════════════════════
 * REPL Callback
 * ═══════════════════════════════════════════════════════════════ */

static void repl_launch_callback(void) {
    if (g_hosted_state) {
        repl_start(g_hosted_state->width, g_hosted_state->height);
    }
}

/* ══════════════════════════════════════════════════════════════════
 * Public API Implementation
 * ══════════════════════════════════════════════════════════════════ */

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

    /* Wire the HolyC Terminal to the live ring-0 AGI compiler: the same
     * compile+run path a human uses, but with EDR disclosure of every
     * agent-authored eval (the transparency edict). The terminal module
     * itself stays decoupled -- it only sees the injected function pointer. */
    holyc_term_set_eval(wubu_holyc_eval);
    wubu_holyd_set_pointer_handler(dosgui_wm_handle_mouse);
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

hosted_state_t *dosgui_wm_get_hosted_state(void) {
    return g_hosted_state;
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
