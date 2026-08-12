/*
 * wubu_accel.c -- kernel-owned NPU/accelerator driver routing.
 *
 * Modern "AI PCs" ship an on-die NPU (Neural Processing Unit). "Runs on
 * everything" includes AI inference acceleration — the kernel must route
 * the NPU to the right driver and expose it to the runtime (through the
 * accel subsystem for Intel/AMD, or dedicated SDKs for Qualcomm/NVIDIA).
 *
 * NPU drivers per vendor:
 *   - Intel: IVPU (integrated VPU) -> accel/ivpu.ko, /dev/accel/accel0
 *   - AMD:   XDNA (Ryzen AI) -> amdxdna.ko (xdna-driver), /dev/accel/accel0
 *   - Qualcomm: Hexagon NPU (Snapdragon) -> via Linux accel (qaic) or SDK
 *   - NVIDIA: NVDLA (open), /dev/nvdla or accel
 *   - Google: TPU (data center) via gasket/edgetpu (Coral)
 *
 * WuBuOS owns this: detect the NPU (via PCI class / sysfs / accel), route
 * to the right driver, and expose the accelerator topology.
 *
 * Research (Kevin-Bacon 7-hop on the accelerator frontier):
 *   - Linux accel subsystem: drivers/accel/ (IVPU, AMDXDNA, QAIC)
 *   - intel/linux-npu-driver: Intel NPU (Meteor Lake+, Arrow Lake, Lunar Lake)
 *   - amd/xdna-driver: AMD Ryzen AI NPU (XDNA, XDNA2, Strix Point)
 *   - NVDLA (NVIDIA open accelerator), Qualcomm Hexagon SDK
 */
#include "wubu_accel.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- PCI classes: accelerators ---- */
#define PCI_CLASS_COMPUTE  0x12   /* Processing accelerators */
#define PCI_SUBCLASS_NPU   0x01   /* AI/NPU accelerator */
#define PCI_SUBCLASS_DSP   0x03   /* DSP */
#define PCI_CLASS_COPROC   0x0B   /* Signal processing */
#define PCI_SUBCLASS_PROC  0x40   /* Processor (NPU on some) */
#define PCI_VENDOR_INTEL   0x8086
#define PCI_VENDOR_AMD     0x1022
#define PCI_VENDOR_QUALCOMM 0x17CB
#define PCI_VENDOR_GOOGLE  0x1AE0

/* ---- Global state ---- */
static int  g_accel = 0;         /* accel subsystem present */
static int  g_npu = 0;           /* NPU present */
static int  g_dsp = 0;           /* DSP present */
static char g_accel_drv[32] = "";
static char g_npu_name[48] = "";
static int  g_npu_vendor = 0;

/* ---- W1: probe the accelerator topology ---- */
void wubu_accel_probe(void)
{
    g_accel = 0; g_npu = 0; g_dsp = 0; g_npu_vendor = 0;
    g_accel_drv[0] = '\0'; g_npu_name[0] = '\0';

#ifdef _GNU_SOURCE
    /* Linux accel subsystem present (accel driver loaded)? */
    if (access("/sys/class/accel", R_OK) == 0 ||
        access("/dev/accel/accel0", R_OK) == 0) {
        g_accel = 1;
        g_npu = 1;
        strcpy(g_accel_drv, "accel");
    }

    /* Bare metal: scan PCI for NPU/DSP accelerator devices. */
    if (wubu_hw_is_wsl()) return;
    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
    for (int i = 0; i < n; i++) {
        if ((devs[i].class_code >> 8) == PCI_CLASS_COMPUTE &&
            devs[i].subclass == PCI_SUBCLASS_NPU) {
            g_npu = 1;
            g_npu_vendor = devs[i].vendor;
            if (devs[i].vendor == PCI_VENDOR_INTEL) {
                strcpy(g_npu_name, "Intel IVPU");
                strcpy(g_accel_drv, "ivpu");
            } else if (devs[i].vendor == PCI_VENDOR_AMD) {
                strcpy(g_npu_name, "AMD XDNA");
                strcpy(g_accel_drv, "amdxdna");
            } else if (devs[i].vendor == PCI_VENDOR_QUALCOMM) {
                strcpy(g_npu_name, "Qualcomm Hexagon NPU");
                strcpy(g_accel_drv, "qaic");
            } else {
                strcpy(g_npu_name, "NPU accelerator");
                strcpy(g_accel_drv, "accel");
            }
            break;
        }
        if ((devs[i].class_code >> 8) == PCI_CLASS_COPROC) {
            g_dsp = 1;
        }
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_accel_present(void)   { return g_accel; }
int  wubu_accel_has_npu(void)   { return g_npu; }
int  wubu_accel_has_dsp(void)   { return g_dsp; }
int  wubu_accel_npu_vendor(void){ return g_npu_vendor; }
const char *wubu_accel_driver(void){ return g_accel_drv[0] ? g_accel_drv : NULL; }
const char *wubu_accel_npu_name(void){ return g_npu_name[0] ? g_npu_name : NULL; }

/* ---- W3: NPU driver routing per vendor ---- */
const char *wubu_accel_npu_driver(int vendor)
{
    switch (vendor) {
    case PCI_VENDOR_INTEL:    return "ivpu";
    case PCI_VENDOR_AMD:      return "amdxdna";
    case PCI_VENDOR_QUALCOMM: return "qaic";
    case PCI_VENDOR_GOOGLE:   return "edgetpu";
    default:                  return "accel";
    }
}

/* ---- W4: summary ---- */
int wubu_accel_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "accel[present=%d npu=%d(%s) dsp=%d drv=%s]",
        g_accel, g_npu,
        wubu_accel_npu_name() ? wubu_accel_npu_name() : "none",
        g_dsp,
        wubu_accel_driver() ? wubu_accel_driver() : "none");
}
