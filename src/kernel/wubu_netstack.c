/*
 * wubu_netstack.c -- minimal Ethernet/ARP/IPv4/ICMP/UDP stack (L5).
 *
 * ONE concern: turn the virtio-net frame pipe into a socket-lite API
 * (wubu_net_udp_send / wubu_net_poll / wubu_net_ping) that a later
 * 9P-over-UDP leg can bind to. Static addressing = QEMU user-netdev
 * defaults (10.0.2.15/24, gw 10.0.2.2). Everything is computed in
 * software: the driver negotiates no offloads.
 *
 * Frame buffers live on the STACK here -- the virtio-net driver bounces
 * every TX frame through DMA-able heap memory and copies RX out, so no
 * higher-half address ever reaches the device.
 *
 * C11 freestanding (metal: klog); hosted tests drive the same code
 * against a fake NIC (crafted frames in, captured frames out).
 */
#include "wubu_netstack.h"

#ifdef MYSEED_METAL
#include "klog.h"
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int   memcmp(const void *a, const void *b, size_t n);
#define NS_LOG   klog_printf
#else
#include <stdio.h>
#include <string.h>
#define NS_LOG   printf
#endif

#define NS_ETH_HDR   14u
#define NS_MAX_FRAME WUBU_VNET_MAX_FRAME
#define NS_MAX_UDP   (NS_MAX_FRAME - NS_ETH_HDR - 20u - 8u)   /* 1472 */

#define NS_ETH_IPV4  0x0800u
#define NS_ETH_ARP   0x0806u

/* -- wire helpers (big-endian) --------------------------------------- */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/* -- internet checksum (RFC 1071) ------------------------------------ */
uint16_t wubu_net_checksum(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += (uint32_t)((p[0] << 8) | p[1]);
        p += 2;
        len -= 2;
    }
    if (len) sum += (uint32_t)(p[0] << 8);   /* odd byte pads low */
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)(~sum & 0xFFFFu);
}

/* -- stack state ------------------------------------------------------ */
static wubu_virtio_net_t *g_nic;
static uint8_t  g_mac[6];
static uint16_t g_ip_id;
static uint16_t g_ping_seq;

#define NS_ARP_CACHE 8
static struct { uint32_t ip; uint8_t mac[6]; int valid; } g_arp[NS_ARP_CACHE];
static int g_arp_next;

static struct {
    int waiting;
    int got;
    uint32_t src;
    uint16_t id, seq;
} g_ping;

int wubu_netstack_init(wubu_virtio_net_t *nic)
{
    if (!nic) return -1;
    if (wubu_virtio_net_get_mac(nic, g_mac) != 0) return -1;
    g_nic = nic;
    memset(g_arp, 0, sizeof(g_arp));
    memset(&g_ping, 0, sizeof(g_ping));
    g_ip_id = 1;
    g_ping_seq = 0;
    return 0;
}

static int ns_send(const uint8_t *frame, uint32_t len)
{
    if (!g_nic) return -1;
    return wubu_virtio_net_send(g_nic, frame, len);
}

/* -- Ethernet --------------------------------------------------------- */
static void ns_eth(uint8_t *f, const uint8_t dst[6], uint16_t type)
{
    memcpy(f, dst, 6);
    memcpy(f + 6, g_mac, 6);
    wr16(f + 12, type);
}

/* -- ARP ---------------------------------------------------------------- */
static int arp_cache_get(uint32_t ip, uint8_t mac_out[6])
{
    for (int i = 0; i < NS_ARP_CACHE; i++) {
        if (g_arp[i].valid && g_arp[i].ip == ip) {
            memcpy(mac_out, g_arp[i].mac, 6);
            return 1;
        }
    }
    return 0;
}

static void arp_cache_put(uint32_t ip, const uint8_t mac[6])
{
    for (int i = 0; i < NS_ARP_CACHE; i++) {
        if (g_arp[i].valid && g_arp[i].ip == ip) {
            memcpy(g_arp[i].mac, mac, 6);
            return;
        }
    }
    int slot = g_arp_next;
    g_arp_next = (g_arp_next + 1) % NS_ARP_CACHE;
    g_arp[slot].ip = ip;
    memcpy(g_arp[slot].mac, mac, 6);
    g_arp[slot].valid = 1;
}

/* ARP packet body (28 bytes) at f+14 */
static void ns_arp_body(uint8_t *f, uint16_t oper,
                        const uint8_t sha[6], uint32_t spa,
                        const uint8_t tha[6], uint32_t tpa)
{
    uint8_t *a = f + NS_ETH_HDR;
    wr16(a, 1);            /* hardware: Ethernet */
    wr16(a + 2, NS_ETH_IPV4);
    a[4] = 6; a[5] = 4;
    wr16(a + 6, oper);
    memcpy(a + 8, sha, 6);
    wr32(a + 14, spa);
    memcpy(a + 18, tha, 6);
    wr32(a + 24, tpa);
}

