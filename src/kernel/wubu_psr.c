/*
 * wubu_psr.c -- kernel-owned display PSR + NIC SR-IOV routing.
 *
 * Two power/virtualization capabilities:
 *   - PSR (panel self-refresh): eDP displays refresh from their own
 *     framebuffer when idle, saving power. i915/amdgpu support PSR.
 *   - SR-IOV (single-root I/O virtualization): a NIC/GPU PF exposes
 *     virtual functions (VFs) to VMs. ixgbe/i40e/ice support it.
 *
 * WuBuOS owns this: detect PSR capability + SR-IOV PF/VF support, route
 * to the right driver, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the PSR/SR-IOV frontier):
 *   - PSR: panel_self_refresh in i915 (eDP), amdgpu PSR; /sys panel
 *   - SR-IOV: sriov_numvfs sysfs node; ixgbe/i40e/ice PF
 *   - VF: virtio or VF drivers (ixgbevf, i40evf, ice_vf)
 */
#include "wubu_psr.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_psr = 0;
static int  g_sriov = 0;
static int  g_vf = 0;           /* virtual function in use */
static int  g_num_vfs = 0;
static char g_psr_drv[24] = "";

/* ---- W1: probe the PSR/SR-IOV topology ---- */
void wubu_psr_probe(void)
{
    g_psr = 0; g_sriov = 0; g_vf = 0; g_num_vfs = 0;
    g_psr_drv[0] = '\0';

#ifdef _GNU_SOURCE
    /* PSR: i915/amdgpu eDP panel self-refresh. */
    if (access("/sys/bus/pci/drivers/i915", R_OK) == 0) {
        g_psr = 1;
        strcpy(g_psr_drv, "i915-psr");
    } else if (access("/sys/bus/pci/drivers/amdgpu", R_OK) == 0) {
        g_psr = 1;
        strcpy(g_psr_drv, "amdgpu-psr");
    }
    /* SR-IOV: check for capable PF drivers. */
    if (wubu_hw_is_wsl()) return;
    if (access("/sys/bus/pci/drivers/ixgbe", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/i40e", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/ice", R_OK) == 0) {
        g_sriov = 1;
        g_num_vfs = 8;  /* typical VF count */
    }
    /* VF driver in use? */
    if (access("/sys/bus/pci/drivers/ixgbevf", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/i40evf", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/ice_vf", R_OK) == 0) {
        g_vf = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_psr_supported(void)  { return g_psr; }
int  wubu_psr_sriov(void)      { return g_sriov; }
int  wubu_psr_vf(void)         { return g_vf; }
int  wubu_psr_num_vfs(void)    { return g_num_vfs; }
const char *wubu_psr_driver(void){ return g_psr_drv[0] ? g_psr_drv : NULL; }

/* ---- W3: routing ---- */
const char *wubu_psr_driver_for(const char *gpu)
{
    if (!gpu) return NULL;
    if (strstr(gpu, "i915") || strstr(gpu, "intel")) return "i915-psr";
    if (strstr(gpu, "amdgpu") || strstr(gpu, "amd")) return "amdgpu-psr";
    if (strstr(gpu, "xe"))     return "xe-psr";
    return "drm-psr";
}

const char *wubu_psr_sriov_for(const char *nic)
{
    if (!nic) return NULL;
    if (strstr(nic, "ixgbe"))  return "ixgbe";
    if (strstr(nic, "i40e"))   return "i40e";
    if (strstr(nic, "ice"))    return "ice";
    if (strstr(nic, "mlx5"))   return "mlx5";
    return "sriov";
}

/* ---- W4: summary ---- */
int wubu_psr_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "psr[psr=%d(%s) sriov=%d vf=%d vfs=%d]",
        g_psr, wubu_psr_driver() ? wubu_psr_driver() : "none",
        g_sriov, g_vf, g_num_vfs);
}
