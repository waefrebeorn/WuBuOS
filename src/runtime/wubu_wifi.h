/*
 * wubu_wifi.h -- Wi-Fi association through the supplicant's control protocol.
 *
 * Association is a workflow, not a driver detail: "join a network" means scan,
 * pick, authenticate and know when it worked. This API is the whole of that, and
 * a transport seam makes it gate-able without hardware.
 */

#ifndef WUBU_WIFI_H
#define WUBU_WIFI_H

#include <stddef.h>

typedef enum {
    WUBU_WIFI_DOWN = 0,
    WUBU_WIFI_DISCONNECTED,
    WUBU_WIFI_SCANNING,
    WUBU_WIFI_ASSOCIATING,
    WUBU_WIFI_AUTHENTICATING,
    WUBU_WIFI_CONNECTED
} WubuWifiState;

typedef struct {
    char ssid[33];
    int  freq;        /* MHz */
    int  signal;      /* dBm */
    int  secure;      /* 1 = WPA/RSN/WEP seen in the scan flags */
    int  known;       /* 1 = a saved network (reserved for LIST_NETWORKS) */
} WubuWifiNet;

/* Connect to /run/wpa_supplicant/<ifname>. Returns 0 on success, -1 when there is
 * no such socket (no supplicant, or a machine with no wireless interface). */
int  wubu_wifi_open(const char *ifname);
void wubu_wifi_close(void);
const char *wubu_wifi_iface(void);

/* Scan and fill up to max networks. Returns the count, or -2 when the supplicant
 * refused/did not answer. */
int wubu_wifi_scan(WubuWifiNet *out, int max);

/* Join: ADD_NETWORK, SET_NETWORK ssid/psk, ENABLE_NETWORK, SAVE_CONFIG.
 * psk may be NULL for an open network. Returns the network id (>= 0) or a
 * negative step number: -1 invalid input, -2 ADD_NETWORK failed, -3 SET_NETWORK
 * failed, -4 ENABLE_NETWORK failed. */
int  wubu_wifi_join(const char *ssid, const char *psk);

/* STATE from the supplicant; `detail` (optional) gets the first STATUS line. */
WubuWifiState wubu_wifi_status(char *detail, size_t n);
const char *wubu_wifi_state_name(WubuWifiState s);

/* Pure parser for SCAN_RESULTS text. Exposed so the protocol shape is tested
 * without a socket; the SSID is everything after the 4th '/' because it may
 * itself contain spaces and slashes. */
int wubu_wifi_scan_parse(const char *raw, WubuWifiNet *out, int max);

/* Test seam: every command goes through this instead of the socket. */
typedef int (*wubu_wifi_xfer_fn)(void *user, const char *cmd, char *reply, size_t n);
int wubu_wifi_set_transport(wubu_wifi_xfer_fn fn, void *user);

#endif /* WUBU_WIFI_H */
