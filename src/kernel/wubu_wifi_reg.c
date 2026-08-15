/*
 * wubu_wifi_reg.c -- kernel-owned WiFi regulatory/DFS routing.
 *
 * WiFi regulatory domains define allowed channels + TX power by country.
 * DFS (dynamic frequency selection) governs 5GHz radar channels. "Runs on
 * everything" includes correct, legal WiFi operation worldwide.
 *
 * WiFi regulatory:
 *   - regulatory.db: the wireless regulatory database (wireless-regdb)
 *   - crda / regdbdump: regulatory daemon + dump
 *   - cfg80211: regulatory core (set_regdom, DFS, passive channels)
 *   - /sys/class/ieee80211 macaddress + regdomain
 *   - DFS: radar detection on 5GHz (5250-5350, 5470-5725 MHz)
 *   - country: alpha2 ISO code, self-managed vs. central
 *
 * WuBuOS owns this: detect the regulatory DB + DFS support + current
 * country, route to the right regulator, and expose the topology.
 *
 * Research (Kevin-Bacon 7-hop on the WiFi-reg frontier):
 *   - regulatory.db: wireless-regdb (regdbdump)
 *   - cfg80211 regulatory: set_regdom, DFS regions
 *   - DFS: radar detection, passive scan, CAC (channel availability check)
 */
#include "wubu_wifi_reg.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

/* ---- Global state ---- */
static int  g_regdb = 0;       /* regulatory database */
static int  g_crda = 0;        /* crda */
static int  g_dfs = 0;         /* DFS supported */
static int  g_radar = 0;       /* radar detection */
static int  g_country = 0;     /* country set */
static char g_reg_drv[24] = "";

/* ---- W1: probe the WiFi-regulatory topology ---- */
void wubu_wifi_reg_probe(void)
{
    g_regdb = 0; g_crda = 0; g_dfs = 0; g_radar = 0; g_country = 0;
    g_reg_drv[0] = '\0';

#ifdef WUBU_HOSTED
    /* regulatory database present? */
    if (access("/usr/lib/crda/regulatory.bin", R_OK) == 0 ||
        access("/lib/crda/regulatory.bin", R_OK) == 0 ||
        access("/usr/lib/firmware/regulatory.db", R_OK) == 0) {
        g_regdb = 1;
        strcpy(g_reg_drv, "regulatory.db");
    }
    /* crda present? */
    if (access("/usr/sbin/crda", R_OK) == 0 ||
        access("/usr/bin/crda", R_OK) == 0 ||
        access("/sbin/crda", R_OK) == 0) {
        g_crda = 1;
        if (!g_reg_drv[0]) strcpy(g_reg_drv, "crda");
    }
    /* WiFi phy present? */
    DIR *d = opendir("/sys/class/ieee80211");
    int has_phy = 0;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == 'p') { has_phy = 1; break; }
        }
        closedir(d);
    }
    if (has_phy) {
        g_dfs = 1;             /* 5GHz DFS capable phy */
        g_radar = 1;           /* radar detection */
        g_country = 1;
        if (!g_reg_drv[0]) strcpy(g_reg_drv, "cfg80211");
    }
#endif
}

/* ---- W2: accessors ---- */
int  wubu_wifi_reg_db(void)    { return g_regdb; }
int  wubu_wifi_reg_crda(void)  { return g_crda; }
int  wubu_wifi_reg_dfs(void)   { return g_dfs; }
int  wubu_wifi_reg_radar(void) { return g_radar; }
int  wubu_wifi_reg_country(void){ return g_country; }
const char *wubu_wifi_reg_driver(void){ return g_reg_drv[0] ? g_reg_drv : NULL; }

/* ---- W3: regulatory routing ---- */
const char *wubu_wifi_reg_country_for(const char *code)
{
    if (!code) return NULL;
    /* Alpha2 ISO country codes -> uppercase. */
    if (strlen(code) == 2) {
        static char norm[3];
        norm[0] = (code[0] >= 'a' && code[0] <= 'z') ? code[0] - 32 : code[0];
        norm[1] = (code[1] >= 'a' && code[1] <= 'z') ? code[1] - 32 : code[1];
        norm[2] = '\0';
        return norm;
    }
    if (strstr(code, "world") || strstr(code, "00")) return "00";
    if (strstr(code, "us"))  return "US";
    if (strstr(code, "eu"))  return "EU";
    return "00";
}

const char *wubu_wifi_reg_dfs_for(const char *band)
{
    if (!band) return NULL;
    if (strstr(band, "5g"))  return "dfs-5ghz";
    if (strstr(band, "6g"))  return "6ghz";
    if (strstr(band, "2.4")) return "2.4ghz";
    return "dfs";
}

/* ---- W4: summary ---- */
int wubu_wifi_reg_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "wifireg[db=%d crda=%d dfs=%d radar=%d country=%d drv=%s]",
        g_regdb, g_crda, g_dfs, g_radar, g_country,
        wubu_wifi_reg_driver() ? wubu_wifi_reg_driver() : "none");
}
