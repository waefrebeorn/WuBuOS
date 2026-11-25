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
    const long frame_ns_idle   = 1000000000L / fps_env("WUBU_FPS_IDLE", 8, 1, 240);
    long last_input_ms = last.tv_sec * 1000L + last.tv_nsec / 1000000L;

    bool screenshot_requested = (state->screenshot_path != NULL);
    int frame_count = 0;

    while (state->running) {
        hosted_wl_dispatch();

        if (hosted_input_dispatch()) {
            clock_gettime(CLOCK_MONOTONIC, &last_input);
            last_input_ms = last_input.tv_sec * 1000L +
                            last_input.tv_nsec / 1000000L;
        }

        /* Gamepads: drain evdev every frame and route it through Steam
         * Input's mapping table into the kernel input queue. Every frame,
         * not throttled -- input latency is visible. */
        wubu_evdev_poll();

        /* The AGI play loop, throttled to about twice a second (the policy
         * is tick-driven): perceive -> decide -> act -> learn. Each learn
         * writes /kv/world/tick_<N> and the /n/kv mirror.
         * wubu_agi_play_tick() returns the ACTION (>=0) or -1 when the
         * session is stopped -- it is not a status code. */
        if (frame_count % 20 == 0) {
            /* The agent's requests (buddy mood, walkthrough) arrive through
             * its own namespace; act on them here. */
            hosted_agent_channel_poll();
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
                hosted_render_desktop(state);
                vbe_swap();
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

        if (g_wl.surface) {
            hosted_wl_frame_render();
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
            if (hosted_wl_wait_ms((int)wait_ms) < 0) {
                /* No display (headless): nothing can wake us early. */
                struct timespec slp = {0, frame_ns - elapsed};
                nanosleep(&slp, NULL);
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &last);
    }
    return 0;
}

void hosted_shutdown(hosted_state_t *state) {
    fprintf(stderr, "WuBuOS shutdown...\n");

    /* Durable namespace.  Without this every change made during the session is
     * thrown away on exit -- the single worst property this OS had. */
    hosted_fs_flush();

    /* Save screenshot if requested */
    if (state->screenshot_path && state->framebuffer) {
        FILE *f = fopen(state->screenshot_path, "wb");
        if (f) {
            /* Write simple PPM format */
            fprintf(f, "P6\n%d %d\n255\n", state->width, state->height);
    /* Screenshot: read from the live VBE framebuffer (which may be an
     * SHM surface in hosted mode, or a malloc buffer in headless/test). */
    uint32_t *fb_src = NULL;
    VBEState *vs = vbe_state();
    if (vs && vs->fb) fb_src = vs->fb;
    if (!fb_src && state->framebuffer) fb_src = state->framebuffer;
    if (fb_src) {
        for (int y = 0; y < state->height; y++) {
            for (int x = 0; x < state->width; x++) {
                uint32_t c = fb_src[y * state->width + x];
                fputc((c >> 16) & 0xFF, f);  /* R */
                fputc((c >> 8) & 0xFF, f);   /* G */
                fputc(c & 0xFF, f);          /* B */
            }
        }
    }
            fclose(f);
            fprintf(stderr, "Screenshot saved to %s\n", state->screenshot_path);
        }
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
