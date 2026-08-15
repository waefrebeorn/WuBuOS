/*
 * wubu_ddcci.c -- kernel-owned display panel DDC/CI control routing.
 *
 * DDC/CI (Display Data Channel / Command Interface) lets the host control
 * brightness, contrast, and OSD over I2C. "Runs on everything" includes
 * correct display control on every panel.
 *
 * DDC/CI:
 *   - /dev/i2c: DDC bus (i2c_dev)
 *   - /sys/bus/i2c/devices/N-0050: EDID EEPROM
 *   - CEC: consumer electronics control
 *   - i2c: I2C bus, /sys/class/i2c-adapter
 *   - brightness: 0x10 (VCB), contrast: 0x12
 *   - backlight: backlight class, led driver
 *
 * WuBuOS owns this: detect DDC/CI + CEC + i2c, route to the right
 * driver, expose the topology.
 *
 * Research (7-hop on the DDC/CI frontier):
 *   - DDC/CI command set (VCB brightness 0x10)
 *   - i2c_dev, i2c-adapter
 *   - CEC, EDID EEPROM
 */
#include "wubu_ddcci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int  g_ddc = 0;         /* DDC/CI present */
static int  g_i2c = 0;         /* i2c bus */
static int  g_cec = 0;         /* CEC */
static int  g_edid = 0;        /* EDID */
static int  g_ctrl = 0;        /* display control */
static char g_ddcci_drv[24] = "";

void wubu_ddcci_probe(void)
{
    g_ddc = 0; g_i2c = 0; g_cec = 0; g_edid = 0; g_ctrl = 0;
    g_ddcci_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/class/i2c-adapter", R_OK) == 0 ||
        access("/dev/i2c-0", R_OK) == 0) {
        g_ddc = 1; g_i2c = 1; g_ctrl = 1;
        strcpy(g_ddcci_drv, "i2c-ddcci");
    }
    if (access("/sys/class/cec", R_OK) == 0) {
        g_cec = 1;
        if (!g_ddcci_drv[0]) strcpy(g_ddcci_drv, "cec");
    }
    if (access("/sys/class/drm", R_OK) == 0) {
        g_edid = 1; g_ctrl = 1;
        if (!g_ddcci_drv[0]) strcpy(g_ddcci_drv, "drm-ddc");
    }
    if (access("/sys/class/backlight", R_OK) == 0) {
        g_ctrl = 1;
        if (!g_ddcci_drv[0]) strcpy(g_ddcci_drv, "backlight-ctrl");
    }
#endif
}

int  wubu_ddcci_present(void){ return g_ddc; }
int  wubu_ddcci_i2c(void)     { return g_i2c; }
int  wubu_ddcci_cec(void)     { return g_cec; }
int  wubu_ddcci_edid(void)    { return g_edid; }
int  wubu_ddcci_ctrl(void)    { return g_ctrl; }
const char *wubu_ddcci_driver(void){ return g_ddcci_drv[0] ? g_ddcci_drv : NULL; }

const char *wubu_ddcci_cmd_for(const char *c)
{
    if (!c) return NULL;
    if (strstr(c, "brightness") || strstr(c, "vcb")) return "0x10";
    if (strstr(c, "contrast")) return "0x12";
    if (strstr(c, "osd"))     return "0x60";
    if (strstr(c, "power"))   return "0x6D";
    if (strstr(c, "input"))   return "0x60";
    return "0x10";
}

const char *wubu_ddcci_bus_for(const char *b)
{
    if (!b) return NULL;
    if (strstr(b, "ddc"))  return "ddc-bus";
    if (strstr(b, "i2c"))  return "i2c-bus";
    if (strstr(b, "cec"))  return "cec-bus";
    return "ddc-bus";
}

int wubu_ddcci_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "ddcci[ddc=%d i2c=%d cec=%d edid=%d ctrl=%d drv=%s]",
        g_ddc, g_i2c, g_cec, g_edid, g_ctrl,
        wubu_ddcci_driver() ? wubu_ddcci_driver() : "none");
}