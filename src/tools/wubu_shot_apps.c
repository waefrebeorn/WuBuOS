/*
 * wubu_shot_apps.c -- Headless GUI screenshot harness for WuBuOS.
 *
 * Inits the software VBE framebuffer + DosGui WM, then creates real
 * Win98-framed windows directly (via dosgui_wm_create + a content draw
 * callback, exactly like the WM test suite does) and composites the full
 * desktop. Captures one PPM per "app" plus a desktop overview. No display
 * server required -- the VBE software framebuffer renders to a memory buffer.
 *
 * This deliberately avoids dosgui_desktop.c / dosgui_startmenu.c /
 * dosgui_apps.c (which pull the entire app+daemon graph); we render our own
 * window content so the harness links only the clean WM core that
 * test_dosgui_wm already proves builds.
 *
 * Build: see Makefile target test_gui_screenshot_apps (cloned from
 *       test_dosgui_wm's link line). Run: ./wubu_shot_apps
 */

#define VBE_HOSTED
#define WUBU_NO_LIBM
#include "../kernel/vbe.h"
#include "../gui/dosgui_wm.h"
#include "../gui/wubu_theme.h"
#include "screenshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHOT_W 1024
#define SHOT_H 768

/* -- App content renderers (self-contained, no external app graph) ------ */

static void draw_notepad(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    int x = win->x + 8, y = win->y + 28;
    vbe_fill_rect(x, y, win->w - 16, win->h - 36, 0x00FFFFFF);
    vbe_rect(x, y, win->w - 16, win->h - 36, 0x00808080);
    const char *lines[] = {
        "WuBuOS Notepad",
        "",
        "  A plain-text editor for the Temple.",
        "  New document -- start typing.",
        "",
        "  Ctrl+S  save    Ctrl+O  open",
    };
    for (int i = 0; i < 6; i++)
        vbe_draw_text(x + 10, y + 10 + i * 18, lines[i], 0x00000000, 1);
}

static void draw_calc(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    int x = win->x + 8, y = win->y + 28;
    vbe_fill_rect(x, y, win->w - 16, win->h - 36, 0x00000000);
    /* display */
    vbe_fill_rect(x + 10, y + 10, win->w - 36, 40, 0x00224422);
    vbe_draw_text(x + 20, y + 22, "42", 0x0033FF66, 1);
    /* button grid */
    const char *keys = "789/456*123-0.=+";
    int bx = x + 14, by = y + 64;
    for (int i = 0; i < 16; i++) {
        vbe_fill_rect(bx, by, 60, 44, 0x00C0C0C0);
        vbe_rect(bx, by, 60, 44, 0x00808080);
        char k[2] = { keys[i], 0 };
        vbe_draw_text(bx + 24, by + 14, k, 0x00000000, 1);
        bx += 70;
        if (i % 4 == 3) { bx = x + 14; by += 52; }
    }
}

static void draw_terminal(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    int x = win->x + 8, y = win->y + 28;
    vbe_fill_rect(x, y, win->w - 16, win->h - 36, 0x00000000);
    const char *lines[] = {
        "wubu@ring0:~$ hc_eval \"1+2+3\"",
        "6",
        "wubu@ring0:~$ hc_eval \"I64 sq(I64 n){return n*n;} sq(9)\"",
        "81",
        "wubu@ring0:~$ _",
    };
    for (int i = 0; i < 5; i++)
        vbe_draw_text(x + 10, y + 10 + i * 18, lines[i], 0x0033FF33, 1);
}

static void draw_canvas(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    int x = win->x + 8, y = win->y + 28;
    /* paint a gradient so compositing is visibly real work */
    int cw = win->w - 16, ch = win->h - 36;
    for (int yy = 0; yy < ch; yy++)
        for (int xx = 0; xx < cw; xx++)
            vbe_set_pixel(x + xx, y + yy,
                (uint32_t)((xx * 255 / cw) << 16) | ((yy * 255 / ch) << 8) | 0x30);
    vbe_rect(x, y, cw, ch, 0x00808080);
    vbe_draw_text(x + 10, y + 10, "WuBu Canvas -- layered editor", 0x00FFFFFF, 1);
}

static void draw_explorer(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    int x = win->x + 8, y = win->y + 28;
    vbe_fill_rect(x, y, win->w - 16, win->h - 36, 0x00FFFFFF);
    vbe_rect(x, y, win->w - 16, win->h - 36, 0x00808080);
    const char *rows[] = { "C:\\", "├─ WuBuOS/", "│  ├─ kernel/", "│  ├─ gui/",
                           "│  ├─ runtime/", "│  └─ compiler/", "└─ Users/" };
    for (int i = 0; i < 7; i++)
        vbe_draw_text(x + 10, y + 10 + i * 18, rows[i], 0x00000000, 1);
}

