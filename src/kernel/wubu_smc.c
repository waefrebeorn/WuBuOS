/*
 * wubu_smc.c -- kernel-owned GPU SMC firmware routing.
 *
 * SMC (System Management Controller) firmware manages GPU power states.
 * "Runs on everything" includes correct SMC on every GPU variant.
 *
 * SMC:
 *   - AMD: SMC firmware, SMU (System Management Unit)
 *   - VCN: Video Coding Engine firmware
 *   - UVD: Unified Video Decoder firmware
 *   - /sys/class/drm/card*: power state, firmware
 *   - amdgpu: smu, ppfeaturemask
 *   - fw: firmware version, loaded, verified
 *
 * WuBuOS owns this: detect SMC + SMU + firmware, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the smc frontier):
 *   -AMD SMC/SMU firmware
 *   - GPU firmware loading
 */
#include "wubu_smc.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_smc = 0;         /* SMC present */
static int  g_smu = 0;         /* SMU firmware */
static int  g_vcn = 0;         /* VCN */
static int  g_uvd = 0;         /* UVD */
static int  g_fw = 0;          /* firmware loaded */
static char g_smc_drv[24] = "";

void wubu_smc_probe(void)
{
    g_smc = 0; g_smu = 0; g_vcn = 0; g_uvd = 0; g_fw = 0;
    g_smc_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/module/amdgpu", R_OK) == 0) {
        g_smc = 1; g_smu = 1; g_vcn = 1; g_uvd = 1; g_fw = 1;
        strcpy(g_smc_drv, "smu-fw");
    }
    if (access("/sys/module/radeon", R_OK) == 0) {
        g_smc = 1; g_fw = 1;
        if (!g_smc_drv[0]) strcpy(g_smc_drv, "radeon-smc");
    }
    if (access("/sys/module/i915", R_OK) == 0) {
        g_smc = 1; g_fw = 1;
        if (!g_smc_drv[0]) strcpy(g_smc_drv, "i915-fw");
    }
#endif
}

int  wubu_smc_present(void){ return g_smc; }
int  wubu_smc_smu(void)    { return g_smu; }
int  wubu_smc_vcn(void)    { return g_vcn; }
int  wubu_smc_uvd(void)    { return g_uvd; }
int  wubu_smc_fw(void)     { return g_fw; }
const char *wubu_smc_driver(void){ return g_smc_drv[0] ? g_smc_drv : NULL; }

const char *wubu_smc_block_for(const char *b)
{
    if (!b) return NULL;
    if (strstr(b, "smu") || strstr(b, "smu1")) return "SMU";
    if (strstr(b, "vcn")) return "VCN";
    if (strstr(b, "uvd")) return "UVD";
    if (strstr(b, "gfx")) return "GFX";
    if (strstr(b, "cpu")) return "CPU";
    return "SMU";
}

const char *wubu_smc_state_for(const char *s)
{
    if (!s) return NULL;
    if (strstr(s, "load") || strstr(s, "boot")) return "loading";
    if (strstr(s, "done") || strstr(s, "ready")) return "ready";
    if (strstr(s, "fail") || strstr(s, "error")) return "failed";
    if (strstr(s, "verif")) return "verifying";
    return "loading";
}

int wubu_smc_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "smc[smc=%d smu=%d vcn=%d uvd=%d fw=%d drv=%s]",
        g_smc, g_smu, g_vcn, g_uvd, g_fw,
        wubu_smc_driver() ? wubu_smc_driver() : "none");
}
