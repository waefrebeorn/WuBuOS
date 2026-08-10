/*
 * wubu_drv_net.c -- the NETWORK drivers (the Deck's RZ616 Wi-Fi 6E +
 * every laptop's NIC).
 *
 * Two drivers:
 *   wubu_drv_wifi — the Wi-Fi class (the MediaTek RZ616 / Intel AX
 *                   series): the PCIe device + the register window +
 *                   the RF state (on/off + the link)
 *   wubu_drv_net  — the Ethernet class (the ax88179 USB / the r8169
 *                   PCIe NICs): the MAC + the link state
 *
 * The register model is the class contract: every NIC exposes the
 * link state + the MAC through its register window. The tests inject
 * a fake window.
 *
 * C11.
 */
#include "wubu_drv.h"
#include "wubu_drv_net.h"

#include <stdio.h>
#include <string.h>

/* ---- the shared NIC state ---- */

typedef struct {
    volatile uint8_t *mmio;
    int               link_up;
    uint8_t           mac[6];
    int               present;
} wubu_net_dev_t;

static wubu_net_dev_t g_wifi;
static wubu_net_dev_t g_eth;

/* the register offsets (the class contract) */
#define NET_REG_LINK  0x00   /* bit0 = link up */
#define NET_REG_MAC0  0x04   /* the MAC (6 bytes) */

static int net_probe_common(wubu_net_dev_t *n, wubu_drv_dev_t *dev)
{
    (void)dev;
    if (!n->mmio) return -1;
    n->link_up = (n->mmio[NET_REG_LINK] & 1) != 0;
    for (int i = 0; i < 6; i++) n->mac[i] = n->mmio[NET_REG_MAC0 + i];
    n->present = 1;
    return 0;
}

/* the Wi-Fi driver */
static int wifi_probe(wubu_drv_dev_t *dev)
{
    return net_probe_common(&g_wifi, dev);
}

const wubu_drv_id_t wubu_wifi_ids[] = {
    { 0x14C3, 0x7922, 0, 0 },   /* MediaTek MT7921K = the Deck's RZ616 */
    { 0x14C3, 0x7961, 0, 0 },   /* MT7921 */
    { 0x8086, 0x2723, 0, 0 },   /* Intel AX211 */
    { 0x8086, 0x51F0, 0, 0 },   /* Intel AX201 */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_wifi = {
    "wifi", wubu_wifi_ids, 4, wifi_probe,
};

/* the Ethernet driver */
static int eth_probe(wubu_drv_dev_t *dev)
{
    return net_probe_common(&g_eth, dev);
}

const wubu_drv_id_t wubu_net_ids[] = {
    { 0x10EC, 0x8168, 0, 0 },   /* Realtek r8168/r8169 */
    { 0x8086, 0x15F3, 0, 0 },   /* Intel I219-V */
    { WUBU_DRV_ANY, WUBU_DRV_ANY, 0x02, 0x00 },  /* the ethernet class */
    { 0, 0, 0, 0 },
};

const wubu_drv_t wubu_drv_net = {
    "net", wubu_net_ids, 3, eth_probe,
};

/* the test hook: inject the register windows */
void wubu_net_set_wifi_mmio(volatile void *mmio)
{
    g_wifi.mmio = (volatile uint8_t *)mmio;
}
void wubu_net_set_eth_mmio(volatile void *mmio)
{
    g_eth.mmio = (volatile uint8_t *)mmio;
}

/* the state — the link is read LIVE from the register window (the
 * world bridge samples the world's motion, so a dropped link must be
 * visible on the next sample) */
int wubu_net_wifi_link(void)
{
    if (!g_wifi.mmio) return 0;
    g_wifi.link_up = (g_wifi.mmio[NET_REG_LINK] & 1) != 0;
    return g_wifi.link_up;
}
int wubu_net_eth_link(void)
{
    if (!g_eth.mmio) return 0;
    g_eth.link_up = (g_eth.mmio[NET_REG_LINK] & 1) != 0;
    return g_eth.link_up;
}
const uint8_t *wubu_net_wifi_mac(void) { return g_wifi.mac; }
const uint8_t *wubu_net_eth_mac(void)  { return g_eth.mac; }
int wubu_net_wifi_present(void) { return g_wifi.present; }
int wubu_net_eth_present(void)  { return g_eth.present; }

const char *wubu_net_mac_str(const uint8_t *mac, char *out, size_t cap)
{
    if (!mac || !out || cap < 18) return NULL;
    snprintf(out, cap, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return out;
}
