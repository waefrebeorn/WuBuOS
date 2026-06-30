/*
 * wubu_demo_record.c  --  WuBuOS Demo Recorder
 * Runs the hosted GUI shell in VBE_HOSTED mode and captures frames to MP4.
 * Links against pre-built objects.
 */

#define _GNU_SOURCE
#define VBE_HOSTED
#define WUBU_NO_LIBM

#include "src/kernel/vbe.h"
#include "src/kernel/tasking.h"
#include "src/kernel/interrupt.h"
#include "src/kernel/memory.h"
#include "src/kernel/input.h"
#include "src/kernel/ps2.h"
#include "src/bridge/bridge.h"
#include "src/gui/wm.h"
#include "src/gui/startmenu.h"
#include "src/gui/gui_dbuf.h"
#include "src/gui/dosgui_wm.h"
#include "src/gui/dosgui_desktop.h"
#include "src/gui/dosgui_startmenu.h"
#include "src/gui/wubu_theme.h"
#include "src/compiler/holyc.h"
#include "src/apps/repl.h"
#include "src/apps/dosgui_apps.h"
#include "src/jit/jit.h"
#include "src/runtime/styx.h"
#include "src/runtime/styxfs.h"
#include "src/runtime/wubu_host_exec.h"
#include "src/tools/screenshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

/* Function declarations for kernel init */
extern int  mem_init(size_t total_bytes);
extern int  tasking_init(void);
extern int  interrupt_init(void);
extern void isr_install(void);
extern int  input_init(void);
extern void ps2_init(int screen_w, int screen_h);

int main(int argc, char **argv) {
    int frame_count = 120;  // 4 seconds at 30fps
    int width = 1024;
    int height = 768;
    int fps = 30;
    
    if (argc > 1) frame_count = atoi(argv[1]);
    if (argc > 2) fps = atoi(argv[2]);
    
    printf("WuBuOS Demo Recorder: %d frames at %dfps\n", frame_count, fps);
    
    /* Init VBE framebuffer (hosted mode) */
    printf("Calling vbe_init...\n");
    fflush(stdout);
    if (vbe_init(width, height) != 0) {
        fprintf(stderr, "Failed to init VBE\n");
        return 1;
    }
    printf("VBE init: %dx%d, fb=%p\n", width, height, vbe_state()->fb);
    fflush(stdout);
    
    /* Init kernel subsystems */
    printf("Calling mem_init...\n");
    fflush(stdout);
    mem_init(16 * 1024 * 1024);  /* 16MB */
    printf("mem_init done\n");
    fflush(stdout);
    
    printf("Calling tasking_init...\n");
    fflush(stdout);
    tasking_init();
    printf("tasking_init done\n");
    fflush(stdout);
    
    printf("Calling interrupt_init...\n");
    fflush(stdout);
    interrupt_init();
    printf("interrupt_init done\n");
    fflush(stdout);
    
    printf("Calling input_init...\n");
    fflush(stdout);
    input_init();
    printf("input_init done\n");
    fflush(stdout);
    
    printf("Calling ps2_init...\n");
    fflush(stdout);
    /* Skip ps2_init - it requires hardware I/O ports not available in userspace */
    /* ps2_init(width, height); */
    printf("ps2_init skipped (no hardware I/O in userspace)\n");
    fflush(stdout);
    
    /* Init GUI */
    dosgui_wm_init(width, height);
    dosgui_desktop_init();
    dosgui_startmenu_init();
    
    /* Start GIF recorder */
    wubu_gif_start("demo_frames", width, height, 1000/fps, frame_count);
    
    /* Simulated mouse clicks to demonstrate the desktop */
    int frame = 0;
    
    while (frame < frame_count) {
        /* Render desktop */
        dosgui_desktop_render(vbe_state()->fb, width, height);
        
        /* Swap back buffer to front buffer */
        vbe_swap();
        
        /* Simulate interactions at specific frames */
        if (frame == 30) {
            /* Click on start button area */
            dosgui_wm_handle_mouse(20, height - 30, 1, 1);
        } else if (frame == 45) {
            /* Click on Programs */
            dosgui_wm_handle_mouse(50, height - 200, 1, 1);
        } else if (frame == 60) {
            /* Click on Accessories */
            dosgui_wm_handle_mouse(250, height - 350, 1, 1);
        } else if (frame == 75) {
            /* Click on FreeDoom */
            dosgui_wm_handle_mouse(250, height - 420, 1, 1);
        } else if (frame == 90) {
            /* Click on FreeDoom to launch */
            dosgui_wm_handle_mouse(250, height - 420, 1, 1);
        }
        
        /* Tick the GUI */
        dosgui_desktop_tick();
        
        /* Capture frame */
        wubu_gif_add_frame(0, 0, width, height);
        
        frame++;
        
        /* Small delay to simulate real-time */
        usleep(1000000 / fps);
        
        if (frame % 30 == 0) {
            printf("Recorded %d/%d frames\n", frame, frame_count);
        }
    }
    
    /* Save frames */
    wubu_gif_stop();
    printf("Frames saved as demo_frames.frameXXX.ppm\n");
    
    /* Convert to MP4 using ffmpeg */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -framerate %d -i demo_frames.frame%%03d.ppm "
        "-c:v libx264 -preset fast -pix_fmt yuv420p -movflags +faststart wubuos_demo.mp4 2>/dev/null",
        fps);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "ffmpeg failed, trying alternative...\n");
        snprintf(cmd, sizeof(cmd),
            "ffmpeg -y -framerate %d -i demo_frames.frame%%03d.ppm "
            "-c:v libx264 -preset fast -pix_fmt yuv420p wubuos_demo.mp4 2>/dev/null",
            fps);
        system(cmd);
    }
    
    printf("Demo video: wubuos_demo.mp4\n");
    
    /* Cleanup frame files */
    for (int i = 0; i < frame_count; i++) {
        char path[256];
        snprintf(path, sizeof(path), "demo_frames.frame%03d.ppm", i);
        unlink(path);
    }
    
    dosgui_wm_shutdown();
    vbe_shutdown();
    
    return 0;
}
