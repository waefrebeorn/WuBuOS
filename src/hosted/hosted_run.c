/*
 * hosted_run.c -- WuBuOS hosted-mode run loop, shutdown, blit + accessors
 *
 * Self-contained concern split out of hosted.c: the frame loop, the teardown
 * sequence, the blit shim, mode switching, and the behavioral-test accessors
 * (kernel-ready / wm-has-windows). Orchestrates the render + input sub-modules
 * and the Wayland blit via hosted_internal.h entry points.
 *
 * Depends on the VBE, WM, screenshot/clipboard/notify/session/settings public
 * APIs and the Wayland blit hooks. No PE launch, no init, no Styx wiring.
 */

#include "hosted_internal.h"
#include "hosted_fs_persist.h"
#include "../kernel/wubu_drv_thermal.h"
#include "../kernel/wubu_evdev.h"
#include "../gui/buddy_sprite.h"
#include "../gui/shell/parental_guard.h"   /* family limits lock the session when the window closes */
#include "hosted_agent_channel.h"

#include "../kernel/vbe.h"
#include "../gui/wm.h"
#include "../runtime/wubu_agi_play.h"
#include "../gui/shell/shell_session.h"
#include "../gui/shell/startmenu.h"
#include "../gui/wubu_screenshot.h"
#include "../gui/wubu_clipboard.h"
#include "../gui/shell/wubu_notify.h"
#include "../gui/wubu_session.h"
#include "../gui/wubu_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../kernel/input.h"

/* Frame-rate request from the environment, clamped: junk or out-of-range
 * falls back to the default rather than spinning at 0 fps or dividing by 0. */
static long fps_env(const char *name, long dflt, long lo, long hi) {
    const char *s = getenv(name);
    if (!s || !*s) return dflt;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (*end || v < lo || v > hi) return dflt;
    return v;
}

/* Frame-cost instrumentation, OFF unless WUBU_FPS_REPORT is set. Left in the product because a
 * number nobody can reproduce is how "it feels laggy" becomes an argument instead of a fix. */
static const char *perf_on(void) { static int v = -1; if (v < 0) v = getenv("WUBU_FPS_REPORT") ? 1 : 0; return v ? "" : NULL; }
static struct timespec g_perf_t0;
static double g_perf_acc[8];   /* 0 render 1 present 2 evdev 3 x11 pump 4 wl dispatch 5 input 6 other */

static long g_present_calls;   /* a cheap slot and a DEAD slot look identical without this */
static long g_present_calls_shown;
static int g_perf_n;
static struct timespec g_perf_start;
static int g_perf_frames;
static void perf_begin(void) { if (perf_on()) clock_gettime(CLOCK_MONOTONIC, &g_perf_t0); }
static void perf_end(int slot) {
    if (!perf_on()) return;
    struct timespec b; clock_gettime(CLOCK_MONOTONIC, &b);
    g_perf_acc[slot] += (b.tv_sec - g_perf_t0.tv_sec) * 1e3 + (b.tv_nsec - g_perf_t0.tv_nsec) / 1e6;
}
static void perf_frame_tick(void) {
    if (!perf_on()) return;
    if (g_perf_frames++ == 0) clock_gettime(CLOCK_MONOTONIC, &g_perf_start);
    if (++g_perf_n % 120) return;
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    double secs = (t.tv_sec - g_perf_start.tv_sec) + (t.tv_nsec - g_perf_start.tv_nsec) / 1e9;
    fprintf(stderr, "[perf] %.1f fps | render %.2f | present %.2f (%ld calls) | evdev %.2f | pump %.2f | wl %.2f | input %.2f | agi %.2f ms\n",
            g_perf_frames / secs, g_perf_acc[0] / g_perf_n, g_perf_acc[1] / g_perf_n,
            g_present_calls - g_present_calls_shown,
            g_perf_acc[2] / g_perf_n, g_perf_acc[3] / g_perf_n, g_perf_acc[4] / g_perf_n,
            g_perf_acc[5] / g_perf_n, g_perf_acc[6] / g_perf_n);
    g_present_calls_shown = g_present_calls;   /* per-window, not cumulative: a growing total hides a dead slot */
    for (int k = 0; k < 8; k++) g_perf_acc[k] = 0;
    g_perf_n = 0; g_perf_frames = 0;
}

