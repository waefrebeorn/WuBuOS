/*
 * wubu_demo_screenshot.c — Render demo frames for screenshots/GIFs
 */

#include "../gui/wubu_wm.h"
#include "../gui/wubu_theme.h"
#include "../kernel/vbe.h"
#include "../kernel/wubu_gaad.h"
#include "../tools/screenshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern WubuWM g_wm;

int main(void) {
    printf("=== WuBuOS Demo Screenshot Generator ===\n");

    /* Initialize VBE (hosted mode) */
    if (vbe_init(1280, 720) != 0) {
        fprintf(stderr, "vbe_init failed\n");
        return 1;
    }

    /* Initialize theme engine */
    wubu_theme_init();

    /* Initialize WM */
    WubuWM *wm = wubu_wm_state();
    wubu_wm_init(1280, 720);

    VBEState *vbe = vbe_state();

    /* Create demo windows */
    WubuWin *win1 = wubu_wm_create(100, 100, 500, 350, "Terminal — WuBuOS");
    if (win1) {
        win1->on_draw = NULL; /* Could add custom draw */
    }

    WubuWin *win2 = wubu_wm_create(650, 150, 450, 300, "Notepad++ — CODE.WUBU");
    if (win2) {
        win2->flags |= WUBU_WIN_FOCUSED;
        wubu_wm_set_focus(win2);
    }

    WubuWin *win3 = wubu_wm_create(200, 500, 400, 200, "Paint — image.wubu");
    if (win3) {
        // minimized for demo
        wubu_wm_minimize(win3);
    }

    WubuWin *win4 = wubu_wm_create(800, 500, 350, 200, "FreeDoom.wubu");
    if (win4) {
        // maximized
        wubu_wm_maximize(win4);
    }

    /* Frame 1: Initial desktop */
    printf("Frame 1: Initial desktop with 4 windows\n");
    wubu_wm_render(vbe->back, 1280, 720);
    vbe_swap();
    wubu_write_bmp("/tmp/wubu_frame1.bmp", vbe->fb, 1280, 720);

    /* Frame 2: Focus on Notepad++ */
    printf("Frame 2: Focus Notepad++\n");
    wubu_wm_set_focus(win2);
    wubu_wm_render(vbe->back, 1280, 720);
    vbe_swap();
    wubu_write_bmp("/tmp/wubu_frame2.bmp", vbe->fb, 1280, 720);

    /* Frame 3: Move terminal window */
    printf("Frame 3: Move terminal\n");
    wubu_wm_move(win1, 50, 50);
    wubu_wm_render(vbe->back, 1280, 720);
    vbe_swap();
    wubu_write_bmp("/tmp/wubu_frame3.bmp", vbe->fb, 1280, 720);

    /* Frame 4: Resize terminal */
    printf("Frame 4: Resize terminal\n");
    wubu_wm_resize(win1, 600, 400);
    wubu_wm_render(vbe->back, 1280, 720);
    vbe_swap();
    wubu_write_bmp("/tmp/wubu_frame4.bmp", vbe->fb, 1280, 720);

    /* Frame 5: GAAD snap terminal to left */
    printf("Frame 5: GAAD snap terminal left\n");
    wubu_wm_gaad_snap(win1);
    wubu_wm_render(vbe->back, 1280, 720);
    vbe_swap();
    wubu_write_bmp("/tmp/wubu_frame5.bmp", vbe->fb, 1280, 720);

    /* Frame 6: Switch to desktop 2 */
    printf("Frame 6: Switch desktop (Ctrl+Alt+Right)\n");
    wm->desktops.current = 1;
    WubuWin *dw2 = wubu_wm_create(200, 200, 400, 300, "Desktop 2 — Browser");
    if (dw2) {
        wubu_wm_set_focus(dw2);
    }
    wubu_wm_render(vbe->back, 1280, 720);
    vbe_swap();
    wubu_write_bmp("/tmp/wubu_frame6.bmp", vbe->fb, 1280, 720);

    /* Frame 7: Theme cycle (F5) */
    printf("Frame 7: Theme cycle to XP Luna Blue\n");
    wubu_theme_cycle();
    wubu_wm_render(vbe->back, 1280, 720);
    vbe_swap();
    wubu_write_bmp("/tmp/wubu_frame7.bmp", vbe->fb, 1280, 720);

    /* Frame 8: Theme cycle to XP Media Center */
    printf("Frame 8: Theme cycle to XP Media Center\n");
    wubu_theme_cycle();
    wubu_wm_render(vbe->back, 1280, 720);
    vbe_swap();
    wubu_write_bmp("/tmp/wubu_frame8.bmp", vbe->fb, 1280, 720);

    /* Frame 9: Theme cycle to WuBu Custom */
    printf("Frame 9: Theme cycle to WuBu Custom\n");
    wubu_theme_cycle();
    wubu_wm_render(vbe->back, 1280, 720);
    vbe_swap();
    wubu_write_bmp("/tmp/wubu_frame9.bmp", vbe->fb, 1280, 720);

    /* Frame 10: Back to Win98 Classic */
    printf("Frame 10: Back to Win98 Classic\n");
    wubu_theme_cycle();
    wubu_wm_render(vbe->back, 1280, 720);
    vbe_swap();
    wubu_write_bmp("/tmp/wubu_frame10.bmp", vbe->fb, 1280, 720);

    /* Cleanup */
    wubu_wm_shutdown();
    vbe_shutdown();

    printf("\n✅ Generated 10 frames: /tmp/wubu_frame1.bmp ... /tmp/wubu_frame10.bmp\n");
    return 0;
}