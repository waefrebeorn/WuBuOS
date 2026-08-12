/*
 * wubu_net.h -- kernel-owned network driver routing + power-save tuning.
 */
#ifndef WUBU_NET_H
#define WUBU_NET_H

#include <stddef.h>

/* W1: probe the network topology (Wi-Fi chip, ethernet, 2.5GbE). */
void wubu_net_probe(void);

/* W2: accessors */
int          wubu_net_has_wifi(void);
int          wubu_net_has_eth(void);
int          wubu_net_has_2g5(void);        /* 2.5GbE NIC present */
const char *wubu_net_wifi_driver(void);     /* "iwlwifi"|"rtl8821ce"|"mt7921e"|... */
const char *wubu_net_eth_driver(void);      /* "igc"|"r8169"|"r8168-dkms"|... */
const char *wubu_net_wifi_chip_name(void);  /* human-readable chip name */
int          wubu_net_wifi_vendor(void);    /* PCI vendor of Wi-Fi chip */

/* W3: the power-save-disabling kernel param (the fix for Wi-Fi latency). */
const char *wubu_net_power_save_disable(void);

/* W4: summary fragment. */
int wubu_net_summary(char *out, size_t cap);

#endif /* WUBU_NET_H */