static void ns_arp_request(uint32_t ip)
{
    static const uint8_t bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    static const uint8_t zero[6]  = { 0, 0, 0, 0, 0, 0 };
    uint8_t f[NS_ETH_HDR + 28];
    ns_eth(f, bcast, NS_ETH_ARP);
    ns_arp_body(f, 1, g_mac, WUBU_NET_OUR_IP, zero, ip);
    ns_send(f, sizeof(f));
}

static void ns_arp_handle(const uint8_t *f, uint32_t len)
{
    if (len < NS_ETH_HDR + 28) return;
    const uint8_t *a = f + NS_ETH_HDR;
    if (rd16(a) != 1 || rd16(a + 2) != NS_ETH_IPV4 ||
        a[4] != 6 || a[5] != 4) return;
    uint16_t oper = rd16(a + 6);
    const uint8_t *sha = a + 8;
    uint32_t spa = rd32(a + 14);
    uint32_t tpa = rd32(a + 24);

    if (oper == 2) {                       /* reply: learn the mapping */
        arp_cache_put(spa, sha);
        return;
    }
    if (oper == 1 && tpa == WUBU_NET_OUR_IP) {   /* request for us */
        uint8_t r[NS_ETH_HDR + 28];
        ns_eth(r, sha, NS_ETH_ARP);
        ns_arp_body(r, 2, g_mac, WUBU_NET_OUR_IP, sha, spa);
        ns_send(r, sizeof(r));
    }
}

int wubu_net_arp_resolve(uint32_t ip, uint8_t mac_out[6])
{
    if (!g_nic || !mac_out) return -1;
    if (arp_cache_get(ip, mac_out)) return 0;
    for (int tries = 0; tries < 3; tries++) {
        ns_arp_request(ip);
        for (int i = 0; i < 200; i++) {
            wubu_net_poll(NULL, NULL);
            if (arp_cache_get(ip, mac_out)) return 0;
        }
    }
    return -1;
}

/* -- IPv4 --------------------------------------------------------------- */
/* Fill the 20-byte header at f+14 for `proto` + payload length; the
 * Ethernet header must already be in place. Returns the payload ptr. */
static uint8_t *ns_ipv4(uint8_t *f, uint8_t proto, uint32_t dst,
                        uint32_t payload_len)
{
    uint8_t *ip = f + NS_ETH_HDR;
    ip[0] = 0x45;                          /* v4, 20-byte header */
    ip[1] = 0;
    wr16(ip + 2, (uint16_t)(20 + payload_len));
    wr16(ip + 4, g_ip_id++);
    wr16(ip + 6, 0);                       /* no flags, no fragment */
    ip[8] = 64;                            /* TTL */
    ip[9] = proto;
    wr16(ip + 10, 0);
    wr32(ip + 12, WUBU_NET_OUR_IP);
    wr32(ip + 16, dst);
    wr16(ip + 10, wubu_net_checksum(ip, 20));
    return ip + 20;
}

/* -- ICMP ---------------------------------------------------------------- */
static void ns_icmp_handle(uint32_t src, const uint8_t *pl, uint32_t pllen,
                           const uint8_t *orig)
{
    if (pllen < 8) return;
    uint8_t type = pl[0];
    if (type == 8) {                       /* echo request -> reply */
        if (wubu_net_checksum(pl, pllen) != 0) return;
        uint32_t ihl = (uint32_t)(orig[NS_ETH_HDR] & 0xF) * 4u;
        if (pllen > NS_MAX_FRAME - NS_ETH_HDR - ihl) return;
        uint8_t r[NS_MAX_FRAME];
        ns_eth(r, orig + 6, NS_ETH_IPV4);  /* back to the sender's MAC */
        uint8_t *rip = r + NS_ETH_HDR;
        memcpy(rip, orig + NS_ETH_HDR, ihl);
        wr32(rip + 12, rd32(orig + NS_ETH_HDR + 16));   /* swap src/dst */
        wr32(rip + 16, src);
        rip[8] = 64;
        wr16(rip + 10, 0);
        wr16(rip + 10, wubu_net_checksum(rip, ihl));
        uint8_t *ricmp = rip + ihl;
        memcpy(ricmp, pl, pllen);
        ricmp[0] = 0;                      /* echo reply */
        wr16(ricmp + 2, 0);
        wr16(ricmp + 2, wubu_net_checksum(ricmp, pllen));
        ns_send(r, NS_ETH_HDR + ihl + pllen);
        return;
    }
    if (type == 0 && g_ping.waiting && src == g_ping.src &&
        rd16(pl + 4) == g_ping.id && rd16(pl + 6) == g_ping.seq) {
        g_ping.got = 1;                    /* our echo came home */
    }
}

