/*
 * wubu_drv_gpu.c -- the DISPLAY/GPU driver (the Deck's Van Gogh RDNA2
 * iGPU + every laptop's iGPU).
 *
 * The GPU class contract (a KMS-lite model): the driver discovers the
 * connector + the modeset state from the device's MMIO window.
 *
 *   - the connector (eDP on laptops, the Deck's MIPI-DSI panel)
 *   - the current mode (the resolution + the refresh)
 *   - the VRAM size (the framebuffer)
 *
 * The tests inject a fake GPU window.
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_drv_gpu.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    volatile uint8_t *mmio;
    int   present;
    int   connector;         /* WUBU_GPU_CONNECTOR_* */
    int   width, height;     /* the mode */
    int   refresh_hz;
    uint64_t vram_mb;
} wubu_gpu_t;

static wubu_gpu_t g_gpu;

/* the register offsets (the class contract) */
#define GPU_REG_CONNECTOR  0x00
#define GPU_REG_WIDTH      0x04
#define GPU_REG_HEIGHT     0x08
#define GPU_REG_REFRESH    0x0C
#define GPU_REG_VRAM       0x10

/* G1: the driver probe. */
static int gpu_probe(wubu_drv_dev_t *dev)
{
    (void)dev;
    if (!g_gpu.mmio) return -1;
    g_gpu.connector = g_gpu.mmio[GPU_REG_CONNECTOR];
    g_gpu.width  = g_gpu.mmio[GPU_REG_WIDTH]  | (g_gpu.mmio[GPU_REG_WIDTH + 1] << 8);
    g_gpu.height = g_gpu.mmio[GPU_REG_HEIGHT] | (g_gpu.mmio[GPU_REG_HEIGHT + 1] << 8);
    g_gpu.refresh_hz = g_gpu.mmio[GPU_REG_REFRESH];
    g_gpu.vram_mb = (uint64_t)g_gpu.mmio[GPU_REG_VRAM] |
                    ((uint64_t)g_gpu.mmio[GPU_REG_VRAM + 1] << 8) |
                    ((uint64_t)g_gpu.mmio[GPU_REG_VRAM + 2] << 16) |
                    ((uint64_t)g_gpu.mmio[GPU_REG_VRAM + 3] << 24);
    g_gpu.present = 1;
    return 0;
}

const wubu_drv_id_t wubu_gpu_ids[] = {
    { 0x1002, 0x163F, 0, 0 },   /* AMD Van Gogh (the Deck's iGPU) */
    { 0x1002, 0x164C, 0, 0 },   /* Rembrandt iGPU */
    { 0x1002, 0x15E7, 0, 0 },   /* Cezanne/Barcelo iGPU */
    { 0x8086, 0x9A49, 0, 0 },   /* Intel Tiger Lake Xe */
    { 0x8086, 0x46A6, 0, 0 },   /* Intel Alder Lake Xe */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_gpu = {
    "gpu", wubu_gpu_ids, 5, gpu_probe,
};

/* the test hooks */
void wubu_gpu_set_mmio(volatile void *mmio)
{
    g_gpu.mmio = (volatile uint8_t *)mmio;
}
int wubu_gpu_present(void) { return g_gpu.present; }
int wubu_gpu_connector(void) { return g_gpu.connector; }
int wubu_gpu_width(void) { return g_gpu.width; }
int wubu_gpu_height(void) { return g_gpu.height; }
int wubu_gpu_refresh(void) { return g_gpu.refresh_hz; }
uint64_t wubu_gpu_vram_mb(void) { return g_gpu.vram_mb; }

const char *wubu_gpu_connector_name(int c)
{
    switch (c) {
    case WUBU_GPU_CONNECTOR_EDP:    return "eDP";
    case WUBU_GPU_CONNECTOR_DSI:    return "MIPI-DSI";
    case WUBU_GPU_CONNECTOR_HDMI:   return "HDMI";
    case WUBU_GPU_CONNECTOR_DP:     return "DisplayPort";
    default:                        return "unknown";
    }
}
