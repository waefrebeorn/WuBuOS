/*
 * wubu_wifiutil.c -- kernel-owned WiFi channel utilization routing.
 *
 * Channel utilization measures how busy a WiFi channel is (CCA, airtime).
 * "Runs on everything" includes correct channel selection.
 *
 * WiFi channel utilization:
 *   - CCA (clear channel assessment): busy/total
 *   - airtime: per-station airtime usage
 *   - /sys/kernel/debug ieee80211 stats: utilization
 *   - channel: 2.4/5/6GHz, width, busy%
 *   - survey: NL80211_SURVEY_INFO_CHANNEL_TIME_BUSY
 *   - mac80211: station airtime accounting
 *
 * WuBuOS owns this: detect channel utilization + CCA + airtime, route
 * to the right driver, expose the topology.
 *
 * Research (7-hop on the wifiutil frontier):
 *   - CCA / channel busy time
 *   - NL80211 survey info
 *   - mac80211 station airtime
 */
#include "wubu_wifiutil.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int  g_util = 0;        /* utilization present */
static int  g_cca = 0;         /* CCA busy */
static int  g_airtime = 0;     /* airtime */
static int  g_survey = 0;      /* survey */
static int  g_chan = 0;        /* channel */
static char g_wifiutil_drv[24] = "";

void wubu_wifiutil_probe(void)
{
    g_util = 0; g_cca = 0; g_airtime = 0; g_survey = 0; g_chan = 0;
    g_wifiutil_drv[0] = '\0';

#ifdef WUBU_HOSTED
    if (access("/sys/class/ieee80211", R_OK) == 0 ||
        access("/sys/module/mac80211", R_OK) == 0) {
        g_util = 1; g_cca = 1; g_airtime = 1; g_survey = 1; g_chan = 1;
        strcpy(g_wifiutil_drv, "mac80211-util");
    }
    if (access("/sys/module/iwlwifi", R_OK) == 0) {
        g_util = 1; g_cca = 1; g_chan = 1;
        if (!g_wifiutil_drv[0]) strcpy(g_wifiutil_drv, "iwlwifi-util");
    }
    if (access("/sys/module/ath11k", R_OK) == 0) {
        if (!g_wifiutil_drv[0]) strcpy(g_wifiutil_drv, "ath11k-util");
    }
#endif
}

int  wubu_wifiutil_present(void){ return g_util; }
int  wubu_wifiutil_cca(void)    { return g_cca; }
int  wubu_wifiutil_airtime(void){ return g_airtime; }
int  wubu_wifiutil_survey(void) { return g_survey; }
int  wubu_wifiutil_chan(void)   { return g_chan; }
const char *wubu_wifiutil_driver(void){ return g_wifiutil_drv[0] ? g_wifiutil_drv : NULL; }

const char *wubu_wifiutil_band_for(const char *b)
{
    if (!b) return NULL;
    if (strstr(b, "24") || strstr(b, "2g")) return "2.4ghz";
    if (strstr(b, "5"))   return "5ghz";
    if (strstr(b, "6"))   return "6ghz";
    return "2.4ghz";
}

const char *wubu_wifiutil_state_for(const char *s)
{
    if (!s) return NULL;
    if (strstr(s, "busy"))  return "busy";
    if (strstr(s, "rx"))    return "rx";
    if (strstr(s, "tx"))    return "tx";
    if (strstr(s, "idle"))  return "idle";
    return "unknown";
}

int wubu_wifiutil_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "wifiutil[util=%d cca=%d airtime=%d survey=%d chan=%d drv=%s]",
        g_util, g_cca, g_airtime, g_survey, g_chan,
        wubu_wifiutil_driver() ? wubu_wifiutil_driver() : "none");
}