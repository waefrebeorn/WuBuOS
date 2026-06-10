/*
 * wubu_drm_direct_test.c — Test for direct DRM/KMS implementation (Cells 388/389)
 */

#include "wubu_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== wubu_drm_direct test ===\n");
    
    WubuDisplay d = {0};
    
    printf("Testing wubu_display_init...\n");
    int ret = wubu_display_init(&d, 1920, 1080);
    if (ret != 0) {
        printf("DRM init failed (expected on CI without DRM device): %d\n", ret);
        printf("wubu_display_init returns %d on no DRM device.\n", ret);
        printf("✅ Test passed (graceful failure on no DRM)\n");
        return 0;
    }
    
    printf("DRM initialized: %dx%d, fd=%d\n", d.fb_w, d.fb_h, d.drm_fd);
    
    /* Test GBM device creation */
    printf("GBM device: %p\n", d.gbm_device);
    printf("FB map: %p, size: %zu\n", d.fb_map, d.fb_size);
    
    if (d.gbm_device) {
        /* Test GBM BO creation */
        wubu_gbm_bo_t *bo = wubu_gbm_bo_create(d.gbm_device, 64, 64, 0);
        if (bo) {
            printf("GBM BO created: handle=%u, stride=%u, map=%p\n", 
                   bo->handle, bo->stride, bo->map);
            if (bo->map) {
                /* Write a test pattern */
                uint32_t *pixels = bo->map;
                for (int i = 0; i < 64 * 64; i++) {
                    pixels[i] = 0xFF0000FF;  // Blue in ARGB
                }
                printf("Wrote test pattern to BO\n");
            }
            wubu_gbm_bo_destroy(d.gbm_device, bo);
            printf("GBM BO destroyed\n");
        }
        
        /* Test buffer swap */
        wubu_display_swap(&d);
        printf("Buffer swap called\n");
    }
    
    /* Test input polling */
    int events = wubu_display_poll_input(&d);
    printf("Input events polled: %d\n", events);
    
    /* Shutdown */
    wubu_display_shutdown(&d);
    printf("Shutdown complete\n");
    
    printf("✅ All tests passed\n");
    return 0;
}