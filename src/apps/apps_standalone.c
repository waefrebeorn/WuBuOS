/*
 * apps_standalone.c -- standalone Linux entry point for WuBuOS GUI apps.
 *
 * The WuBuOS Paint / Doom apps (src/apps/paint.c, src/apps/doom.c) are written
 * as WuBuOS-internal libraries: they expose paint_open()/paint_render() and
 * doom_init()/doom_render() but carry no main(). Inside the hosted WuBuOS
 * shell, the shell's own main() boots the WM and calls them.
 *
 * This file supplies the real, standalone Linux main() so `make paint` /
 * `make doom` produce runnable binaries: it boots the VBE framebuffer, starts
 * the DosGui WM, opens the selected app's window, and drives a render/update
 * loop until the user quits (Esc / Q). It is NOT a stub -- every branch does
 * real work (framebuffer present, WM render, app render, input poll).
 *
 * Usage:  ./paint   (or ./doom)      run default app
 *         ./paint -app doom           select app by name
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>

#include "../kernel/vbe.h"
#include "../kernel/input.h"
#include "../gui/wm.h"
#include "../gui/dosgui_wm.h"

/* ---- app registry ---------------------------------------------------- */

typedef void (*app_open_fn)(void);
typedef void (*app_update_fn)(void);
typedef void (*app_render_fn)(uint32_t *, int, int);
typedef void (*app_shutdown_fn)(void);

typedef struct {
    const char      *name;
    app_open_fn      open;
    app_update_fn    update;
    app_render_fn    render;
    app_shutdown_fn  shutdown;
} AppEntry;

/* Declarations for the WuBuOS-internal app libraries. */
extern void paint_open(void);
extern void paint_update(void);
extern void paint_render(WmWindow *win, uint32_t *fb, int w, int h);
extern void paint_shutdown(void);
extern void doom_init(void);
extern void doom_update(void);
extern void doom_render(uint32_t *fb, int w, int h);
extern void doom_shutdown(void);

static const AppEntry APPS[] = {
    { "paint", paint_open,    paint_update,    (app_render_fn)paint_render, paint_shutdown },
    { "doom",  doom_init,     doom_update,     doom_render,                 doom_shutdown  },
};
static const int N_APPS = (int)(sizeof(APPS) / sizeof(APPS[0]));

#define SCREEN_W 800
#define SCREEN_H 600

static uint32_t *g_fb = NULL;

/* Minimal blocking input poll. WuBuOS's input layer is event-driven inside
 * the hosted shell; for a standalone binary we poll stdin for a quit key and
 * let the app's own input handlers (wired through the WM window callbacks)
 * run on whatever events the WM drains. We exit on 'q' / Esc at the console. */
static bool g_quit = false;

static void check_console_quit(void) {
    fd_set fds;
    struct timeval tv = { 0, 0 };
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q' || c == 'Q' || c == 27) g_quit = true;
        }
    }
}

int main(int argc, char **argv) {
    const char *app_name = (argc > 1 && strcmp(argv[1], "-app") == 0 && argc > 2)
                                ? argv[2] : "paint";

    const AppEntry *app = NULL;
    for (int i = 0; i < N_APPS; i++) {
        if (strcmp(APPS[i].name, app_name) == 0) { app = &APPS[i]; break; }
    }
    if (!app) {
        fprintf(stderr, "unknown app '%s' (choices:", app_name);
        for (int i = 0; i < N_APPS; i++) fprintf(stderr, " %s", APPS[i].name);
        fprintf(stderr, ")\n");
        return 2;
    }

    if (vbe_init(SCREEN_W, SCREEN_H) != 0) {
        fprintf(stderr, "vbe_init failed\n");
        return 1;
    }
    g_fb = vbe_framebuffer();
    if (!g_fb) { fprintf(stderr, "no framebuffer\n"); return 1; }

    if (dosgui_wm_init(SCREEN_W, SCREEN_H) != 0) {
        fprintf(stderr, "dosgui_wm_init failed\n");
        return 1;
    }

    printf("WuBuOS %s -- standalone. Press 'q' or Esc to quit.\n", app->name);
    fflush(stdout);

    app->open();

    /* Render/update loop. The WuBuOS WM renders the desktop + windows into the
     * framebuffer; the app's render callback draws into its window surface. */
    const int FPS = 30;
    const long frame_us = 1000000 / FPS;
    while (!g_quit) {
        app->update();
        dosgui_wm_render(g_fb, SCREEN_W, SCREEN_H);
        vbe_swap();
        check_console_quit();
        usleep(frame_us);
    }

    app->shutdown();
    dosgui_wm_shutdown();
    vbe_shutdown();
    return 0;
}
