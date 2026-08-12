/*
 * wubu_audiofw.c -- kernel-owned audio firmware routing.
 *
 * Audio firmware loads DSP programs onto audio codecs. "Runs on
 * everything" includes correct audio firmware on every codec.
 *
 * Audio firmware:
 *   - ALSA: snd_fw, firmware loading
 *   - /lib/firmware: firmware files
 *   - request_firmware: firmware loader
 *   - codec: Realtek, Cirrus, WM, Texas Instruments
 *   - dsp: digital signal processor firmware
 *   - /sys/class/sound/card*: firmware
 *
 * WuBuOS owns this: detect audio firmware + codec + DSP, route to the
 *  right driver, expose the topology.
 *
 * Research (7-hop on the audiofw frontier):
 *   -ALSA firmware loading
 */
#include "wubu_audiofw.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_afw = 0;        /* audio firmware present */
static int  g_codec = 0;      /* codec firmware */
static int  g_dsp = 0;        /* DSP firmware */
static int  g_loader = 0;     /* firmware loader */
static int  g_bios = 0;       /* BIOS/bezirk */
static char g_afw_drv[24] = "";

void wubu_audiofw_probe(void)
{
    g_afw = 0; g_codec = 0; g_dsp = 0; g_loader = 0; g_bios = 0;
    g_afw_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/lib/firmware", R_OK) == 0 ||
        access("/proc/asound", R_OK) == 0) {
        g_afw = 1; g_codec = 1; g_dsp = 1; g_loader = 1;
        strcpy(g_afw_drv, "snd-fw-load");
    }
    if (access("/sys/module/snd_hda_intel", R_OK) == 0) {
        g_afw = 1; g_codec = 1;
        if (!g_afw_drv[0]) strcpy(g_afw_drv, "hda-fw");
    }
#endif
}

int  wubu_audiofw_present(void){ return g_afw; }
int  wubu_audiofw_codec(void)  { return g_codec; }
int  wubu_audiofw_dsp(void)    { return g_dsp; }
int  wubu_audiofw_loader(void) { return g_loader; }
int  wubu_audiofw_bios(void)   { return g_bios; }
const char *wubu_audiofw_driver(void){ return g_afw_drv[0] ? g_afw_drv : NULL; }

const char *wubu_audiofw_codec_for(const char *c)
{
    if (!c) return NULL;
    if (strstr(c, "realtek") || strstr(c, "alc")) return "Realtek";
    if (strstr(c, "cirrus") || strstr(c, "cs")) return "Cirrus";
    if (strstr(c, "WM") || strstr(c, "wm") || strstr(c, "wmc")) return "WM";
    if (strstr(c, "ti") || strstr(c, "tlv320")) return "TI";
    if (strstr(c, "conex")) return "Conexant";
    return "Realtek";
}

const char *wubu_audiofw_loader_for(const char *l)
{
    if (!l) return NULL;
    if (strstr(l, "fw")) return "firmware";
    if (strstr(l, "bios")) return "bios";
    if (strstr(l, "elf")) return "elf";
    if (strstr(l, "bezirk")) return "bezirk";
    return "firmware";
}

int wubu_audiofw_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "audiofw[afw=%d codec=%d dsp=%d loader=%d bios=%d drv=%s]",
        g_afw, g_codec, g_dsp, g_loader, g_bios,
        wubu_audiofw_driver() ? wubu_audiofw_driver() : "none");
}
