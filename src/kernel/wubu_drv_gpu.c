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
#include <unistd.h>

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
    /* ---- AMD (0x1002) ---- */
    /* GCN1/2 (Southern Islands / Sea Islands) -- need amdgpu.si_support=1 /
     * amdgpu.cik_support=1 module params (radeon KMD has no Vulkan). */
    { 0x1002, 0x6798, 0, 0 },   /* Tahiti (HD 7950/7970) GCN1 */
    { 0x1002, 0x6810, 0, 0 },   /* Pitcairn (HD 7850/7870) GCN1 */
    { 0x1002, 0x6821, 0, 0 },   /* Verde (HD 7750/7770) GCN1 */
    { 0x1002, 0x6600, 0, 0 },   /* Oland (HD 8500+) GCN1 */
    { 0x1002, 0x6780, 0, 0 },   /* Cape Verde GCN1 */
    { 0x1002, 0x6640, 0, 0 },   /* Bonaire (R7 260) GCN2/Sea Islands */
    { 0x1002, 0x6658, 0, 0 },   /* Bonaire XTX GCN2 */
    { 0x1002, 0x130F, 0, 0 },   /* Kaveri/Kabini GCN2 APU iGPU */
    { 0x1002, 0x1313, 0, 0 },   /* Kaveri GCN2 APU */
    /* RDNA2/3 iGPU (amdgpu default, no module params needed). */
    { 0x1002, 0x163F, 0, 0 },   /* Van Gogh (the Deck's iGPU) RDNA2 */
    { 0x1002, 0x1640, 0, 0 },   /* Rembrandt (660M/680M) RDNA2 */
    { 0x1002, 0x164C, 0, 0 },   /* Rembrandt G */
    { 0x1002, 0x15E7, 0, 0 },   /* Cezanne/Barcelo iGPU */
    { 0x1002, 0x15BF, 0, 0 },   /* Phoenix (780M) RDNA3 */
    { 0x1002, 0x164E, 0, 0 },   /* Raphael (710M) RDNA2 desktop */
    { 0x1002, 0x15C8, 0, 0 },   /* Strix Point (890M) RDNA3.5 */
    { 0x1002, 0x1538, 0, 0 },   /* Granite Ridge (9800X3D iGPU) RDNA2 */
    { 0x1002, 0x1681, 0, 0 },   /* Kraken Point (Strix Halo) RDNA3.5 */
    /* RDNA4 discrete (Navi44/48) -- prefer AMDVLK ICD. */
    { 0x1002, 0x74C4, 0, 0 },   /* Navi48 (RX 9070) RDNA4 */
    { 0x1002, 0x74C2, 0, 0 },   /* Navi44 (RX 9060) RDNA4 */

    /* ---- Intel (0x8086) ---- */
    /* Gen8+ i915 (iris driver). */
    { 0x8086, 0x9A49, 0, 0 },   /* Tiger Lake Xe iGPU */
    { 0x8086, 0x9A40, 0, 0 },   /* Tiger Lake UHD */
    { 0x8086, 0x46A6, 0, 0 },   /* Alder Lake Xe */
    { 0x8086, 0x4680, 0, 0 },   /* Alder Lake UHD */
    { 0x8086, 0xA7A0, 0, 0 },   /* Raptor Lake Xe */
    { 0x8086, 0xA7A9, 0, 0 },   /* Raptor Lake UHD */
    { 0x8086, 0x7D45, 0, 0 },   /* Meteor Lake Arc iGPU */
    { 0x8086, 0x7D55, 0, 0 },   /* Meteor Lake UHD */
    { 0x8086, 0x5927, 0, 0 },   /* Kaby Lake Iris Plus 650 (legacy) */
    /* Xe2 (Lunar Lake / Battlemage) -- prefer the xe KMD. */
    { 0x8086, 0x7D5A, 0, 0 },   /* Lunar Lake Arc 140V */
    { 0x8086, 0xE20B, 0, 0 },   /* Battlemage (Arc B580) */

    /* NVIDIA consumer cards (bare-metal path). The vendor is 0x10DE.
     * We list the RTX 40-series (Blackwell/GA102/GA103/GA104/GA106/GA107)
     * + 30-series (Ampere) so a bare-metal WuBuOS install can bind the
     * driver to real NVIDIA hardware. */
    { 0x10DE, 0x2684, 0, 0 },   /* RTX 5050 (sm_89, Blackwell) */
    { 0x10DE, 0x2660, 0, 0 },   /* RTX 4090 */
    { 0x10DE, 0x2681, 0, 0 },   /* RTX 4080 */
    { 0x10DE, 0x2682, 0, 0 },   /* RTX 4070 */
    { 0x10DE, 0x26E8, 0, 0 },   /* RTX 4060 */
    { 0x10DE, 0x2503, 0, 0 },   /* RTX 3090 */
    { 0x10DE, 0x2504, 0, 0 },   /* RTX 3080 */
    { 0x10DE, 0x25E1, 0, 0 },   /* RTX 3070 */
    { 0x10DE, 0x2489, 0, 0 },   /* RTX 3060 */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_gpu = {
    "gpu", wubu_gpu_ids,
    sizeof(wubu_gpu_ids) / sizeof(wubu_gpu_ids[0]) - 1,  /* -1 for the terminator */
    gpu_probe,
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

#ifdef WUBU_HOSTED
/* ---- WSL2 passthrough (the magic OS detects its own runtime) ----
 *
 * On WSL2, /dev/dxg is the paravirtualized GPU. There's no PCI device,
 * no MMIO window to read, no modeset registers to poke. Instead the
 * Windows host owns the real GPU and exposes it through dxgkrnl.
 *
 * We stub in conservative defaults (the host's GPU is the RTX 4050,
 * but the paravirtualized path can't read its real VRAM). On WSL2 with
 * dzn, the Vulkan ICD (dzn_icd.json) translates Vulkan -> D3D12 ->
 * /dev/dxg -> host GPU. See wubu_hw_detect.c W4/W6 for the full ICD
 * selection chain. The kernel just needs to know "gpu present=yes,
 * platform=wsl2" so wubu_world reports it in the snapshot and the AGI
 * doesn't think the box is dead.
 *
 * Real VRAM + mode detection happens in wubu_gpu_backend.c via the
 * DirectX->Vulkan translation layer. This is KMS-lite perception only. */
int wubu_gpu_present_wsl(void)
{
    if (access("/dev/dxg", R_OK) != 0) return 0;   /* not WSL2 */

    /* stub in WSL2 defaults */
    g_gpu.present = 1;
    g_gpu.connector = WUBU_GPU_CONNECTOR_DSI;  /* Deck-style */
    g_gpu.width = 1024;  /* conservative; backend probes real mode */
    g_gpu.height = 768;
    g_gpu.refresh_hz = 60;
    g_gpu.vram_mb = 1024;  /* WSL2 GPU memory limit (GB, from .wslconfig) */
    return 1;
}
#else
/* Bare-metal kernel: no /dev/dgx, no stdio. */
int wubu_gpu_present_wsl(void) { return 0; }
#endif

/* NOTE: Vulkan ICD selection lives in wubu_hw_detect.c (W4-W7), not here.
 * The GPU driver (wubu_drv_gpu.c) handles KMS-lite display state only.
 * See wubu_hw_vulkan_icd(), wubu_hw_has_dzn(), wubu_hw_vulkan_icd_chain()
 * for the platform-aware ICD chain (dzn on WSL2, nvidia on bare metal). */
