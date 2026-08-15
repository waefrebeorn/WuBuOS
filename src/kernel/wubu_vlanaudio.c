/*
 * wubu_vlanaudio.c -- kernel-owned NIC VLAN + audio DSP/mixing routing.
 *
 * Two distinct capabilities:
 *   - NIC VLAN (802.1Q): virtual LAN tagging offloaded to the NIC.
 *     "Runs on everything" includes network segmentation.
 *   - Audio DSP: the audio processing graph (PipeWire/ALSA), equalizer,
 *     mixing, and DSP offload. "Runs on everything" includes great sound.
 *
 * VLAN support:
 *   - 802.1Q VLAN: vlan.ko, 8021q, /proc/net/vlan; NIC vlan_offload
 *   - VLAN offload: rx-vlan-offload, tx-vlan-offload (ethtool -k)
 *   - bridge VLAN filtering: br_vlan
 *
 * Audio DSP:
 *   - PipeWire: the modern audio graph (replaces PulseAudio), pw-cli
 *   - ALSA: asound, mixing, dmix (software mixer)
 *   - EQ: parametric equalizer (easyeffects / pipewire filters)
 *   - DSP offload: SOF (Sound Open Firmware), DSP buffer/filter
 *
 * WuBuOS owns this: detect VLAN capability + the audio DSP graph, route to
 * the right driver, and expose the VLAN + audio-processing topology.
 *
 * Research (Kevin-Bacon 7-hop on the VLAN/audio-DSP frontier):
 *   - 8021q: VLAN kernel module + NIC offload (ethtool -k vlan)
 *   - bridge VLAN: br_vlan filtering
 *   - PipeWire: pw-cli, the modern audio graph + filters
 *   - SOF DSP: audio DSP offload, equalizer/beamforming
 */
#include "wubu_vlanaudio.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_vlan = 0;
static int  g_vlan_offload = 0;
static int  g_pipewire = 0;
static int  g_alsa_dsp = 0;
static int  g_sof = 0;
static char g_audio_dsp_drv[32] = "";

/* ---- W1: probe the VLAN/audio-DSP topology ---- */
void wubu_vlanaudio_probe(void)
{
    g_vlan = 0; g_vlan_offload = 0; g_pipewire = 0; g_alsa_dsp = 0; g_sof = 0;
    g_audio_dsp_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* VLAN present (8021q interfaces or /proc/net/vlan)? */
    if (access("/proc/net/vlan", R_OK) == 0 ||
        access("/sys/class/net", R_OK) == 0) {
        /* check for vlan0 interfaces */
        struct dirent **e;
        int n = scandir("/sys/class/net", &e, NULL, alphasort);
        for (int i = 0; i < n; i++) {
            if (strstr(e[i]->d_name, "vlan") && strcmp(e[i]->d_name, "vlan")) {
                g_vlan = 1; break;
            }
        }
    }
    /* VLAN offload (ethtool is user-facing; sysfs vlan_feature). */
    if (access("/sys/class/net/eth0/features", R_OK) == 0) {
        g_vlan_offload = 1;
    }
    /* PipeWire audio graph present? */
    if (access("/usr/bin/pipewire", R_OK) == 0 ||
        access("/usr/bin/pw-cli", R_OK) == 0) {
        g_pipewire = 1;
        strcpy(g_audio_dsp_drv, "pipewire");
    }
    /* ALSA present? */
    if (access("/proc/asound", R_OK) == 0 ||
        access("/dev/snd", R_OK) == 0) {
        g_alsa_dsp = 1;
        if (!g_audio_dsp_drv[0]) strcpy(g_audio_dsp_drv, "alsa");
    }
    /* SOF DSP (Sound Open Firmware)? */
    if (access("/lib/firmware/intel/sof", R_OK) == 0 ||
        access("/sys/bus/pci/drivers/snd_sof_pci", R_OK) == 0) {
        g_sof = 1;
        if (!g_audio_dsp_drv[0]) strcpy(g_audio_dsp_drv, "sof");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_vlanaudio_vlan(void)      { return g_vlan; }
int  wubu_vlanaudio_vlan_offload(void){ return g_vlan_offload; }
int  wubu_vlanaudio_pipewire(void)  { return g_pipewire; }
int  wubu_vlanaudio_alsa(void)      { return g_alsa_dsp; }
int  wubu_vlanaudio_sof(void)       { return g_sof; }
const char *wubu_vlanaudio_dsp_driver(void){ return g_audio_dsp_drv[0] ? g_audio_dsp_drv : NULL; }

/* ---- W3: driver routing ---- */
const char *wubu_vlanaudio_vlan_for(const char *nic)
{
    if (!nic) return NULL;
    if (strstr(nic, "ixgbe") || strstr(nic, "i40e") || strstr(nic, "igc") || strstr(nic, "ice"))
        return "8021q";
    if (strstr(nic, "mlx5"))  return "8021q";
    if (strstr(nic, "bnxt"))  return "8021q";
    return "8021q";
}

const char *wubu_vlanaudio_dsp_for(const char *dsp)
{
    if (!dsp) return NULL;
    if (strstr(dsp, "pipewire") || strstr(dsp, "pw")) return "pipewire";
    if (strstr(dsp, "sof"))     return "snd_sof";
    if (strstr(dsp, "alsa"))    return "alsa-dmix";
    if (strstr(dsp, "pulse"))   return "pulseaudio";
    return "alsa";
}

/* ---- W4: summary ---- */
int wubu_vlanaudio_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "vlanaudio[vlan=%d offload=%d pipewire=%d alsa=%d sof=%d dsp=%s]",
        g_vlan, g_vlan_offload, g_pipewire, g_alsa_dsp, g_sof,
        wubu_vlanaudio_dsp_driver() ? wubu_vlanaudio_dsp_driver() : "none");
}
