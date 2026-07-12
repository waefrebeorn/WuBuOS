/*
 * canvas_standalone.c -- standalone Linux entry point for the WuBuOS image
 * editor (the real, layered wubu_canvas engine — Photoshop-class, not the
 * removed MS-Paint toy).
 *
 * Boots the VBE framebuffer + DosGui WM, creates a wubu_canvas with a real
 * background layer, and drives a render/update loop that composites the canvas
 * into the framebuffer every frame. Every branch does real work (framebuffer
 * present, WM render, canvas composite, input poll).
 *
 * Build: make canvas
 * Run:   ./canvas
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
#include "wubu_canvas.h"

#define SCREEN_W 800
#define SCREEN_H 600

static uint32_t *g_fb = NULL;
static WubuCanvas *g_cv = NULL;
static bool       g_quit = false;

/* Minimal blocking input poll. Quits on 'q' / Esc at the console; draws a
 * brush dab at the center of the canvas on any other key to prove the engine
 * is live (undo-capable, layered compositing). */
static void check_console_quit(void) {
    fd_set fds;
    struct timeval tv = { 0, 0 };
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q' || c == 'Q' || c == 27) { g_quit = true; return; }
            /* 'b' paints a red dab on the active layer to exercise the engine */
            if (c == 'b' && g_cv) {
                wubu_cv_brush(g_cv, g_cv->w / 2, g_cv->h / 2);
            }
        }
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

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

    /* Real editor: 800x600 canvas, background + a paint layer. */
    g_cv = wubu_cv_create(SCREEN_W, SCREEN_H);
    if (!g_cv) { fprintf(stderr, "wubu_cv_create failed\n"); return 1; }
    wubu_cv_layer_add(g_cv, "Paint");
    g_cv->active_layer = 1;

    /* Seed background with a vertical gradient so compositing is visible. */
    {
        WubuLayer *bg = wubu_cv_layer_get(g_cv, 0);
        if (bg && bg->pixels) {
            for (int y = 0; y < bg->h; y++)
                for (int x = 0; x < bg->w; x++)
                    bg->pixels[y * bg->w + x] =
                        (0x20 << 16) | (((y * 255) / bg->h) << 8) | 0x40;
        }
    }

    printf("WuBuOS Canvas (wubu_canvas) -- standalone. Press 'q' or Esc to quit, 'b' to brush.\n");
    fflush(stdout);

    const int FPS = 30;
    const long frame_us = 1000000 / FPS;
    while (!g_quit) {
        /* Composite the layered canvas into the framebuffer. */
        wubu_cv_composite(g_cv, g_fb, SCREEN_W, SCREEN_H);
        dosgui_wm_render(g_fb, SCREEN_W, SCREEN_H);
        vbe_swap();
        check_console_quit();
        usleep(frame_us);
    }

    wubu_cv_destroy(g_cv);
    dosgui_wm_shutdown();
    vbe_shutdown();
    return 0;
}