int wubu_net_ping(uint32_t ip)
{
    uint8_t mac[6];
    if (wubu_net_arp_resolve(ip, mac) != 0) return -1;

    static const uint8_t payload[9] = { 'W','U','B','U','-','P','I','N','G' };
    uint8_t f[NS_ETH_HDR + 20 + 8 + sizeof(payload)];
    ns_eth(f, mac, NS_ETH_IPV4);
    uint8_t *icmp = ns_ipv4(f, 1, ip, 8 + sizeof(payload));
    icmp[0] = 8; icmp[1] = 0;
    wr16(icmp + 2, 0);
    wr16(icmp + 4, 0x5755);                /* id 'WU' */
    wr16(icmp + 6, ++g_ping_seq);
    memcpy(icmp + 8, payload, sizeof(payload));
    wr16(icmp + 2, wubu_net_checksum(icmp, 8 + sizeof(payload)));

    g_ping.waiting = 1; g_ping.got = 0;
    g_ping.src = ip; g_ping.id = 0x5755; g_ping.seq = g_ping_seq;
    if (ns_send(f, sizeof(f)) != 0) { g_ping.waiting = 0; return -1; }
    for (int i = 0; i < 600 && !g_ping.got; i++)
        wubu_net_poll(NULL, NULL);
    int ok = g_ping.got;
    g_ping.waiting = 0;
    return ok ? 0 : -1;
}

