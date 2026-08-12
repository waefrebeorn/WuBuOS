/*
 * wubu_bthfp.c -- kernel-owned Bluetooth HSP/HFP routing.
 *
 * HSP/HFP profiles carry voice audio (microphone/speaker) with call
 * control (AT commands). "Runs on everything" includes correct
 * profile routing on all Bluetooth stacks (BlueZ/Intel).
 *
 * Impl routing:
 *   - /sys/class/bluetooth/hci0: BT adapter presence
 *   - /sys/module/btintel/parameters: Intel BT module params
 */
#include "wubu_bthfp.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_bthfp_present = 0;
static int g_bthfp_a2dp = 0;

void wubu_bthfp_probe(void)
{
    /* Detect BT HFP adapter presence. */
#ifdef _GNU_SOURCE
    g_bthfp_present = (access("/sys/class/bluetooth/hci0", R_OK) == 0) ? 1 : 0;
    g_bthfp_a2dp = (access("/sys/module/btintel/parameters", R_OK) == 0) ? 1 : 0;
#else
    g_bthfp_present = g_bthfp_a2dp = 0;
#endif
}

int wubu_bthfp_present(void)
{
#ifdef _GNU_SOURCE
    return g_bthfp_present;
#else
    return 0;
#endif
}

int wubu_bthfp_call_state(int sco_active, int at_cmd_ready)
{
    if (sco_active && at_cmd_ready) return 1; /* active call */
    if (at_cmd_ready) return 2;                /* ringing */
    return 0;                                   /* idle */
}

const char *wubu_bthfp_state_str(int state)
{
    switch (state) {
        case 0: return "idle";
        case 1: return "active";
        case 2: return "ringing";
        default: return "unknown";
    }
}

void wubu_bthfp_summary(char *out, size_t cap)
{
    snprintf(out, cap, "bthfp[dev=%d a2dp=%d]",
             g_bthfp_present, g_bthfp_a2dp);
}
