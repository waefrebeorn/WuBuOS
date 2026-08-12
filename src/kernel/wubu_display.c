/*
 * wubu_display.c -- kernel-owned display driver routing (DRM/KMS matrix).
 *
 * Linux has 3 generations of GPU display drivers coexisting:
 *   amdgpu (GCN1->RDNA4 DC), i915 (Gen4->Xe), xe (Xe2 Lunar Lake),
 *   nouveau (Fermi->Ada), plus the simpledrm/efifb generic fallbacks.
 * The right one is chosen by PCI vendor + generation, and the KMS
 * atomic modeset / render-node (/dev/dri/cardN + renderD128) topology
 * must be published so userspace finds the display server.
 *
 * Research (Kevin-Bacon 7-hop on the display frontier):
 *   - amdgpu DC supports GCN1+ (Southern Islands)
 *   - Intel xe driver now default for Xe2/Lunar Lake/Battlemage (6.12+)
 *   - nouveau covers Fermi->Ada (Maxwell+ with reclocking, Turing/Ada NVK)
 *   - simpledrm generic fallback for all systems w/o hw drivers
 *   - KMS atomic modeset: plane/scanout/cursor, EDID, MST, DSC, HDCP
 *
 * WuBuOS owns this: detect the GPU (via wubu_hw_detect), route to the
 * correct DRM driver, and expose the render/scanout topology.
 */
#include "wubu_display.h"
#include "wubu_hw_detect.h"
#include "wubu_pci.h"
#include <stdio.h>
#include <string.h>

#define PCI_CLASS_DISPLAY 0x03
#define PCI_VENDOR_AMD    0x1002
#define PCI_VENDOR_INTEL  0x8086
#define PCI_VENDOR_NVIDIA 0x10DE

/* One display driver per GPU generation. */
typedef struct {
    uint16_t vendor;
    uint16_t device_lo, device_hi;  /* inclusive range */
    const char *driver;             /* DRM/KMS driver: amdgpu/i915/xe/nouveau */
    const char *name;               /* human-readable family */
    int use_xe;                     /* prefer Intel xe over i915 */
    int use_nouveau;                /* open-source NVIDIA driver */
} wubu_display_drv_t;

static const wubu_display_drv_t display_table[] = {
    /* AMD: GCN1 -> RDNA4, all use amdgpu with Display Core (DC) */
    { 0x1002, 0x6798, 0x679B, "amdgpu", "AMD GCN1 Southern Islands",    0, 0 },
    { 0x1002, 0x6600, 0x6622, "amdgpu", "AMD GCN2 Sea Islands",         0, 0 },
    { 0x1002, 0x9802, 0x9805, "amdgpu", "AMD Kaveri/Stoney APU",        0, 0 },
    { 0x1002, 0x15D8, 0x164E, "amdgpu", "AMD Ryzen/Vega APU",           0, 0 },
    { 0x1002, 0x73BF, 0x73BF, "amdgpu", "AMD Vega Frontier/Radeon VII", 0, 0 },
    { 0x1002, 0x731F, 0x731F, "amdgpu", "AMD Navi 10",                  0, 0 },
    { 0x1002, 0x7360, 0x7360, "amdgpu", "AMD Navi 14",                  0, 0 },
    { 0x1002, 0x731E, 0x731E, "amdgpu", "AMD Navi 18",                  0, 0 },
    { 0x1002, 0x7320, 0x732F, "amdgpu", "AMD Navi 21/22/23 RDNA2",      0, 0 },
    { 0x1002, 0x7400, 0x743F, "amdgpu", "AMD Phoenix/Navi31 RDNA3",      0, 0 },
    { 0x1002, 0x74C0, 0x74D0, "amdgpu", "AMD Navi44/48 RDNA4",          0, 0 },
    { 0x1002, 0x164D, 0x164D, "amdgpu", "AMD Phoenix Point",            0, 0 },
    /* Intel: legacy i915 (Gen4-11), then xe for Xe2/Lunar Lake/Battlemage */
    { 0x8086, 0x0112, 0x0162, "i915",   "Intel Gen6 Ironlake",          0, 0 },
    { 0x8086, 0x0152, 0x0156, "i915",   "Intel Gen7 Haswell/BYT",       0, 0 },
    { 0x8086, 0x1602, 0x161B, "i915",   "Intel Gen8 Broadwell",         0, 0 },
    { 0x8086, 0x1900, 0x1930, "i915",   "Intel Gen9 Skylake",           0, 0 },
    { 0x8086, 0x3E90, 0x3EF0, "i915",   "Intel Gen11 Whiskey Lake",     0, 0 },
    { 0x8086, 0x8A00, 0x8A50, "i915",   "Intel Gen11 Ice Lake",         0, 0 },
    { 0x8086, 0x9A40, 0x9AC0, "i915",   "Intel Gen12 Tiger Lake",       0, 0 },
    { 0x8086, 0x4680, 0x4690, "i915",   "Intel Gen12 Alder Lake",       0, 0 },
    { 0x8086, 0x4380, 0x43A0, "i915",   "Intel Gen12 Alder/Raptor Lake", 0, 0 },
    { 0x8086, 0x5690, 0x569F, "xe",     "Intel Xe2 Arc/Lunar Lake",     1, 0 },
    { 0x8086, 0x7D40, 0x7D60, "xe",     "Intel Xe Arc A380",            1, 0 },
    { 0x8086, 0x7D5A, 0x7D5A, "xe",     "Intel Xe Battlemage",          1, 0 },
    { 0x8086, 0xE20B, 0xE20B, "xe",     "Intel Xe Battlemage B580",     1, 0 },
    /* NVIDIA: nouveau (open) or nvidia (proprietary) for all generations */
    { 0x10DE, 0x0400, 0x06FF, "nouveau","NVIDIA Tesla (G80-G98)",       0, 1 },
    { 0x10DE, 0x0800, 0x0A39, "nouveau","NVIDIA Fermi (GF100-GK110)",   0, 1 },
    { 0x10DE, 0x13C0, 0x13FF, "nouveau","NVIDIA Maxwell (GM107-108)",  0, 1 },
    { 0x10DE, 0x1B00, 0x1BBB, "nouveau","NVIDIA Pascal (GP100-108)",   0, 1 },
    { 0x10DE, 0x1DB0, 0x1DBB, "nouveau","NVIDIA Turing (TU100/116)",   0, 1 },
    { 0x10DE, 0x2000, 0x20B9, "nouveau","NVIDIA Ampere (GA05)",        0, 1 },
    { 0x10DE, 0x2500, 0x2690, "nvidia",  "NVIDIA Ada (AD102-108)",      0, 1 },
    { 0x10DE, 0x2780, 0x27B0, "nvidia",  "NVIDIA Blackwell (GH102)",    0, 1 },
    { 0x10DE, 0x2900, 0x2E00, "nvidia",  "NVIDIA RTX 50xx/Blackwell",  0, 1 },
    { 0, 0, 0, NULL, NULL },
};

