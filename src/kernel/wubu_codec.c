/*
 * wubu_codec.c -- kernel-owned audio codec/DSP driver routing.
 *
 * The codec is the chip that turns digital audio into analog (and back).
 * On HD-Audio systems the codec is a Realtek/Conexant/etc chip on the
 * HDA bus; on SoC/embedded systems it's an ASoC codec (wm8960, cs42l42,
 * rt5682). The DSP (SOF - Sound Open Firmware) offloads audio processing.
 * "Runs on everything" means the right codec gets the right driver.
 *
 * Codec drivers:
 *   - HD-Audio: snd_hda_codec_realtek (ALC/ALC2xx/ALC8xx/ALC89x),
 *     snd_hda_codec_idt (IDT/Sigmatel), snd_hda_codec_cirrus (CS),
 *     snd_hda_codec_conexant, snd_hda_codec_hdmi
 *   - ASoC codecs: snd_soc_wm8960, snd_soc_cs42l42, snd_soc_rt5682,
 *     snd_soc_nau8825, snd_soc_max98357a (amp), snd_soc_tas2770 (amp)
 *   - DSP: snd_sof_pci (SOF - Sound Open Firmware), Intel HDA DSP
 *
 * WuBuOS owns this: detect the codec (HD-Audio PCI class + ASoC cards),
 * route to the right codec driver, and flag SOF DSP presence.
 *
 * Research (Kevin-Bacon 7-hop on the codec frontier):
 *   - HD-Audio codecs: Realtek ALC family (ALC269/ALC897/ALC1220),
 *     IDT/Sigmatel, Cirrus, Conexant, HDMI (ATI/NVIDIA/Intel)
 *   - ASoC codecs: wm8960 (Wolfson), cs42l42 (Cirrus), rt5682 (Realtek),
 *     max98357a (maxim amp), tas2770 (TI amp)
 *   - SOF (Sound Open Firmware): snd_sof, DSP firmware, Intel HDA
 */
#include "wubu_codec.h"
#include "wubu_pci.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- PCI class: audio device ---- */
#define PCI_CLASS_AUDIO  0x04
#define PCI_SUBCLASS_AUDIO 0x03
#define PCI_VENDOR_REALTEK 0x10EC
#define PCI_VENDOR_IDT    0x111D
#define PCI_VENDOR_CIRRUS 0x1013
#define PCI_VENDOR_CONEXANT 0x14F1
#define PCI_VENDOR_ANALOG  0x11D4

/* ---- Global state ---- */
static int  g_codec = 0;
static int  g_hda_codec = 0;
static int  g_asoc_codec = 0;
static int  g_sof_dsp = 0;
static char g_codec_drv[40] = "";
static char g_codec_name[32] = "";

/* ---- W1: probe the audio codec topology ---- */
void wubu_codec_probe(void)
{
    g_codec = 0; g_hda_codec = 0; g_asoc_codec = 0; g_sof_dsp = 0;
    g_codec_drv[0] = '\0'; g_codec_name[0] = '\0';

#ifdef WUBU_HOSTED
    /* HD-Audio codec driver loaded? */
    if (access("/sys/bus/hdaudio/drivers/snd_hda_codec_realtek", R_OK) == 0 ||
        access("/sys/bus/hdaudio/devices", R_OK) == 0) {
        g_hda_codec = 1;
        g_codec = 1;
        strcpy(g_codec_drv, "snd_hda_codec_realtek");
        strcpy(g_codec_name, "HD-Audio codec");
    }
    /* ASoC codecs present? */
    if (access("/sys/bus/i2c/drivers/snd_soc_wm8960", R_OK) == 0 ||
        access("/sys/bus/i2c/drivers/snd_soc_cs42l42", R_OK) == 0 ||
        access("/sys/bus/i2c/drivers/snd_soc_rt5682", R_OK) == 0) {
        g_asoc_codec = 1;
        g_codec = 1;
        if (!g_codec_drv[0]) strcpy(g_codec_drv, "snd_soc");
        if (access("/sys/bus/i2c/drivers/snd_soc_wm8960", R_OK) == 0)
            strcpy(g_codec_name, "WM8960");
        else if (access("/sys/bus/i2c/drivers/snd_soc_cs42l42", R_OK) == 0)
            strcpy(g_codec_name, "CS42L42");
        else if (access("/sys/bus/i2c/drivers/snd_soc_rt5682", R_OK) == 0)
            strcpy(g_codec_name, "RT5682");
    }
    /* SOF DSP (Sound Open Firmware)? */
    if (access("/sys/bus/pci/drivers/snd_sof_pci", R_OK) == 0 ||
        access("/lib/firmware/intel/sof", R_OK) == 0) {
        g_sof_dsp = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_codec_present(void)    { return g_codec; }
int  wubu_codec_has_hda(void)    { return g_hda_codec; }
int  wubu_codec_has_asoc(void)   { return g_asoc_codec; }
int  wubu_codec_has_sof_dsp(void){ return g_sof_dsp; }
const char *wubu_codec_driver(void){ return g_codec_drv[0] ? g_codec_drv : NULL; }
const char *wubu_codec_name(void){ return g_codec_name[0] ? g_codec_name : NULL; }

/* ---- W3: codec driver routing ---- */
const char *wubu_codec_hda_driver(const char *vendor)
{
    if (!vendor) return NULL;
    if (strstr(vendor, "realtek"))  return "snd_hda_codec_realtek";
    if (strstr(vendor, "idt"))      return "snd_hda_codec_idt";
    if (strstr(vendor, "cirrus"))   return "snd_hda_codec_cirrus";
    if (strstr(vendor, "conexant")) return "snd_hda_codec_conexant";
    if (strstr(vendor, "analog"))   return "snd_hda_codec_analog";
    if (strstr(vendor, "hdmi"))     return "snd_hda_codec_hdmi";
    return "snd_hda_codec_generic";
}

const char *wubu_codec_asoc_driver(const char *chip)
{
    if (!chip) return NULL;
    if (strstr(chip, "wm8960"))   return "snd_soc_wm8960";
    if (strstr(chip, "cs42l42"))  return "snd_soc_cs42l42";
    if (strstr(chip, "rt5682"))   return "snd_soc_rt5682";
    if (strstr(chip, "nau8825"))  return "snd_soc_nau8825";
    if (strstr(chip, "max98357")) return "snd_soc_max98357a";
    if (strstr(chip, "tas2770"))  return "snd_soc_tas2770";
    return "snd_soc_generic";
}

/* ---- W4: summary ---- */
int wubu_codec_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "codec[present=%d hda=%d asoc=%d sof=%d drv=%s name=%s]",
        g_codec, g_hda_codec, g_asoc_codec, g_sof_dsp,
        wubu_codec_driver() ? wubu_codec_driver() : "none",
        wubu_codec_name() ? wubu_codec_name() : "-");
}
