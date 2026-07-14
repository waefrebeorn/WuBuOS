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

#include "../kernel/vbe.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_desktop.h"
#include "../gui/dosgui_startmenu.h"
#include "../gui/wubu_screenshot.h"
#include "../gui/wubu_clipboard.h"
#include "../gui/wubu_notify.h"
#include "../gui/wubu_session.h"
#include "../gui/wubu_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

        hosted_input_dispatch();

        if (state->framebuffer) {
            if (state->mode == HMODE_GUI) {
                hosted_render_desktop(state);
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

int hosted_kernel_ready(void) {
    VBEState *vs = vbe_state();
    return (vs && vs->fb && vs->back && vs->width > 0) ? 1 : 0;
}

int hosted_wm_has_windows(void) {
    return dosgui_wm_window_count() > 0 ? 1 : 0;
}