static void draw_placeholder(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    int x = win->x + 8, y = win->y + 28;
    vbe_fill_rect(x, y, win->w - 16, win->h - 36, 0x00ECE9D8);
    vbe_rect(x, y, win->w - 16, win->h - 36, 0x00808080);
    vbe_draw_text(x + 10, y + 10, win->title, 0x00000000, 1);
    vbe_draw_text(x + 10, y + 30, "(window ready)", 0x00666666, 1);
}

typedef void (*appdraw)(DosGuiWindow*, uint32_t*, int, int);

static struct { const char *title; int w, h; appdraw draw; } g_apps[] = {
    { "Untitled - Notepad",   500, 400, draw_notepad },
    { "Calculator",           400, 500, draw_calc },
    { "Terminal",             800, 600, draw_terminal },
    { "WuBu Canvas",          900, 640, draw_canvas },
    { "File Manager",         800, 600, draw_explorer },
    { "Control Panel",        640, 480, draw_placeholder },
    { "My Computer",          640, 480, draw_placeholder },
};

static void close_all(void) {
    for (int id = 0; id < 256; id++) {
        DosGuiWindow *w = dosgui_wm_find_by_id(id);
        if (w) dosgui_wm_destroy(w);
    }
}

static void capture(const char *name) {
    uint32_t *fb = vbe_framebuffer();
    dosgui_wm_render(fb, SHOT_W, SHOT_H);
    vbe_swap();
    char path[256];
    snprintf(path, sizeof(path), "/tmp/wubu_shot_%s.ppm", name);
    int r = wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    printf("  shot %-22s -> %s (%s)\n", name, path, r == 0 ? "ok" : "FAIL");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== WuBuOS Headless GUI Screenshot Harness ===\n");

    if (vbe_init(SHOT_W, SHOT_H) != 0) { fprintf(stderr, "vbe_init failed\n"); return 1; }
    if (dosgui_wm_init(SHOT_W, SHOT_H) != 0) { fprintf(stderr, "wm_init failed\n"); return 1; }

    /* Populate the desktop with the standard Win98-style icon set so the
     * desktop is not an empty teal void. */
    dosgui_icon_add_ex("My Computer",   DESK_ICON_DRIVE,  NULL, 0, 0, 0x00808080, NULL);
    dosgui_icon_add_ex("Recycle Bin",   DESK_ICON_FOLDER, NULL, 1, 0, 0x0080C000, NULL);
    dosgui_icon_add_ex("Notepad",       DESK_ICON_APP,    NULL, 0, 1, 0x00C0C0C0, NULL);
    dosgui_icon_add_ex("Calculator",    DESK_ICON_APP,    NULL, 1, 1, 0x00A0A0A0, NULL);
    dosgui_icon_add_ex("Terminal",      DESK_ICON_APP,    NULL, 0, 2, 0x0000A000, NULL);
    dosgui_icon_add_ex("WuBu Canvas",   DESK_ICON_APP,    NULL, 1, 2, 0x00C00080, NULL);
    dosgui_icon_add_ex("File Manager",  DESK_ICON_FOLDER, NULL, 0, 3, 0x000080C0, NULL);

    /* 1) Bare desktop (with icons) */
    printf("[desktop baseline]\n");
    capture("desktop");

    /* 2) One screenshot per app window -- clean cascade so each is fully
     * visible (real Windows-style staggered placement, no overlap). */
    printf("[per-app shots]\n");
    int n = (int)(sizeof(g_apps) / sizeof(g_apps[0]));
    for (int i = 0; i < n; i++) {
        close_all();
        int wx = 60 + (i % 4) * 30, wy = 40 + (i % 4) * 24;
        DosGuiWindow *w = dosgui_wm_create(wx, wy, g_apps[i].w, g_apps[i].h, g_apps[i].title);
        if (w) w->on_draw = g_apps[i].draw;
        dosgui_wm_render(vbe_framebuffer(), SHOT_W, SHOT_H);
        char slug[64];
        strncpy(slug, g_apps[i].title, sizeof(slug) - 1);
        for (char *p = slug; *p; p++) if (*p == ' ' || *p == '-') *p = '_';
        capture(slug);
    }

    /* 3) Desktop overview: tiled grid (2 columns x 4 rows) so every window
     * is visible without overlap. Keep every window ABOVE the taskbar. */
    printf("[overview: multiple apps]\n");
    close_all();
    {
        int task_h = dosgui_taskbar_height();
        int avail_h = SHOT_H - task_h - 40;       /* top margin 40 */
        int gap = 10;
        int wh = (avail_h - gap * 3) / 4;
        int ww = (SHOT_W - 90) / 2;
        for (int i = 0; i < n; i++) {
            int col = i % 2, row = i / 2;
            int wx = 30 + col * (ww + 30);
            int wy = 30 + row * (wh + gap);
            DosGuiWindow *w = dosgui_wm_create(wx, wy, ww, wh, g_apps[i].title);
            if (w) w->on_draw = g_apps[i].draw;
        }
    }
    capture("overview");

    close_all();
    dosgui_wm_shutdown();
    vbe_shutdown();
    printf("=== done ===\n");
    return 0;
}
