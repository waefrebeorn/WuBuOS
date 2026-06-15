/*
 * dosgui_screenshot.c  --  DOSGUI Desktop Screenshot Generator
 *
 * Renders the DosGui desktop to a PPM file for documentation.
 */

#include "dosgui_wm.h"
#include "dosgui_desktop.h"
#include "dosgui_startmenu.h"
#include "dosgui_apps.h"
#include "../kernel/vbe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* Write PPM file */
static void write_ppm(const char *filename, uint32_t *fb, int w, int h) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t c = fb[y * w + x];
            unsigned char rgb[3] = {
                (c >> 16) & 0xFF,
                (c >> 8) & 0xFF,
                c & 0xFF
            };
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
}

int main(void) {
    const int W = 1024;
    const int H = 768;

    /* Init VBE with hosted allocator */
    if (vbe_init(W, H) != 0) {
        fprintf(stderr, "vbe_init failed\n");
        return 1;
    }

    /* Init DosGui WM */
    dosgui_wm_init(W, H);

    /* Init desktop (adds icons) */
    dosgui_desktop_init();

    /* Launch some apps to show */
    printf("Launching apps...\n");
    dosgui_launch_app("My Computer");
    dosgui_launch_app("Temple REPL");
    dosgui_launch_app("Notepad");
    dosgui_launch_app("Paint");
    dosgui_launch_app("Calculator");
    dosgui_launch_app("Terminal");
    dosgui_launch_app("File Manager");
    dosgui_launch_app("Settings");
    dosgui_launch_app("Editor");
    dosgui_launch_app("WuBu Canvas");

    /* Frame 0: Clean desktop with all icons */
    for (int frame = 0; frame < 5; frame++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, W, H);
        vbe_swap();
    }

    VBEState *vs = vbe_state();
    if (vs && vs->fb) {
        write_ppm("docs/frame_00_desktop.ppm", vs->fb, W, H);
        printf("Saved frame 0: desktop\n");
    }

    /* Frame 1: Open start menu */
    dosgui_startmenu_open();
    for (int frame = 0; frame < 3; frame++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, W, H);
        dosgui_startmenu_render(NULL, W, H);
        vbe_swap();
    }
    if (vs && vs->fb) {
        write_ppm("docs/frame_01_startmenu.ppm", vs->fb, W, H);
        printf("Saved frame 1: start menu open\n");
    }

    /* Frame 2: Close start menu, open a few app windows */
    dosgui_startmenu_close();
    dosgui_launch_app("Calculator");
    dosgui_launch_app("Notepad");
    dosgui_launch_app("Terminal");
    for (int frame = 0; frame < 5; frame++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, W, H);
        vbe_swap();
    }
    if (vs && vs->fb) {
        write_ppm("docs/frame_02_apps.ppm", vs->fb, W, H);
        printf("Saved frame 2: apps open\n");
    }

    /* Frame 3: Move mouse over taskbar window buttons to highlight */
    dosgui_wm_handle_mouse(150, H - 14, 0, 0);  /* hover over taskbar */
    for (int frame = 0; frame < 3; frame++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, W, H);
        vbe_swap();
    }
    if (vs && vs->fb) {
        write_ppm("docs/frame_03_taskbar.ppm", vs->fb, W, H);
        printf("Saved frame 3: taskbar hover\n");
    }

    /* Frame 4: Click on a window to focus it */
    dosgui_wm_handle_mouse(200, 100, 1, 1);  /* click on first window */
    for (int frame = 0; frame < 3; frame++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, W, H);
        vbe_swap();
    }
    if (vs && vs->fb) {
        write_ppm("docs/frame_04_focus.ppm", vs->fb, W, H);
        printf("Saved frame 4: window focus\n");
    }

    /* Frame 5: Control Panel */
    dosgui_launch_app("Settings");
    for (int frame = 0; frame < 5; frame++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, W, H);
        vbe_swap();
    }
    if (vs && vs->fb) {
        write_ppm("docs/frame_05_control.ppm", vs->fb, W, H);
        printf("Saved frame 5: control panel\n");
    }

    /* Frame 6: Paint app */
    dosgui_launch_app("Paint");
    for (int frame = 0; frame < 5; frame++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, W, H);
        vbe_swap();
    }
    if (vs && vs->fb) {
        write_ppm("docs/frame_06_paint.ppm", vs->fb, W, H);
        printf("Saved frame 6: paint\n");
    }

    vbe_shutdown();
    printf("All frames saved!\n");
    return 0;
}
