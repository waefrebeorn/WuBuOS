/*
 * dosgui_screenshot.c  --  DOSGUI Desktop Screenshot Generator
 *
 * Renders the DosGui desktop to a PPM file for documentation.
 */

#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_desktop.h"
#include "../gui/dosgui_startmenu.h"
#include "../apps/dosgui_apps.h"
#include "../gui/dosgui_explorer.h"
#include "../gui/dosgui_daemon_panel.h"
#include "../gui/wubu_notify.h"
#include "../gui/wubu_theme.h"
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

    /* Frame 7: FreeDoom demo (GAAD-scaled window) */
    dosgui_launch_app("FreeDoom");
    for (int frame = 0; frame < 15; frame++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, W, H);
        vbe_swap();
    }
    if (vs && vs->fb) {
        write_ppm("docs/frame_07_freedoom.ppm", vs->fb, W, H);
        printf("Saved frame 7: freedoom demo\n");
    }

    /* Frame 8: GAAD snap demonstration - drag window to edge */
    {
        printf("Demonstrating GAAD snap...\n");
        /* Create a test window */
        DosGuiWindow *win = dosgui_wm_create(200, 200, 400, 300, "GAAD Snap Demo");
        if (win) {
            /* Simulate dragging to left edge (snap to left half) */
            dosgui_wm_handle_mouse(win->x + 10, win->y + 10, 1, 1);  /* mouse down on titlebar */
            dosgui_wm_handle_mouse(12, 210, 0, 0);  /* drag to left edge */
            dosgui_wm_handle_mouse(12, 210, 1, 2);  /* mouse up - triggers snap */
            for (int frame = 0; frame < 5; frame++) {
                dosgui_desktop_tick();
                dosgui_desktop_render(NULL, W, H);
                vbe_swap();
            }
            if (vs && vs->fb) {
                write_ppm("docs/frame_08_gaad_left.ppm", vs->fb, W, H);
                printf("Saved frame 8: GAAD left half snap\n");
            }
        }
    }

    /* Frame 9: GAAD snap to top-right quadrant */
    {
        DosGuiWindow *win = dosgui_wm_create(500, 100, 300, 200, "GAAD Quadrant Demo");
        if (win) {
            dosgui_wm_handle_mouse(win->x + 10, win->y + 10, 1, 1);
            dosgui_wm_handle_mouse(W - 12, 12, 0, 0);  /* drag to top-right corner */
            dosgui_wm_handle_mouse(W - 12, 12, 1, 2);
            for (int frame = 0; frame < 5; frame++) {
                dosgui_desktop_tick();
                dosgui_desktop_render(NULL, W, H);
                vbe_swap();
            }
            if (vs && vs->fb) {
                write_ppm("docs/frame_09_gaad_quadrant.ppm", vs->fb, W, H);
                printf("Saved frame 9: GAAD top-right quadrant snap\n");
            }
        }
    }

    /* Frame 10: Virtual Desktop Migration - Win+Shift+Right */
    {
        printf("Demonstrating virtual desktop migration...\n");
        /* Create windows on desktop 0 */
        DosGuiWindow *win1 = dosgui_wm_create(100, 100, 300, 200, "Desktop 0 Window A");
        DosGuiWindow *win2 = dosgui_wm_create(400, 300, 350, 250, "Desktop 0 Window B");
        
        if (win1 && win2) {
            /* Render current state (desktop 0) */
            for (int frame = 0; frame < 3; frame++) {
                dosgui_desktop_tick();
                dosgui_desktop_render(NULL, W, H);
                vbe_swap();
            }
            if (vs && vs->fb) {
                write_ppm("docs/frame_10_vdesk_0.ppm", vs->fb, W, H);
                printf("Saved frame 10: virtual desktop 0\n");
            }
            
            /* Switch to desktop 1 */
            dosgui_wm_set_current_desktop(1);
            for (int frame = 0; frame < 3; frame++) {
                dosgui_desktop_tick();
                dosgui_desktop_render(NULL, W, H);
                vbe_swap();
            }
            if (vs && vs->fb) {
                write_ppm("docs/frame_11_vdesk_1.ppm", vs->fb, W, H);
                printf("Saved frame 11: virtual desktop 1 (empty)\n");
            }
            
            /* Create window on desktop 1 */
            DosGuiWindow *win3 = dosgui_wm_create(200, 200, 400, 300, "Desktop 1 Window");
            if (win3) {
                for (int frame = 0; frame < 3; frame++) {
                    dosgui_desktop_tick();
                    dosgui_desktop_render(NULL, W, H);
                    vbe_swap();
                }
                if (vs && vs->fb) {
                    write_ppm("docs/frame_12_vdesk_1_win.ppm", vs->fb, W, H);
                    printf("Saved frame 12: virtual desktop 1 with window\n");
                }
            }
            
            /* Simulate Win+Shift+Right to move focused window from desktop 0 to desktop 1 */
            dosgui_wm_set_current_desktop(0);
            if (win1) {
                dosgui_wm_set_focus(win1);
                /* Win key (0xE05B) + Shift (0x01) + Right arrow (0xE04D) */
                dosgui_wm_handle_key(0xE04D, 0x09);  /* Win+Shift+Right */
                for (int frame = 0; frame < 3; frame++) {
                    dosgui_desktop_tick();
                    dosgui_desktop_render(NULL, W, H);
                    vbe_swap();
                }
                if (vs && vs->fb) {
                    write_ppm("docs/frame_13_vdesk_migrate.ppm", vs->fb, W, H);
                    printf("Saved frame 13: window migrated to desktop 1\n");
                }
            }
            
            /* Back to desktop 1 to show migrated window */
            dosgui_wm_set_current_desktop(1);
            for (int frame = 0; frame < 3; frame++) {
                dosgui_desktop_tick();
                dosgui_desktop_render(NULL, W, H);
                vbe_swap();
            }
            if (vs && vs->fb) {
                write_ppm("docs/frame_14_vdesk_1_after_migrate.ppm", vs->fb, W, H);
                printf("Saved frame 14: desktop 1 with migrated window\n");
            }
        }
    }

    /* Frame 15: HolyC Terminal with modifier keys */
    {
        printf("Demonstrating HolyC terminal...\n");
        DosGuiWindow *term = dosgui_wm_spawn_holyc_term(150, 150, 600, 400);
        if (term) {
            for (int frame = 0; frame < 5; frame++) {
                dosgui_desktop_tick();
                dosgui_desktop_render(NULL, W, H);
                vbe_swap();
            }
            if (vs && vs->fb) {
                write_ppm("docs/frame_15_holyc_term.ppm", vs->fb, W, H);
                printf("Saved frame 15: HolyC terminal\n");
            }
            
            /* Simulate typing "2 + 2" and pressing Enter */
            dosgui_wm_handle_key('2', 0);
            dosgui_wm_handle_key(' ', 0);
            dosgui_wm_handle_key('+', 0);
            dosgui_wm_handle_key(' ', 0);
            dosgui_wm_handle_key('2', 0);
            dosgui_wm_handle_key('\n', 0);
            for (int frame = 0; frame < 3; frame++) {
                dosgui_desktop_tick();
                dosgui_desktop_render(NULL, W, H);
                vbe_swap();
            }
            if (vs && vs->fb) {
                write_ppm("docs/frame_16_holyc_eval.ppm", vs->fb, W, H);
                printf("Saved frame 16: HolyC eval 2+2\n");
            }
            
            /* Ctrl+L to clear screen */
            dosgui_wm_handle_key(12, 0x02);  /* Ctrl+L */
            for (int frame = 0; frame < 3; frame++) {
                dosgui_desktop_tick();
                dosgui_desktop_render(NULL, W, H);
                vbe_swap();
            }
            if (vs && vs->fb) {
                write_ppm("docs/frame_17_holyc_cleared.ppm", vs->fb, W, H);
                printf("Saved frame 17: HolyC cleared with Ctrl+L\n");
            }
        }
    }

    /* Frame 18: Theme Cycling - Win98 Classic */
    {
        printf("Demonstrating theme cycling...\n");
        wubu_theme_set(THEME_WIN98_CLASSIC);
        for (int frame = 0; frame < 5; frame++) {
            dosgui_desktop_tick();
            dosgui_desktop_render(NULL, W, H);
            vbe_swap();
        }
        if (vs && vs->fb) {
            write_ppm("docs/frame_18_theme_win98.ppm", vs->fb, W, H);
            printf("Saved frame 18: Win98 Classic theme\n");
        }
    }

    /* Frame 19: Theme Cycling - XP Luna Blue */
    {
        wubu_theme_set(THEME_XP_LUNA_BLUE);
        for (int frame = 0; frame < 5; frame++) {
            dosgui_desktop_tick();
            dosgui_desktop_render(NULL, W, H);
            vbe_swap();
        }
        if (vs && vs->fb) {
            write_ppm("docs/frame_19_theme_xp_luna.ppm", vs->fb, W, H);
            printf("Saved frame 19: XP Luna Blue theme\n");
        }
    }

    /* Frame 20: Theme Cycling - XP Media Center Orange */
    {
        wubu_theme_set(THEME_XP_MEDIA_ORANGE);
        for (int frame = 0; frame < 5; frame++) {
            dosgui_desktop_tick();
            dosgui_desktop_render(NULL, W, H);
            vbe_swap();
        }
        if (vs && vs->fb) {
            write_ppm("docs/frame_20_theme_xp_media.ppm", vs->fb, W, H);
            printf("Saved frame 20: XP Media Center Orange theme\n");
        }
    }

    /* Frame 21: Theme Cycling - WuBu Green */
    {
        wubu_theme_set(THEME_WUBU_CUSTOM);
        for (int frame = 0; frame < 5; frame++) {
            dosgui_desktop_tick();
            dosgui_desktop_render(NULL, W, H);
            vbe_swap();
        }
        if (vs && vs->fb) {
            write_ppm("docs/frame_21_theme_wubu.ppm", vs->fb, W, H);
            printf("Saved frame 21: WuBu Green theme\n");
        }
    }

    /* Frame 22: File Manager (dosgui_explorer) */
    {
        dosgui_explorer_show();
        for (int frame = 0; frame < 10; frame++) {
            dosgui_desktop_tick();
            dosgui_desktop_render(NULL, W, H);
            vbe_swap();
        }
        if (vs && vs->fb) {
            write_ppm("docs/frame_22_explorer.ppm", vs->fb, W, H);
            printf("Saved frame 22: File Manager\n");
        }
        dosgui_explorer_hide();
    }

    /* Frame 23: Control Panel (daemon panel) */
    {
        dosgui_launch_app("Settings");
        for (int frame = 0; frame < 10; frame++) {
            dosgui_desktop_tick();
            dosgui_desktop_render(NULL, W, H);
            vbe_swap();
        }
        if (vs && vs->fb) {
            write_ppm("docs/frame_23_control_panel.ppm", vs->fb, W, H);
            printf("Saved frame 23: Control Panel\n");
        }
    }

    /* Frame 24: Container Manager (archd panel) */
    {
        dosgui_launch_app("Container Manager");
        for (int frame = 0; frame < 10; frame++) {
            dosgui_desktop_tick();
            dosgui_desktop_render(NULL, W, H);
            vbe_swap();
        }
        if (vs && vs->fb) {
            write_ppm("docs/frame_24_container_mgr.ppm", vs->fb, W, H);
            printf("Saved frame 24: Container Manager\n");
        }
    }

    /* Frame 25: Notification Center */
    {
        dosgui_notif_center_add("WuBuOS", "System Update", "Updates available", 1);
        dosgui_notif_center_add("TempleOS", "HolyC Compile", "Build complete", 0);
        dosgui_notif_center_add("Network", "Connected", "WiFi connected", 0);
        dosgui_notif_center_toggle();
        for (int frame = 0; frame < 5; frame++) {
            dosgui_desktop_tick();
            dosgui_desktop_render(NULL, W, H);
            vbe_swap();
        }
        if (vs && vs->fb) {
            write_ppm("docs/frame_25_notifications.ppm", vs->fb, W, H);
            printf("Saved frame 25: Notification Center\n");
        }
        dosgui_notif_center_toggle();
    }

    vbe_shutdown();
    printf("All frames saved!\n");
    return 0;
}
