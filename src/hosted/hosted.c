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

#include "../egojar.h"
#include "../egojar_net.h"
#include "hosted.h"
#include "hosted_internal.h"
#include "wubu_boot_attest_edr.h"   /* the measured boot -> EDR + peers */

#include "../kernel/vbe.h"
#include "../kernel/memory.h"
#include "../kernel/tasking.h"
#include "../kernel/input.h"
#include "../kernel/wubu_evdev.h"
#include "../runtime/wubu_steaminput.h"
#include "../gui/wm.h"
#include "../gui/shell/startmenu.h"
#include "../gui/wubu_settings.h"
#include "../gui/wubu_a11y.h"
#include "wubu_agent.h"
#include "wubu_agi_play.h"
#include "wubu_game_launch.h"
#include "wubu_ns_kv.h"
#include "wubu_ns_worldspace.h"
#include "../gui/shell/wm_holyc_term.h"
#include "../runtime/wubu_holyd_agi.h"
#include "../gui/shell/desktop.h"
#include "../gui/shell/wubu_screensaver.h"
#include "../gui/shell/wubu_lock_screen.h"
#include "../runtime/styx.h"
#include "../bridge/bridge.h"
#include "../apps/repl.h"
#include "../gui/wubu_screenshot.h"
#include "../gui/shell/wubu_shell_greeter.h"
#include "hosted_fs_persist.h"
#include "../runtime/wubu_onboarding.h"
#include "../gui/wubu_clipboard.h"
#include "../gui/shell/wubu_notify.h"
#include "../gui/wubu_session.h"

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
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

    /* --- Display backend setup (modular, resolution-agnostic) ---
     * Probe the native display resolution first.  If the host can't
     * report a mode, fall back to HOSTED_DEFAULT_W/H.  Then attach the
     * Wayland SHM surface as the VBE backing store so rendering writes
     * directly into the compositor buffer (zero-copy path). */
    {
        int native_w = 0, native_h = 0;
        if (vbe_probe_native_mode(&native_w, &native_h) == 0
            && native_w > 0 && native_h > 0) {
            state->width  = native_w;
            state->height = native_h;
        }
        /* else: keep the -w flag or HOSTED_DEFAULT */
    }

    state->depth = 32;
    state->fb_pitch = state->width * 4;
    state->framebuffer = (uint32_t*)calloc((size_t)state->width * state->height, 4);
    if (!state->framebuffer) { fprintf(stderr, "OOM\n"); return -1; }

    mem_init(8 * 1024 * 1024);
    vbe_init(state->width, state->height);
    input_init();
    /* Steam Input owns the controller->key/mouse map; the evdev pipeline
       feeds it. Both are best-effort: no pad, no permission, a device that
       unplugs -- all quiet no-ops. */
    wubu_si_init();
    wubu_evdev_init();
    /* The sound system's device sink. Every sound call site passes
     * wubu_audio_sink_int16 as its sink; this opens the device it writes to
     * (pulse -> alsa -> wav). Without it the PCM went nowhere. */
    wubu_audio_sink_open();

    /* Load the saved settings and apply the theme BEFORE the WM's first
     * render. Nothing else calls wubu_settings_init() -- the theme's live
     * colour table is only seeded from wubu_theme_set(), which it calls, so
     * skipping this leaves tc()->btn_face / icon_text at zero: a black
     * taskbar, invisible desktop icons and borderless windows. */
    wubu_settings_init();

    /* Bring up the accessibility subsystem when any a11y feature is asked
     * for. The capture harness enables it unconditionally; the product
     * honours the settings instead. */
    {
        const A11ySettings *a = &wubu_settings_get()->a11y;
        if (!wubu_a11y_is_enabled() &&
            (a->high_contrast || a->screen_reader || a->screen_magnifier ||
             a->sticky_keys || a->slow_keys || a->bounce_keys || a->mouse_keys))
            wubu_a11y_set_enabled(true);
    }

    /* Cell 400+401+402: DosGui — Fable windowing agent + desktop + start menu */
    wm_init(state->width, state->height);

    /* Wire the HolyC Terminal to the live ring-0 AGI compiler: the same
     * compile+run path a human uses, but with EDR disclosure of every
     * agent-authored eval (the transparency edict). The terminal module
     * itself stays decoupled -- it only sees the injected function pointer. */
    holyc_term_set_eval(wubu_holyc_eval);
    wubu_holyd_set_pointer_handler(wm_handle_mouse);
    desktop_init();

    /* Hosted session: the host desktop owns idle blanking and session
     * locking.  The in-guest screensaver/lock would only interrupt the
     * user, so they are off here (the bare-metal path keeps them). */
    wubu_screensaver_set_enabled(false);
    wubu_lock_screen_set_enabled(false);

    /* Report the measured boot.  On bare metal the firmware's PCR bank is
     * live and this seals + logs + broadcasts it, with the kernel supplying
     * REAL millisecond offsets for the firmware/loader/kernel stages.
     *
     * A hosted process cannot observe any of that: it starts long after the
     * kernel, has no firmware handoff, and no attestation exists -- so this
     * returns -1 and claims nothing.  The four stage names are still passed
     * (they are the shape of a WuBuOS boot) but the offsets are 0 here, which
     * is exactly what this process knows.  They are NOT measurements. */
    {
        static const char *const kStages[] = { "firmware", "loader",
                                               "kernel", "desktop" };
        static const uint32_t kStarts[] = { 0u, 0u, 0u, 0u };
        wubu_boot_attest_edr_boot((uint64_t)time(NULL), kStages, kStarts, 4,
                                  NULL, NULL, NULL, NULL, NULL);
    }

    /* The federation agent card. The OS never mints an AID -- the federation
     * does (JOINING.md) -- so this loads an enrolled card from the home
     * directory and publishes it under /n/agent, making the sponsor visible
     * to every action the body takes. No card, no identity: nothing is
     * invented here. */
    {
        const char *home = getenv("HOME");
        char agent_path[512];
        if (home) {
            snprintf(agent_path, sizeof(agent_path), "%s/.config/wubu/agent-card.txt", home);
            if (wubu_agent_load(agent_path) == 0)
                wubu_ns_publish_agent();
        }
    }

    /* Publish the body's space-facing surface: what it perceives, who it acts
     * for, and which space instance it reaches. Says "unconfigured" / "not
     * enroled" honestly when neither is set. */
    wubu_ns_publish_worldspace();

    /* Publish the KV namespace so the Brain can mount /n/kv/ over 9P. The
     * tick files appear as the play loop writes them (wubu_ns_kv_touch). */
    wubu_ns_publish_kv();

    /* Start the AGI play session. The loop itself is ticked from the frame
     * loop (hosted_run) -- perceive -> decide -> act -> learn -- so the
     * experience stream lands in /kv/world/tick_<N> and /n/kv mirrors it. */
    wubu_agi_play_start("freeplay", WUBU_GAME_UNKNOWN);
    /* This session runs the loop for the perception/ledger stream only.  It
     * must never synthesize clicks or keys at the user's pointer -- that is
     * what made the shell click, drag and close its own windows. */
    wubu_agi_play_set_input_injection(0);
    wubu_ns_kv_refresh();
    startmenu_init();

    /* Launch the initial windows: the file manager and the HolyC terminal.
     * The registry entry's title is "HolyC Terminal" -- launch_app matches on
     * name OR title, and the old "HolyC REPL" string matched neither, so that
     * window silently never opened. */
    launch_app("My Computer");
    launch_app("HolyC Terminal");

    /* GAP F: the first-boot greeter -- the visible configurator the
     * onboarding decision layer has always targeted but never drawn.  Shown
     * only when no onboarding sentinel exists; completing it persists the
     * sentinel so the next boot goes straight to the desktop.  Supersedes the
     * old passive wubu_welcome dialog (no input handler, own marker file). */
    {
        const char *home = getenv("HOME");
        char obpath[512];
        if (home) {
            snprintf(obpath, sizeof(obpath), "%s/.config/wubu", home);
            mkdir(obpath, 0755);   /* best-effort; complete() writes inside */
            snprintf(obpath, sizeof(obpath), "%s/.config/wubu/onboarding", home);
        } else {
            snprintf(obpath, sizeof(obpath), "/tmp/wubu-onboarding");
        }
        wubu_onboarding_load(obpath);
        if (wubu_onboarding_target() == WUBU_ONBOARD_GREETER)
            wubu_shell_greeter_run();
    }

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

    /* Restore the durable namespace BEFORE adding anything.  A saved copy of a
     * stock file wins over the rebuilt default, so an edit the user made to
     * hello.wubu survives a restart instead of being silently reverted. */
    int restored = fs_persist_load(1);
    if (restored < 0) {
        fprintf(stderr, "WuBuOS: WARNING: filesystem image at %s is unreadable; "
                        "starting from defaults\n",
                fs_persist_path() ? fs_persist_path() : "(no path)");
    } else if (restored > 0) {
        fprintf(stderr, "WuBuOS: filesystem restored (%d entries) from %s\n",
                restored, fs_persist_path());
    }

    if (fs_find("wubu") < 0)  fs_add_dir("wubu");
    if (fs_find("dev") < 0)   fs_add_dir("dev");
    if (fs_find("prog") < 0)  fs_add_dir("prog");
    if (fs_find("cons") < 0)
        fs_add_file("cons", (const uint8_t*)"WuBuOS blob OS -- Styx namespace\n", 33);

    uint8_t demo_wubu[64];
    memset(demo_wubu, 0, sizeof(demo_wubu));
    memcpy(demo_wubu, "WUBU!\0\x01\x02", 8);
    demo_wubu[8] = 1;
    if (fs_find("hello.wubu") < 0)
        fs_add_file("hello.wubu", demo_wubu, sizeof(demo_wubu));

    fprintf(stderr, "WuBuOS: Styx namespace built\n");
    return 0;
}

hosted_state_t *wm_get_hosted_state(void) {
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
    srv.write = styx_write_cb;
    srv.clunk = styx_clunk_cb;
    srv.remove = styx_remove_cb;
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
    int init_ret = hosted_init(&state, argc, argv);
    if (init_ret != 0) return 1;
    int ret = hosted_run(&state);
    hosted_shutdown(&state);
    return ret;
}
#endif