/* -- UDP ------------------------------------------------------------------ */
/* UDP checksum over the pseudo-header + segment (RFC 768). */
static uint16_t ns_udp_csum(uint32_t src, uint32_t dst,
                            const uint8_t *udp, uint32_t ulen)
{
    uint8_t pseudo[12];
    wr32(pseudo, src);
    wr32(pseudo + 4, dst);
    pseudo[8] = 0; pseudo[9] = 17;
    wr16(pseudo + 10, (uint16_t)ulen);
    uint32_t sum = 0;
    const uint8_t *p = pseudo;
    for (int i = 0; i < 6; i++) { sum += (uint32_t)((p[0] << 8) | p[1]); p += 2; }
    p = udp;
    uint32_t n = ulen;
    while (n > 1) { sum += (uint32_t)((p[0] << 8) | p[1]); p += 2; n -= 2; }
    if (n) sum += (uint32_t)(p[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    /* raw one's-complement result: 0 when VERIFYING a valid segment.
     * The "0 means no checksum -> send 0xFFFF" rule is applied by the
     * SENDER only, never here (it would break verification). */
    return (uint16_t)(~sum & 0xFFFFu);
}

int wubu_net_udp_send(uint32_t ip, uint16_t port, const void *buf, uint32_t len)
{
    if (!g_nic || !buf || len > NS_MAX_UDP) return -1;
    uint32_t nexthop =
        ((ip & WUBU_NET_NETMASK) == (WUBU_NET_OUR_IP & WUBU_NET_NETMASK))
        ? ip : WUBU_NET_GATEWAY;
    uint8_t mac[6];
    if (wubu_net_arp_resolve(nexthop, mac) != 0) return -1;

    uint8_t f[NS_MAX_FRAME];
    ns_eth(f, mac, NS_ETH_IPV4);
    uint8_t *udp = ns_ipv4(f, 17, ip, 8 + len);
    wr16(udp, WUBU_NET_SRC_PORT);
    wr16(udp + 2, port);
    wr16(udp + 4, (uint16_t)(8 + len));
    wr16(udp + 6, 0);
    memcpy(udp + 8, buf, len);
    uint16_t uc = ns_udp_csum(WUBU_NET_OUR_IP, ip, udp, 8 + len);
    wr16(udp + 6, uc ? uc : 0xFFFFu);      /* 0 on wire = "no checksum" */
    return ns_send(f, NS_ETH_HDR + 20 + 8 + len);
}

static void ns_udp_handle(uint32_t src, const uint8_t *pl, uint32_t pllen,
                          wubu_net_rx_cb cb, void *ctx)
{
    if (pllen < 8 || !cb) return;
    uint16_t sport = rd16(pl);
    uint16_t dport = rd16(pl + 2);
    uint16_t ulen  = rd16(pl + 4);
    uint16_t csum  = rd16(pl + 6);
    if (ulen < 8 || ulen > pllen) return;
    if (csum != 0 &&
        ns_udp_csum(src, WUBU_NET_OUR_IP, pl, ulen) != 0)
        return;                            /* corrupted datagram */
    cb(src, sport, dport, pl + 8, (uint32_t)(ulen - 8), ctx);
}

/* -- RX dispatch ----------------------------------------------------------- */
static void ns_handle(const uint8_t *f, uint32_t len,
                      wubu_net_rx_cb cb, void *ctx)
{
    if (len < NS_ETH_HDR) return;
    uint16_t type = rd16(f + 12);
    if (type == NS_ETH_ARP) { ns_arp_handle(f, len); return; }
    if (type != NS_ETH_IPV4 || len < NS_ETH_HDR + 20) return;

    const uint8_t *ip = f + NS_ETH_HDR;
    uint32_t ihl = (uint32_t)(ip[0] & 0xF) * 4u;
    if ((ip[0] >> 4) != 4 || ihl < 20 || len < NS_ETH_HDR + ihl) return;
    uint16_t tot = rd16(ip + 2);
    if (tot < ihl || (uint32_t)tot > len - NS_ETH_HDR) return;
    if (wubu_net_checksum(ip, ihl) != 0) return;
    if (rd32(ip + 16) != WUBU_NET_OUR_IP) return;   /* not for us */

    uint32_t src = rd32(ip + 12);
    const uint8_t *pl = ip + ihl;
    uint32_t pllen = (uint32_t)(tot - ihl);
    if (ip[9] == 1)       ns_icmp_handle(src, pl, pllen, f);
    else if (ip[9] == 17) ns_udp_handle(src, pl, pllen, cb, ctx);
}

int wubu_net_poll(wubu_net_rx_cb cb, void *ctx)
{
    if (!g_nic) return 0;
    uint8_t f[NS_MAX_FRAME];
    uint32_t len = 0;
    if (wubu_virtio_net_recv(g_nic, f, sizeof(f), &len) != 0) return 0;
    ns_handle(f, len, cb, ctx);
    return 1;
}

/* -- boot-time end-to-end selftest ----------------------------------------- */
#define NS_MARKER_LEN 64u
static uint8_t ns_marker[NS_MARKER_LEN];

typedef struct {
    uint8_t buf[256];
    uint32_t len;
    uint32_t src;
    uint16_t sport;
    int got;
} ns_echo_cap_t;

static void ns_echo_cb(uint32_t src_ip, uint16_t src_port, uint16_t dst_port,
                       const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)dst_port;
    ns_echo_cap_t *c = (ns_echo_cap_t *)ctx;
    if (!c->got && len <= sizeof(c->buf)) {
        memcpy(c->buf, payload, len);
        c->len = len;
        c->src = src_ip;
        c->sport = src_port;
        c->got = 1;
    }
}

int wubu_net_selftest(void)
{
    if (!g_nic) return -1;

    uint8_t gwmac[6];
    if (wubu_net_arp_resolve(WUBU_NET_GATEWAY, gwmac) != 0) {
        NS_LOG("NET-FAIL: arp for 10.0.2.2 timed out\n");
        return -1;
    }
    NS_LOG("NET-ARP: 10.0.2.2 at %x:%x:%x:%x:%x:%x\n",
           (unsigned)gwmac[0], (unsigned)gwmac[1], (unsigned)gwmac[2],
           (unsigned)gwmac[3], (unsigned)gwmac[4], (unsigned)gwmac[5]);

    /* ICMP is informational: slirp answers pings to the gateway on
     * most builds; UDP is the hard gate either way. */
    if (wubu_net_ping(WUBU_NET_GATEWAY) == 0)
        NS_LOG("NET-PING: 10.0.2.2 answered echo request\n");
    else
        NS_LOG("NET-PING: no echo reply from 10.0.2.2 (informational)\n");

    for (uint32_t i = 0; i < NS_MARKER_LEN; i++)
        ns_marker[i] = (uint8_t)(i * 13u + 7u);

    for (int tries = 0; tries < 3; tries++) {
        ns_echo_cap_t cap;
        memset(&cap, 0, sizeof(cap));
        if (wubu_net_udp_send(WUBU_NET_GATEWAY, WUBU_NET_GATE_PORT,
                              ns_marker, NS_MARKER_LEN) != 0)
            continue;
        for (int i = 0; i < 600 && !cap.got; i++)
            wubu_net_poll(ns_echo_cb, &cap);
        if (cap.got && cap.len == NS_MARKER_LEN &&
            memcmp(cap.buf, ns_marker, NS_MARKER_LEN) == 0) {
            NS_LOG("NET-OK: udp round-trip %u bytes\n", (unsigned)cap.len);
            return 0;
        }
    }
    NS_LOG("NET-FAIL: udp round-trip\n");
    return -1;
}