/*
 * wubu_fpga.c -- kernel-owned FPGA driver routing (FPGA manager).
 *
 * FPGAs (field-programmable gate arrays) appear on adapters, embedded
 * SoCs (Xilinx Zynq, Altera/Intel SoC), and reconfigurable compute cards.
 * "Runs on everything" includes FPGA-accelerated systems. The kernel owns
 * the FPGA manager (fpga-mgr), regions, and bridges via /dev/fpga or
 * sysfs — the bitstream load path.
 *
 * FPGA manager subsystems:
 *   - fpga-mgr: loads bitstreams (Xilinx, Altera, Lattice, Microsemi)
 *   - fpga-region: device-tree regions with bridges + overlays
 *   - fpga-bridge: freeze bridges (altera-freeze-bridge, xlnx-pr-decoupler)
 *   - /sys/class/fpga_manager, /dev/fpga0 (userspace)
 *
 * WuBuOS owns this: detect the FPGA manager/bridges, route to the right
 * fpga-* driver, and expose the bitstream-load topology.
 *
 * Research (Kevin-Bacon 7-hop on the FPGA frontier):
 *   - fpga-mgr: Xilinx (xilinx-pr-decoupler), Altera (altera-fpga2sdram),
 *     Lattice (lattice-ecp3), Microsemi (microsemi-spi)
 *   - fpga-region: DT overlay programming (Zynq PL, SoCFPGA)
 *   - fpga-bridge: altera-freeze-bridge, xlnx-pr-decoupler
 *   - FPGA devices exposed as /dev/fpga or via VFIO for passthrough
 */
#include "wubu_fpga.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_fpga = 0;
static int  g_fpga_mgr = 0;
static int  g_fpga_region = 0;
static int  g_fpga_bridge = 0;
static char g_fpga_drv[32] = "";

/* ---- W1: probe the FPGA topology ---- */
void wubu_fpga_probe(void)
{
    g_fpga = 0; g_fpga_mgr = 0; g_fpga_region = 0; g_fpga_bridge = 0;
    g_fpga_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* FPGA manager present? */
    if (access("/sys/class/fpga_manager", R_OK) == 0 ||
        access("/dev/fpga0", R_OK) == 0 ||
        access("/sys/bus/fpga", R_OK) == 0) {
        g_fpga = 1;
        g_fpga_mgr = 1;
        strcpy(g_fpga_drv, "fpga-mgr");
    }
    /* FPGA regions (DT overlay programming)? */
    if (access("/sys/bus/platform/drivers/of-fpga-region", R_OK) == 0) {
        g_fpga_region = 1;
        g_fpga = 1;
    }
    /* FPGA bridges (freeze/decoupler)? */
    if (access("/sys/bus/platform/drivers/altera-freeze-bridge", R_OK) == 0 ||
        access("/sys/bus/platform/drivers/xlnx-pr-decoupler", R_OK) == 0) {
        g_fpga_bridge = 1;
        g_fpga = 1;
        if (!g_fpga_drv[0]) strcpy(g_fpga_drv, "fpga-bridge");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_fpga_present(void)   { return g_fpga; }
int  wubu_fpga_has_mgr(void)   { return g_fpga_mgr; }
int  wubu_fpga_has_region(void){ return g_fpga_region; }
int  wubu_fpga_has_bridge(void){ return g_fpga_bridge; }
const char *wubu_fpga_driver(void){ return g_fpga_drv[0] ? g_fpga_drv : NULL; }

/* ---- W3: FPGA manager driver routing ---- */
const char *wubu_fpga_mgr_driver(const char *vendor)
{
    if (!vendor) return NULL;
    if (strstr(vendor, "xilinx"))   return "xilinx-pr-decoupler";
    if (strstr(vendor, "altera") || strstr(vendor, "intel")) return "altera-fpga2sdram";
    if (strstr(vendor, "lattice"))  return "lattice-ecp3";
    if (strstr(vendor, "microsemi"))return "microsemi-spi";
    if (strstr(vendor, "zynq"))     return "xilinx-pr-decoupler";
    return "fpga-mgr";
}

/* ---- W4: summary ---- */
int wubu_fpga_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "fpga[present=%d mgr=%d region=%d bridge=%d drv=%s]",
        g_fpga, g_fpga_mgr, g_fpga_region, g_fpga_bridge,
        wubu_fpga_driver() ? wubu_fpga_driver() : "none");
}
