/*
 * demo_capture.c  --  Headless WuBuOS Demo Frame Capture
 * Renders theme cycling demo to PPM frames, then ffmpeg -> GIF/MP4
 */

#include "src/apps/dosgui_apps.h"
#include "src/gui/dosgui_startmenu.h"
#include "src/kernel/vbe.h"
#include "src/kernel/memory.h"
#include "src/kernel/input.h"
#include "src/gui/dosgui_wm.h"
#include "src/gui/dosgui_desktop.h"
#include "src/gui/wubu_theme.h"
#include "src/tools/screenshot.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEMO_W 800
#define DEMO_H 600
#define FRAME_DIR "/tmp/wubuos_demo_frames"

int main(void) {
    printf("WuBuOS Demo Capture: %dx%d\n", DEMO_W, DEMO_H);
    
    /* Clean and create frame directory */
    system("rm -rf " FRAME_DIR);
    system("mkdir -p " FRAME_DIR);
    
    /* Init kernel subsystems */
    mem_init(1024 * 1024);
    vbe_init(DEMO_W, DEMO_H);
    input_init();
    
    VBEState *vbe = vbe_state();
    if (!vbe || !vbe->fb) { fprintf(stderr, "VBE init failed\n"); return 1; }
    
    /* Init GUI with theme engine */
    dosgui_wm_init(DEMO_W, DEMO_H);
    dosgui_desktop_init();
    
    int frame = 0;
    char path[256];
    
    /* ===== PHASE 1: Win98 Classic (default) - 10 frames ===== */
    for (int i = 0; i < 10; i++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, DEMO_W, DEMO_H);
        vbe_swap(); snprintf(path, sizeof(path), FRAME_DIR "/frame_%04d.ppm", frame++);
        wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    }
    
    /* ===== PHASE 2: Launch some windows - 15 frames ===== */
    DosGuiWindow *win1 = dosgui_wm_create(100, 80, 500, 380, "Calculator");
    win1->on_draw = dosgui_calc_draw;
    DosGuiWindow *win2 = dosgui_wm_create(250, 150, 600, 450, "File Manager");
    win2->on_draw = dosgui_explorer_draw;
    DosGuiWindow *win3 = dosgui_wm_create(400, 200, 400, 300, "Notepad");
    win3->on_draw = dosgui_notepad_draw;
    
    for (int i = 0; i < 15; i++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, DEMO_W, DEMO_H);
        vbe_swap(); snprintf(path, sizeof(path), FRAME_DIR "/frame_%04d.ppm", frame++);
        wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    }
    
    /* ===== PHASE 3: Cycle to XP Luna Blue - 20 frames ===== */
    wubu_theme_set(THEME_XP_LUNA_BLUE);
    dosgui_wm_init(DEMO_W, DEMO_H);  /* Reinit WM for new theme metrics */
    dosgui_desktop_init();
    
    /* Re-launch windows */
    win1 = dosgui_wm_create(100, 80, 500, 380, "Calculator");
    win1->on_draw = dosgui_calc_draw;
    win2 = dosgui_wm_create(250, 150, 600, 450, "File Manager");
    win2->on_draw = dosgui_explorer_draw;
    win3 = dosgui_wm_create(400, 200, 400, 300, "Notepad");
    win3->on_draw = dosgui_notepad_draw;
    
    for (int i = 0; i < 20; i++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, DEMO_W, DEMO_H);
        vbe_swap(); snprintf(path, sizeof(path), FRAME_DIR "/frame_%04d.ppm", frame++);
        wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    }
    
    /* ===== PHASE 4: Open Start Menu - 15 frames ===== */
    dosgui_startmenu_open();
    for (int i = 0; i < 15; i++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, DEMO_W, DEMO_H);
        vbe_swap(); snprintf(path, sizeof(path), FRAME_DIR "/frame_%04d.ppm", frame++);
        wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    }
    dosgui_startmenu_close();
    
    /* ===== PHASE 5: Cycle to XP Media Orange - 15 frames ===== */
    wubu_theme_set(THEME_XP_MEDIA_ORANGE);
    dosgui_wm_init(DEMO_W, DEMO_H);
    dosgui_desktop_init();
    
    win1 = dosgui_wm_create(100, 80, 500, 380, "Calculator");
    win1->on_draw = dosgui_calc_draw;
    win2 = dosgui_wm_create(250, 150, 600, 450, "File Manager");
    win2->on_draw = dosgui_explorer_draw;
    win3 = dosgui_wm_create(400, 200, 400, 300, "Notepad");
    win3->on_draw = dosgui_notepad_draw;
    
    for (int i = 0; i < 15; i++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, DEMO_W, DEMO_H);
        vbe_swap(); snprintf(path, sizeof(path), FRAME_DIR "/frame_%04d.ppm", frame++);
        wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    }
    
    /* ===== PHASE 6: Cycle to WuBu Green - 15 frames ===== */
    wubu_theme_set(THEME_WUBU_CUSTOM);
    dosgui_wm_init(DEMO_W, DEMO_H);
    dosgui_desktop_init();
    
    win1 = dosgui_wm_create(100, 80, 500, 380, "Calculator");
    win1->on_draw = dosgui_calc_draw;
    win2 = dosgui_wm_create(250, 150, 600, 450, "File Manager");
    win2->on_draw = dosgui_explorer_draw;
    win3 = dosgui_wm_create(400, 200, 400, 300, "Notepad");
    win3->on_draw = dosgui_notepad_draw;
    
    for (int i = 0; i < 15; i++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, DEMO_W, DEMO_H);
        vbe_swap(); snprintf(path, sizeof(path), FRAME_DIR "/frame_%04d.ppm", frame++);
        wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    }
    
    /* ===== PHASE 7: Back to Win98 Classic - 10 frames ===== */
    wubu_theme_set(THEME_WIN98_CLASSIC);
    dosgui_wm_init(DEMO_W, DEMO_H);
    dosgui_desktop_init();
    
    win1 = dosgui_wm_create(100, 80, 500, 380, "Calculator");
    win1->on_draw = dosgui_calc_draw;
    win2 = dosgui_wm_create(250, 150, 600, 450, "File Manager");
    win2->on_draw = dosgui_explorer_draw;
    win3 = dosgui_wm_create(400, 200, 400, 300, "Notepad");
    win3->on_draw = dosgui_notepad_draw;
    
    for (int i = 0; i < 10; i++) {
        dosgui_desktop_tick();
        dosgui_desktop_render(NULL, DEMO_W, DEMO_H);
        vbe_swap(); snprintf(path, sizeof(path), FRAME_DIR "/frame_%04d.ppm", frame++);
        wubu_shot_fullscreen(path, SHOT_FMT_PPM);
    }
    
    printf("Captured %d frames to %s\n", frame, FRAME_DIR);
    
    /* Generate GIF and MP4 with ffmpeg */
    printf("Generating GIF...\n");
    system("ffmpeg -y -framerate 10 -i " FRAME_DIR "/frame_%04d.ppm "
           "-vf \"scale=800:600:flags=lanczos\" "
           "/tmp/wubuos_demo.gif 2>/dev/null");
    
    printf("Generating MP4...\n");
    system("ffmpeg -y -framerate 10 -i " FRAME_DIR "/frame_%04d.ppm "
           "-c:v libx264 -pix_fmt yuv420p -crf 18 "
           "/tmp/wubuos_demo.mp4 2>/dev/null");
    
    /* Copy to project docs */
    system("cp /tmp/wubuos_demo.gif docs/wubuos_demo.gif 2>/dev/null");
    system("cp /tmp/wubuos_demo.mp4 docs/wubuos_demo.mp4 2>/dev/null");
    
    printf("Done! Demo saved to:\n");
    printf("  /tmp/wubuos_demo.gif\n");
    printf("  /tmp/wubuos_demo.mp4\n");
    printf("  docs/wubuos_demo.gif\n");
    printf("  docs/wubuos_demo.mp4\n");
    
    return 0;
}