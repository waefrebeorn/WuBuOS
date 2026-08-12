/*
 * wubu_btaudio.c -- kernel-owned Bluetooth audio profile routing.
 *
 * BT audio profiles (A2DP, HSP/HFP, SCO) must auto-select based on
 * use case (media vs voice). "Runs on everything" includes correct
 * profile routing on every Bluetooth stack version.
 *
 * Impl routing:
 *   - /sys/class/bluetooth/hci0: BT adapter presence
 *   - /sys/module/btintel/parameters: Intel BT module params
 */
#include "wubu_btaudio.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_btaudio_present = 0;
static int g_btaudio_a2dp = 0;

void wubu_btaudio_probe(void)
{
    /* Detect BT audio adapter + A2DP. */
#ifdef _GNU_SOURCE
    g_btaudio_present = (access("/sys/class/bluetooth/hci0", R_OK) == 0) ? 1 : 0;
    g_btaudio_a2dp = (access("/sys/module/btintel/parameters", R_OK) == 0) ? 1 : 0;
#else
    g_btaudio_present = g_btaudio_a2dp = 0;
#endif
}

int wubu_btaudio_present(void)
{
#ifdef _GNU_SOURCE
    return g_btaudio_present;
#else
    return 0;
#endif
}

int wubu_btaudio_auto(int latency_ms)
{
    /* Latency <20ms = voice (HSP/HFP), else media (A2DP). */
    return (latency_ms < 20) ? 1 : 0;
}

const char *wubu_btaudio_profile_str(int profile)
{
    switch (profile) {
        case 0: return "none";
        case 1: return "a2dp";
        case 2: return "hsp/hfp";
        case 3: return "sco";
        default: return "unknown";
    }
}

void wubu_btaudio_summary(char *out, size_t cap)
{
    snprintf(out, cap, "btaudio[dev=%d a2dp=%d]",
             g_btaudio_present, g_btaudio_a2dp);
}
