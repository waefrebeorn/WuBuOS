/*
 * wubu_dspmode.c -- kernel-owned audio codec DSP modes + suspend routing.
 *
 * Codec DSPs (SOF - Sound Open Firmware, and HD-Audio DSP) have power
 * modes: active, low-power, and voice-trigger wake during S0ix/suspend.
 * "Runs on everything" includes correct audio wake + low-power audio.
 *
 * DSP mode capabilities:
 *   - Active: full DSP processing (EQ, effects, beamforming)
 *   - Low power: reduced processing during idle
 *   - Voice trigger: DSP listens for "hey" during S0ix (wake word)
 *   - Suspend/resume: codec D3/D3cold, S3 suspend hooks
 *   - DSP trace/firmware: SOF firmware load, topology, debugging
 *
 * WuBuOS owns this: detect the audio DSP + its power/wake modes, route
 * to the right DSP driver, and expose the DSP topology.
 *
 * Research (Kevin-Bacon 7-hop on the audio-DSP-mode frontier):
 *   - SOF (Sound Open Firmware): snd_sof, DSP power states
 *   - HD-Audio DSP: snd_hda_intel + codec D3 (snd_pcm_suspend)
 *   - Voice trigger: wake-word detection in S0ix (SOF hotword)
 *   - DSP low-power: snd_sof PM, runtime PM for codec
 */
#include "wubu_dspmode.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- Global state ---- */
static int  g_sof = 0;          /* SOF DSP */
static int  g_dsp_pm = 0;       /* DSP runtime PM */
static int  g_voice_wake = 0;   /* voice trigger wake */
static int  g_suspend_ok = 0;   /* codec suspend hook */
static char g_dsp_drv[24] = "";
static char g_dsp_mode[16] = "active";

/* ---- W1: probe the audio DSP-mode topology ---- */
void wubu_dspmode_probe(void)
{
    g_sof = 0; g_dsp_pm = 0; g_voice_wake = 0; g_suspend_ok = 0;
    g_dsp_drv[0] = '\0'; strcpy(g_dsp_mode, "active");

#ifdef WUBU_HOSTED
    /* SOF DSP present? */
    if (access("/sys/bus/pci/drivers/snd_sof_pci", R_OK) == 0 ||
        access("/lib/firmware/intel/sof", R_OK) == 0) {
        g_sof = 1;
        strcpy(g_dsp_drv, "snd_sof");
    }
    /* HD-Audio DSP (Intel) present? */
    if (access("/sys/bus/pci/drivers/snd_hda_intel", R_OK) == 0) {
        if (!g_sof) {
            g_sof = 1;
            strcpy(g_dsp_drv, "snd_hda_intel");
        }
    }
    /* DSP runtime PM (autosuspend)? */
    if (access("/sys/module/snd_sof_pci/parameters/pm_runtime", R_OK) == 0 ||
        access("/sys/module/snd_hda_intel/parameters/power_save", R_OK) == 0) {
        g_dsp_pm = 1;
    }
    /* Voice trigger / wake word (SOF hotword). */
    if (access("/sys/bus/platform/drivers/snd_sof_pci", R_OK) == 0 ||
        access("/sys/module/snd_sof/parameters/hotword", R_OK) == 0) {
        g_voice_wake = 1;
    }
    /* Suspend hook present (D3 power state via /sys power). */
    if (access("/sys/power/suspend_stats", R_OK) == 0 ||
        access("/sys/power/state", R_OK) == 0) {
        g_suspend_ok = 1;
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_dspmode_sof(void)      { return g_sof; }
int  wubu_dspmode_pm(void)       { return g_dsp_pm; }
int  wubu_dspmode_voice_wake(void){ return g_voice_wake; }
int  wubu_dspmode_suspend_ok(void){ return g_suspend_ok; }
const char *wubu_dspmode_driver(void){ return g_dsp_drv[0] ? g_dsp_drv : NULL; }
const char *wubu_dspmode_current(void){ return g_dsp_mode; }

/* ---- W3: DSP mode routing ---- */
const char *wubu_dspmode_for(const char *mode)
{
    if (!mode) return NULL;
    if (strstr(mode, "voice") || strstr(mode, "wake")) return "voice-trigger";
    if (strstr(mode, "low") || strstr(mode, "power")) return "low-power";
    if (strstr(mode, "suspend") || strstr(mode, "d3")) return "suspend";
    return "active";
}

/* ---- W4: summary ---- */
int wubu_dspmode_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "dspmode[sof=%d(%s) pm=%d voice_wake=%d suspend=%d mode=%s]",
        g_sof, wubu_dspmode_driver() ? wubu_dspmode_driver() : "none",
        g_dsp_pm, g_voice_wake, g_suspend_ok,
        wubu_dspmode_current() ? wubu_dspmode_current() : "none");
}
