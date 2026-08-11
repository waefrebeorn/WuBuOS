/*
 * wubu_drv_gpu.h -- the display/GPU driver.
 */
#ifndef WUBU_DRV_GPU_H
#define WUBU_DRV_GPU_H

#include <stdint.h>

/* the connectors */
enum {
    WUBU_GPU_CONNECTOR_EDP  = 1,
    WUBU_GPU_CONNECTOR_DSI  = 2,
    WUBU_GPU_CONNECTOR_HDMI = 3,
    WUBU_GPU_CONNECTOR_DP   = 4,
};

/* the driver (registered by the registry) */
extern const struct wubu_drv wubu_drv_gpu;

/* the test hooks */
void wubu_gpu_set_mmio(volatile void *mmio);
int  wubu_gpu_present(void);
int  wubu_gpu_connector(void);
int  wubu_gpu_width(void);
int  wubu_gpu_height(void);
int  wubu_gpu_refresh(void);
uint64_t wubu_gpu_vram_mb(void);
const char *wubu_gpu_connector_name(int c);

/* WSL2 passthrough (the magic OS detects its own runtime): on /dev/dxg
 * (WSL2 paravirtualized GPU), the hardware probe is skipped and this
 * stub fills in the state from the DXG backend. Returns nonzero on WSL. */
int wubu_gpu_present_wsl(void);

#endif
