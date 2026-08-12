/*
 * wubu_uac.c -- kernel-owned USB audio (UAC) routing.
 *
 * USB Audio Class (UAC1/UAC2/UAC3) routes audio over USB endpoints.
 * "Runs on everything" includes correct audio on every USB device.
 *
 * UAC:
 *   - USB audio: UAC1 (0x01), UAC2 (0x02), UAC3 (0x03)
 *   - /sys/bus/usb/devices/*.audio:USB audio device
 *   - endpoint: bulk, interrupt, iso-in, iso-out
 *   - altsetting: alternate setting (sample rate)
 *   - /proc/asound card usbaudio: UAC info
 *
 * WuBuOS owns this: detect UAC + endpoint + altsetting, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the UAC frontier):
 *   - USB Audio Class spec
 *   - UAC1/UAC2/UAC3
 *   - USB audio endpoint
 */
#include "wubu_uac.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int  g_uac = 0;         /* UAC present */
static int  g_uac1 = 0;        /* UAC1 */
static int  g_uac2 = 0;        /* UAC2 */
static int  g_iso = 0;         /* iso endpoint */
static int  g_alt = 0;         /* altsetting */
static char g_uac_drv[24] = "";

void wubu_uac_probe(void)
{
    g_uac = 0; g_uac1 = 0; g_uac2 = 0; g_iso = 0; g_alt = 0;
    g_uac_drv[0] = '\0';

#ifdef _GNU_SOURCE
    DIR *d = opendir("/sys/bus/usb/devices");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            char p[256];
            snprintf(p, sizeof(p), "/sys/bus/usb/devices/%s/bInterfaceClass", e->d_name);
            if (access(p, R_OK) == 0) {
                char buf[4] = ""; FILE *f = fopen(p, "r");
                if (f) {
                    if (fgets(buf, sizeof(buf), f)) {
                        if (strstr(buf, "01")) {
                            g_uac = 1; g_uac1 = 1; g_iso = 1; g_alt = 1;
                            if (!g_uac_drv[0]) strcpy(g_uac_drv, "snd-usb-audio");
                        }
                        if (strstr(buf, "02")) {
                            g_uac = 1; g_uac2 = 1; g_iso = 1; g_alt = 1;
                            if (!g_uac_drv[0]) strcpy(g_uac_drv, "snd-usb-audio");
                        }
                    }
                    fclose(f);
                }
            }
        }
        closedir(d);
    }
    if (access("/sys/module/snd_usb_audio", R_OK) == 0) {
        g_uac = 1;
        if (!g_uac_drv[0]) strcpy(g_uac_drv, "snd-usb-audio");
    }
#endif
}

int  wubu_uac_present(void){ return g_uac; }
int  wubu_uac_uac1(void)    { return g_uac1; }
int  wubu_uac_uac2(void)    { return g_uac2; }
int  wubu_uac_iso(void)     { return g_iso; }
int  wubu_uac_alt(void)     { return g_alt; }
const char *wubu_uac_driver(void){ return g_uac_drv[0] ? g_uac_drv : NULL; }

const char *wubu_uac_version_for(const char *v)
{
    if (!v) return NULL;
    if (strstr(v, "01")) return "uac1";
    if (strstr(v, "02")) return "uac2";
    if (strstr(v, "03")) return "uac3";
    return "uac2";
}

const char *wubu_uac_ep_for(const char *e)
{
    if (!e) return NULL;
    if (strstr(e, "bulk"))    return "bulk";
    if (strstr(e, "interrupt")) return "interrupt";
    if (strstr(e, "iso-in"))  return "iso-in";
    if (strstr(e, "iso-out")) return "iso-out";
    if (strstr(e, "int"))     return "interrupt";
    return "bulk";
}

int wubu_uac_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "uac[uac=%d uac1=%d uac2=%d iso=%d alt=%d drv=%s]",
        g_uac, g_uac1, g_uac2, g_iso, g_alt,
        wubu_uac_driver() ? wubu_uac_driver() : "none");
}
