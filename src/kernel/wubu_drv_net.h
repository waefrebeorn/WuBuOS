/*
 * wubu_drv_net.h -- the network drivers (Wi-Fi + Ethernet).
 */
#ifndef WUBU_DRV_NET_H
#define WUBU_DRV_NET_H

#include <stddef.h>
#include <stdint.h>

/* the drivers (registered by the registry) */
extern const struct wubu_drv wubu_drv_wifi;
extern const struct wubu_drv wubu_drv_net;

/* the test hooks: inject the register windows */
void wubu_net_set_wifi_mmio(volatile void *mmio);
void wubu_net_set_eth_mmio(volatile void *mmio);

/* the state */
int wubu_net_wifi_link(void);
int wubu_net_eth_link(void);
const uint8_t *wubu_net_wifi_mac(void);
const uint8_t *wubu_net_eth_mac(void);
int wubu_net_wifi_present(void);
int wubu_net_eth_present(void);

/* format a MAC */
const char *wubu_net_mac_str(const uint8_t *mac, char *out, size_t cap);

#endif