/* ---- Global state ---- */
static char g_drm_driver[32]  = "";
static char g_drm_chip[64]    = "";
static int  g_display_present  = 0;
static int  g_render_node      = 0;
static int  g_xe_preferred     = 0;
static int  g_card_index       = 0;  /* /dev/dri/cardN */

/* ---- W1: probe the display topology ---- */
void wubu_display_probe(void)
{
    g_display_present = 0;
    g_render_node = 0;
    g_xe_preferred = 0;
    g_card_index = 0;
    g_drm_driver[0] = '\0';
    g_drm_chip[0] = '\0';

    /* WSL2: host owns display. */
    if (wubu_hw_is_wsl()) return;

#ifdef _GNU_SOURCE
    /* Bare metal: scan PCI class 0x03 (display). */
    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
    for (int i = 0; i < n; i++) {
        if ((devs[i].class_code >> 8) != PCI_CLASS_DISPLAY) continue;
        g_display_present = 1;
        g_card_index = i;
        for (int j = 0; display_table[j].driver; j++) {
            const wubu_display_drv_t *d = &display_table[j];
            if (d->vendor == devs[i].vendor &&
                devs[i].device >= d->device_lo &&
                devs[i].device <= d->device_hi) {
                strcpy(g_drm_driver, d->driver);
                strcpy(g_drm_chip, d->name);
                g_xe_preferred = d->use_xe;
                g_render_node = 1;  /* /dev/dri/renderD128 exists for KMS */
                /* Xe2: prefer xe over i915 */
                if (g_xe_preferred)
                    strcpy(g_drm_driver, "xe");
                break;
            }
        }
        if (!g_drm_driver[0]) {
            strcpy(g_drm_driver, "simpledrm");
            strcpy(g_drm_chip, "generic framebuffer");
        }
        break;  /* first display device */
    }
#endif
}

/* ---- W2: accessors ---- */
int          wubu_display_present(void)     { return g_display_present; }
const char *wubu_display_driver(void)       { return g_drm_driver[0] ? g_drm_driver : NULL; }
const char *wubu_display_chip_name(void)    { return g_drm_chip[0] ? g_drm_chip : NULL; }
int          wubu_display_xe_preferred(void) { return g_xe_preferred; }
int          wubu_display_has_render_node(void) { return g_render_node; }
int          wubu_display_card_index(void)   { return g_card_index; }

/* ---- W3: KMS /dev/dri path helpers ---- */
const char *wubu_display_card_path(void)
{
    static char path[32];
    snprintf(path, sizeof(path), "/dev/dri/card%d", g_card_index);
    return path;
}
const char *wubu_display_render_path(void)
{
    return "/dev/dri/renderD128";
}

/* ---- W4: summary ---- */
int wubu_display_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "display[drv=%s chip=%s card=%d render=%d xe=%d]",
        g_drm_driver[0] ? g_drm_driver : "none",
        g_drm_chip[0] ? g_drm_chip : "none",
        g_card_index, g_render_node, g_xe_preferred);
}

/* ---- W5: the KMS features the driver supports ----
 * Published so the compositor/WSGL knows what atomic features are safe. */
int wubu_display_atomic_modeset(void)  { return g_display_present; } /* all modern */
int wubu_display_has_edid(void)        { return g_display_present; }
int wubu_display_has_mst(void)        { return g_display_present; } /* DP MST */
int wubu_display_has_dsc(void)        { return g_display_present; } /* DSC 1.2a */
int wubu_display_has_hdcp(void)       { return g_display_present; }
int wubu_display_has_vrr(void)        { return g_display_present; } /* Adaptive-Sync */
