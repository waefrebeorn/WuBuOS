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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>   /* _NSGetExecutablePath (no /proc on macOS) */
#endif

#include "../kernel/vbe.h"
#include "../kernel/memory.h"
#include "../kernel/tasking.h"
#include "../kernel/input.h"
#include "../kernel/wubu_evdev.h"
#include "../runtime/wubu_steaminput.h"
#include "../gui/wm.h"
#include "../gui/shell/startmenu.h"
#include "../gui/wubu_settings.h"

/* defined below main; launches the live shader companion orb */
static void wubu_hosted_spawn_buddy(void);
#include "wubu_agent.h"
#include "wubu_agi_play.h"
#include "wubu_game_launch.h"
#include "wubu_ns_kv.h"
#include "wubu_ns_worldspace.h"
#include "../gui/shell/holyc_repl.h"
#include "../runtime/holyc_eval.h"
#include "../gui/shell/shell_session.h"
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
#include "../runtime/wubu_container.h"
#include "../audio/wubu_audio.h"
#include "wubu_holyd.h"

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

/* --bench-render N: compose N frames headless and time them (see main). */
static int bench_frames = 0;

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
        /* Measurement entry point: compose N frames with no display at all and
         * time them. The idle desktop repaints in full every frame, and this is
         * how that cost gets measured on a box with no compositor running
         * (`wubu -h --bench-render 240`, WUBU_RENDER_PROBE=1 for the phase
         * split). */
        else if (strcmp(argv[i], "--bench-render") == 0 && i + 1 < argc) {
            bench_frames = atoi(argv[++i]);
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

    wubu_hosted_spawn_buddy();

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
/* The wubu_buddy orb is a live shader companion, not a bitmap sprite. The
 * desktop launches it as its own GLX window so it can run in real time next to
 * the WM, fed by the same sensors. Absent binary or WUBU_BUDDY=0 -> no-op. */
static void wubu_hosted_spawn_buddy(void)
{
    const char *want = getenv("WUBU_BUDDY");
    if (want && strcmp(want, "0") == 0) return;

    /* Find the orb without depending on the CWD: WuBuCompositor forks the
     * client from its own directory, so "./build/wubu-buddy" would miss. */
    char orb[4096];
    if (getenv("WUBU_BUDDY_BIN")) {
        snprintf(orb, sizeof(orb), "%s", getenv("WUBU_BUDDY_BIN"));
    } else {
        char self[4096];
#if defined(__APPLE__)
        /* macOS has no /proc — the WSL-replacement philosophy needs the
         * same derivation on both hosts (see wcc_spawn.c). */
        uint32_t sz = (uint32_t)sizeof(self);
        if (_NSGetExecutablePath(self, &sz) != 0) return;
        char real[4096];
        if (realpath(self, real)) snprintf(self, sizeof(self), "%s", real);
#else
        ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (n <= 0) return;
        self[n] = 0;
#endif
        /* Walk up from the executable to the repo root and look for build/.
         * src/hosted/wubu sits two levels below it; the launcher may also copy
         * or wrap the binary elsewhere. */
        char dir[4096];
        snprintf(dir, sizeof(dir), "%s", self);
        int guard;
        for (guard = 0; guard < 5 && !*orb; ++guard) {
            char *slash = strrchr(dir, '/');
            if (!slash) break;
            *slash = 0;
            if (!*dir) break;
            snprintf(orb, sizeof(orb), "%s/build/wubu-buddy", dir);
            if (access(orb, X_OK) != 0) orb[0] = 0;
        }
        if (!*orb) return;
    }
    if (access(orb, X_OK) != 0) return;

    pid_t pid = fork();
    if (pid != 0) return;                 /* parent keeps running the desktop */
    setsid();                             /* detach from the desktop's group */
    int devnull = open("/dev/null", O_RDWR);
    const char *blog = getenv("WUBU_BUDDY_LOG");
    if (blog && *blog) {
        int lf = open(blog, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); }
    } else if (devnull >= 0) { dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2); }
    execl(orb, "wubu-buddy", (char *)NULL);
    _exit(127);
}
#else
/* The capture harness compiles this TU with -DWUBU_HOSTED_TEST and supplies
 * its own main. Keep the unconditional spawn call above linkable without
 * launching a companion window into the capture session. */
static void wubu_hosted_spawn_buddy(void) {}
#endif

#ifndef WUBU_HOSTED_TEST
int main(int argc, char **argv) {
    hosted_state_t state;
    int init_ret = hosted_init(&state, argc, argv);
    if (init_ret != 0) return 1;
    if (bench_frames > 0) {
        struct timespec a, b;
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < bench_frames; i++) {
            hosted_render_desktop(&state);
            vbe_swap();
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        double ms = (double)(b.tv_sec - a.tv_sec) * 1e3
                  + (double)(b.tv_nsec - a.tv_nsec) / 1e6;
        printf("bench-render: %d frames, %.3f ms/frame\n",
               bench_frames, ms / bench_frames);
        hosted_shutdown(&state);
        return 0;
    }
    int ret = hosted_run(&state);
    hosted_shutdown(&state);
    return ret;
}
#endif
