/*
 * desktop_render_test.c -- Test that desktop rendering produces non-black output
 */
#define VBE_HOSTED
#define WUBU_NO_LIBM
#include "../kernel/vbe.h"
#include "../gui/dosgui_desktop.h"
#include "../gui/dosgui_wm.h"
#include "../gui/wubu_theme.h"
#include "screenshot.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    int width = 1024;
    int height = 768;
    
    printf("=== Desktop Render Test ===\n");
    
    /* Init VBE */
    if (vbe_init(width, height) != 0) {
        fprintf(stderr, "vbe_init failed\n");
        return 1;
    }
    printf("VBE init: %dx%d\n", width, height);
    
    /* Init GUI */
    dosgui_wm_init(width, height);
    dosgui_desktop_init();
    
    /* Render desktop */
    printf("Rendering desktop...\n");
    dosgui_desktop_render(vbe_state()->fb, width, height);
    
    /* Swap back buffer to front */
    vbe_swap();
    
    /* Check front buffer */
    VBEState *vs = vbe_state();
    int nonzero = 0;
    for (int i = 0; i < width * height; i++) {
        if (vs->fb[i] != 0) nonzero++;
    }
    printf("Front buffer non-zero pixels: %d / %d\n", nonzero, width * height);
    
    if (nonzero == 0) {
        printf("FAIL: Frame is all black!\n");
        vbe_shutdown();
        return 1;
    }
    
    /* Capture screenshot */
    printf("Capturing screenshot...\n");
    int ret = wubu_shot_fullscreen("/tmp/desktop_render_test.ppm", SHOT_FMT_PPM);
    if (ret != 0) {
        fprintf(stderr, "Screenshot failed\n");
        vbe_shutdown();
        return 1;
    }
    printf("Screenshot saved to /tmp/desktop_render_test.ppm\n");
    
    dosgui_wm_shutdown();
    vbe_shutdown();
    
    printf("✅ Test passed - desktop rendering works!\n");
    return 0;
}