int hosted_run(hosted_state_t *state) {
    fprintf(stderr, "WuBuOS running. Mode: %s\n",
            state->mode == HMODE_GUI ? "GUI" :
            state->mode == HMODE_TEMPLE ? "Temple" :
            state->mode == HMODE_CONSOLE ? "Console" : "Headless");

    struct timespec last;
struct timespec last_input;
    clock_gettime(CLOCK_MONOTONIC, &last);
    /* 60 fps while the user is interacting -- input is sampled once per loop,
     * so the loop rate IS the input latency -- and a lower rate when idle.
     * A full-rate redraw of a 1920x1080 desktop cost a whole core; idling at
     * 20 fps measured 31% of a core with nothing happening (headless weston
     * probe). The mascot and clock only need a few frames a second, so the
     * idle rate is tunable and defaults lower -- WUBU_FPS_IDLE / WUBU_FPS_ACTIVE
     * exist so the number can be measured instead of argued about. */
    const long frame_ns_active = 1000000000L / fps_env("WUBU_FPS_ACTIVE", 60, 1, 240);
    /* Idle at 30, not 8: any pointer move must repaint promptly, and a 125 ms idle tick made
     * the whole shell feel broken even when nothing was wrong with the drawing. */
    const long frame_ns_idle   = 1000000000L / fps_env("WUBU_FPS_IDLE", 30, 1, 240);
    long last_input_ms = last.tv_sec * 1000L + last.tv_nsec / 1000000L;

    bool screenshot_requested = (state->screenshot_path != NULL);
    int frame_count = 0;

    while (state->running) {
        /* X11 input FIRST. It used to be pumped at the END of the frame, so a pointer move
         * was queued after dispatch had already run and only took effect one frame later --
         * with the idle rate that is up to 125 ms of lag on every app and on the window. */
        perf_begin();
        if (hosted_x11_present_active() && hosted_x11_present_pump())
            state->running = false;
        perf_end(3);

        perf_begin();
        hosted_wl_dispatch();
        perf_end(4);

        perf_begin();
        int had_input = hosted_input_dispatch();
        /* An X event is input even if the same frame's dispatch already drained the queue: without this the
         * loop dropped to the 30 fps idle rate mid-drag (measured). */
        if (hosted_x11_present_activity()) had_input = 1;
        perf_end(5);
        if (had_input) {
            clock_gettime(CLOCK_MONOTONIC, &last_input);
            last_input_ms = last_input.tv_sec * 1000L +
                            last_input.tv_nsec / 1000000L;
        }

        /* Gamepads: drain evdev every frame and route it through Steam
         * Input's mapping table into the kernel input queue. Every frame,
         * not throttled -- input latency is visible. */
        perf_begin();
        wubu_evdev_poll();
        perf_end(2);

        /* The AGI play loop, throttled to about twice a second (the policy
         * is tick-driven): perceive -> decide -> act -> learn. Each learn
         * writes /kv/world/tick_<N> and the /n/kv mirror.
         * wubu_agi_play_tick() returns the ACTION (>=0) or -1 when the
         * session is stopped -- it is not a status code. */
        if (frame_count % 20 == 0) {
            /* The agent's requests (buddy mood, walkthrough) arrive through
             * its own namespace; act on them here. */
            hosted_agent_channel_poll();
            /* Family limits: throttled inside the guard, so this is one stat + one localtime a minute. */
            (void)wubu_parental_enforce(time(NULL), 30);
            if (wubu_agi_play_tick() >= 0) wubu_agi_play_learn();
            /* The buddy reports the thermal state for real: past 85C on the
             * CPU or the GPU the box is on fire and he reaches for the
             * extinguisher. Below that, activity drives the pose. */
            int hot = wubu_thermal_temp(WUBU_THERMAL_CPU);
            int ghot = wubu_thermal_temp(WUBU_THERMAL_GPU);
            if (hot >= 85 || ghot >= 85) wubu_bonzi_mood(WUBU_BUDDY_STATE_OVERHEAT);
        }
        frame_count++;

        if (state->framebuffer) {
            if (state->mode == HMODE_GUI) {
                /* Render into the SHM buffer the compositor is NOT holding:
                 * it reads the one committed last, so a mid-render read can
                 * never show a half-drawn frame. */
                shm_buffer_t *rb = &g_shm_bufs[g_shm_render_idx];
                if (rb->pixels)
                    vbe_set_render_target(rb->pixels, rb->width, rb->height);
                perf_begin();
                hosted_render_desktop(state);
                vbe_swap();
                perf_end(0);
                /* The GUI presents straight out of the VBE/Shm buffer; the
                 * shadow framebuffer is only needed for screenshots. */
                if (state->screenshot_path) {
                    VBEState *vs = vbe_state();
                    if (vs && vs->fb) {
                        memcpy(state->framebuffer, vs->fb,
                               (size_t)state->width * state->height * 4);
                    }
                }
            } else if (state->mode == HMODE_TEMPLE) {
                for (int i = 0; i < state->width * state->height; i++)
                    state->framebuffer[i] = 0x00000000;
            } else {
                /* HMODE_HEADLESS or HMODE_CONSOLE - render desktop for screenshot */
                hosted_render_desktop(state);
                vbe_swap();
                VBEState *vs = vbe_state();
                if (vs && vs->fb) {
                    memcpy(state->framebuffer, vs->fb,
                           (size_t)state->width * state->height * 4);
                }
            }
        }

        static int path_logged;
        if (!path_logged) {
            path_logged = 1;
            fprintf(stderr, "[path] present=%s mode=%d wl_surface=%p x11=%d\n",
                    g_wl.surface ? "wayland" : hosted_x11_present_active() ? "x11" : "none",
                    (int)state->mode, (void *)g_wl.surface, hosted_x11_present_active());
        }
        if (g_wl.surface) {
            hosted_wl_frame_render();
        } else if (hosted_x11_present_active()) {
            VBEState *vs = vbe_state();
            perf_begin();
            if (vs && vs->fb) { hosted_x11_present_frame(vs->fb); g_present_calls++; }
            perf_end(1);
        }

        /* If screenshot requested in headless mode, render a couple frames and exit */
        if (screenshot_requested && state->mode == HMODE_HEADLESS) {
            frame_count++;
            if (frame_count >= 2) {  /* Render a couple frames to ensure desktop is drawn */
                state->running = false;
                break;
            }
        }

        /* If headless mode without screenshot, just render once and exit */
        if (state->mode == HMODE_HEADLESS && !screenshot_requested) {
            frame_count++;
            if (frame_count >= 1) {
                state->running = false;
                break;
            }
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long now_ms = now.tv_sec * 1000L + now.tv_nsec / 1000000L;
        long frame_ns = (now_ms - last_input_ms < 1500) ? frame_ns_active
                                                       : frame_ns_idle;
        if (getenv("WUBU_FPS_DEBUG") && (frame_count % 30) == 0)
            fprintf(stderr, "[fpsdbg] frame=%d had_input=%d since_input=%ldms -> %s\n",
                    frame_count, had_input, now_ms - last_input_ms,
                    frame_ns == frame_ns_active ? "ACTIVE" : "idle");
        long elapsed = (now.tv_sec - last.tv_sec) * 1000000000L +
                       (now.tv_nsec - last.tv_nsec);
        if (elapsed < frame_ns) {
            /* Wait for the frame deadline IN THE EVENT WAIT, not in a blind
             * sleep: an input or compositor event then wakes the loop at
             * once, so a keystroke lands on the next frame instead of up to
             * a full frame later. poll() only reports readiness -- the next
             * iteration's hosted_wl_dispatch() consumes it. Headless (no
             * display) keeps the plain sleep. */
            long wait_ms = (frame_ns - elapsed) / 1000000L;
            if (wait_ms < 1) wait_ms = 1;
            int woke = (g_wl.surface) ? hosted_wl_wait_ms((int)wait_ms)
                                      : (hosted_x11_present_active()
                                             ? hosted_x11_wait_ms((int)wait_ms) : -1);
            if (woke < 0) {
                /* No display (headless): nothing can wake us early. */
                struct timespec slp = {0, frame_ns - elapsed};
                nanosleep(&slp, NULL);
            }
        }
        /* FULLSCREEN / resize: the surface changed size, so the guest must re-init at that size. Passing the
         * new size is the difference between a fullscreen session and a 1400x880 letterbox stretched over the
         * panel -- the guest reflows its desktop, panels and windows to the real surface. */
        {
            int nw = 0, nh = 0;
            if (hosted_x11_take_resize(&nw, &nh) && nw > 0 && nh > 0 &&
                (nw != state->width || nh != state->height)) {
                vbe_shutdown();
                state->width = nw;
                state->height = nh;
                state->fb_pitch = nw * 4;
                vbe_init(nw, nh);
                wubu_world_set_display((uint16_t)nw, (uint16_t)nh, 0, 0);
                if (hosted_x11_resize_surface(nw, nh) != 0)
                    fprintf(stderr, "[hosted] could not resize the surface to %dx%d\n", nw, nh);
                else
                    fprintf(stderr, "[hosted] surface re-init %dx%d (same window, focus kept)\n", nw, nh);
            }
        }

        perf_frame_tick();
        clock_gettime(CLOCK_MONOTONIC, &last);
    }
    return 0;
}

void hosted_shutdown(hosted_state_t *state) {
    fprintf(stderr, "WuBuOS shutdown...\n");

    /* Durable namespace.  Without this every change made during the session is
     * thrown away on exit -- the single worst property this OS had. */
    hosted_fs_flush();

    /* Save screenshot if requested: the writer picks PNG or PPM from the extension. Naming a PPM
     * ".png" (which this did) produces a file no viewer opens. */
    if (state->screenshot_path && state->framebuffer) {
        uint32_t *fb_src = NULL;
        VBEState *vs = vbe_state();
        if (vs && vs->fb) fb_src = vs->fb;
        if (!fb_src) fb_src = state->framebuffer;
        if (fb_src && hosted_screenshot_write(state->screenshot_path, fb_src, state->width, state->height))
            fprintf(stderr, "Screenshot saved to %s\n", state->screenshot_path);
    }

    wm_shutdown();
    desktop_shutdown();
    startmenu_shutdown();
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
    if (hosted_x11_present_active()) {
        VBEState *vs = vbe_state();
        if (vs && vs->fb) hosted_x11_present_frame(vs->fb);
        return;
    }
    hosted_wl_frame_render();
}

void hosted_set_mode(hosted_state_t *state, hosted_mode_t mode) {
    state->mode = mode;
    fprintf(stderr, "Mode: %s\n", mode == HMODE_GUI ? "GUI" :
            mode == HMODE_TEMPLE ? "Temple" : "Other");
}

int hosted_kernel_ready(void) {
    VBEState *vs = vbe_state();
    return (vs && vs->fb && vs->back && vs->width > 0) ? 1 : 0;
}

int hosted_wm_has_windows(void) {
    return wm_window_count() > 0 ? 1 : 0;
}
