/* test_netstack.c -- L5 unit gate: the minimal Ethernet/ARP/IPv4/ICMP/
 * UDP stack against a FAKE NIC boundary (crafted frames in, captured
 * frames out) + known checksum vectors.
 *
 * The fake NIC auto-responds like slirp would (ARP reply for the
 * gateway, ICMP echo reply, UDP echo on the gate port) but builds every
 * response with TEST-LOCAL code -- an independent implementation, so a
 * stack that talks to itself still gets checked against an outside
 * oracle. Checksum anchors: the RFC 1071 / Wikipedia IPv4 example
 * (0xb861) plus UDP/ICMP vectors computed independently (python
 * struct.pack one's-complement sum, hardcoded below).
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "wubu_netstack.h"
#include "wubu_netstack.c"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

/* ---- test-local wire helpers + reference checksum ------------------ */
static void t_wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void t_wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}
static uint16_t t_rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t t_rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
/* independent RFC 1071 implementation (byte-stream accumulator) */
static uint16_t t_csum(const uint8_t *p, uint32_t n)
{
    uint32_t acc = 0;
    for (uint32_t i = 0; i < n; i++)
        acc += (i & 1) ? p[i] : ((uint32_t)p[i] << 8);
    acc = (acc & 0xFFFF) + (acc >> 16);
    acc = (acc & 0xFFFF) + (acc >> 16);
    return (uint16_t)(~acc);
}
static uint16_t t_udp_csum(uint32_t src, uint32_t dst,
                           const uint8_t *udp, uint32_t ulen)
{
    uint8_t ph[12];
    t_wr32(ph, src); t_wr32(ph + 4, dst);
    ph[8] = 0; ph[9] = 17; t_wr16(ph + 10, (uint16_t)ulen);
    uint32_t acc = 0;
    for (int i = 0; i < 12; i++) acc += (i & 1) ? ph[i] : ((uint32_t)ph[i] << 8);
    for (uint32_t i = 0; i < ulen; i++) acc += (i & 1) ? udp[i] : ((uint32_t)udp[i] << 8);
    acc = (acc & 0xFFFF) + (acc >> 16);
    acc = (acc & 0xFFFF) + (acc >> 16);
    return (uint16_t)(~acc);   /* raw: 0 verifies a valid segment */
}

/* ---- fake NIC boundary --------------------------------------------- */
static const uint8_t nic_mac[6] = { 0x52, 0x54, 0x00, 0xAA, 0xBB, 0xCC };
static const uint8_t gw_mac[6]  = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x02 };

static uint8_t last_tx[1514];
static uint32_t last_tx_len;
static int tx_count;
static uint32_t last_arp_tpa;
static int auto_arp = 1, auto_icmp = 1, auto_udp_echo = 1;

static struct { uint8_t f[1514]; uint32_t len; } rxq[64];
static int rxq_head, rxq_tail;

static void rx_push(const uint8_t *f, uint32_t len)
{
    if (rxq_tail - rxq_head >= 64) return;
    memcpy(rxq[rxq_tail % 64].f, f, len);
    rxq[rxq_tail % 64].len = len;
    rxq_tail++;
}

int wubu_virtio_net_get_mac(const wubu_virtio_net_t *n, uint8_t out[6])
{ (void)n; memcpy(out, nic_mac, 6); return 0; }

int wubu_virtio_net_recv(wubu_virtio_net_t *n, void *buf, uint32_t cap,
                         uint32_t *out_len)
{
    (void)n;
    if (rxq_head == rxq_tail) return -1;
    uint32_t len = rxq[rxq_head % 64].len;
    uint32_t copy = len < cap ? len : cap;
    memcpy(buf, rxq[rxq_head % 64].f, copy);
    *out_len = len;
    rxq_head++;
    return 0;
}

/* build an IPv4 frame with TEST-LOCAL code (the outside oracle) */
static uint32_t t_build_ip(uint8_t *f, const uint8_t dstmac[6],
                           uint32_t src, uint32_t dst, uint8_t proto,
                           const uint8_t *pl, uint32_t pllen, uint16_t id)
{
    memcpy(f, dstmac, 6);
    memcpy(f + 6, gw_mac, 6);
    t_wr16(f + 12, 0x0800);
    uint8_t *ip = f + 14;
    ip[0] = 0x45; ip[1] = 0;
    t_wr16(ip + 2, (uint16_t)(20 + pllen));
    t_wr16(ip + 4, id);
    t_wr16(ip + 6, 0);
    ip[8] = 64; ip[9] = proto;
    t_wr16(ip + 10, 0);
    t_wr32(ip + 12, src);
    t_wr32(ip + 16, dst);
    t_wr16(ip + 10, t_csum(ip, 20));
    memcpy(ip + 20, pl, pllen);
    return 14 + 20 + pllen;
}

int wubu_virtio_net_send(wubu_virtio_net_t *n, const void *frame, uint32_t len)
{
    (void)n;
    if (len > sizeof(last_tx)) return -1;
    memcpy(last_tx, frame, len);
    last_tx_len = len;
    tx_count++;
    const uint8_t *f = last_tx;
    uint16_t type = t_rd16(f + 12);

    if (type == 0x0806 && len >= 42) {
        const uint8_t *a = f + 14;
        uint16_t oper = t_rd16(a + 6);
        uint32_t tpa = t_rd32(a + 24);
        last_arp_tpa = tpa;
        if (oper == 1 && auto_arp && tpa == WUBU_NET_GATEWAY) {
            /* slirp-style ARP reply, built independently */
            uint8_t r[42];
            memcpy(r, f + 22, 6);              /* dst = requester sha */
            memcpy(r + 6, gw_mac, 6);
            t_wr16(r + 12, 0x0806);
            uint8_t *ra = r + 14;
            t_wr16(ra, 1); t_wr16(ra + 2, 0x0800);
            ra[4] = 6; ra[5] = 4;
            t_wr16(ra + 6, 2);
            memcpy(ra + 8, gw_mac, 6);
            t_wr32(ra + 14, WUBU_NET_GATEWAY);
            memcpy(ra + 18, f + 22, 6);        /* tha = requester sha */
            t_wr32(ra + 24, t_rd32(a + 14));   /* tpa = requester spa */
            rx_push(r, 42);
        }
        return 0;
    }
    if (type == 0x0800 && len >= 34) {
        const uint8_t *ip = f + 14;
        uint8_t proto = ip[9];
        const uint8_t *pl = ip + 20;
        uint32_t pllen = t_rd16(ip + 2) - 20;
        if (proto == 1 && auto_icmp && pllen >= 8 && pl[0] == 8) {
            /* ICMP echo reply, built independently */
            uint8_t reply[1514];
            memcpy(reply, pl, pllen);
            reply[0] = 0;
            t_wr16(reply + 2, 0);
            t_wr16(reply + 2, t_csum(reply, pllen));
            uint8_t r[1514];
            uint32_t rl = t_build_ip(r, nic_mac, WUBU_NET_GATEWAY,
                                     WUBU_NET_OUR_IP, 1, reply, pllen, 0x7777);
            rx_push(r, rl);
        }
        if (proto == 17 && auto_udp_echo && pllen >= 8 &&
            t_rd16(pl + 2) == WUBU_NET_GATE_PORT) {
            /* host echo server (via slirp): datagram back to sender */
            uint8_t udp[1514];
            uint32_t ulen = t_rd16(pl + 4);
            t_wr16(udp, t_rd16(pl + 2));       /* sport = gate port */
            t_wr16(udp + 2, t_rd16(pl));       /* dport = our src port */
            t_wr16(udp + 4, (uint16_t)ulen);
            t_wr16(udp + 6, 0);
            memcpy(udp + 8, pl + 8, ulen - 8);
            t_wr16(udp + 6, t_udp_csum(WUBU_NET_GATEWAY, WUBU_NET_OUR_IP,
                                       udp, ulen));
            uint8_t r[1514];
            uint32_t rl = t_build_ip(r, nic_mac, WUBU_NET_GATEWAY,
                                     WUBU_NET_OUR_IP, 17, udp, ulen, 0x7778);
            rx_push(r, rl);
        }
    }
    return 0;
}

/* ---- UDP delivery capture ------------------------------------------ */
typedef struct {
    uint32_t src; uint16_t sport, dport;
    uint8_t data[256]; uint32_t len; int calls;
} cap_t;
static void cap_cb(uint32_t src, uint16_t sport, uint16_t dport,
                   const uint8_t *payload, uint32_t len, void *ctx)
{
    cap_t *c = (cap_t *)ctx;
    c->calls++;
    c->src = src; c->sport = sport; c->dport = dport;
    c->len = len < sizeof(c->data) ? len : sizeof(c->data);
    memcpy(c->data, payload, c->len);
}

static void fake_reset(void)
{
    tx_count = 0; last_tx_len = 0; last_arp_tpa = 0;
    rxq_head = rxq_tail = 0;
    auto_arp = auto_icmp = auto_udp_echo = 1;
}

int main(void)
{
    printf("test_netstack: L5 minimal netstack unit gate\n");

    /* 1. checksums against KNOWN VECTORS ---------------------------- */
    /* IPv4: the classic RFC 1071 example (checksum field zeroed) */
    const uint8_t ipvec[20] = {
        0x45,0x00,0x00,0x73,0x00,0x00,0x40,0x00,0x40,0x11,
        0x00,0x00,0xc0,0xa8,0x00,0x01,0xc0,0xa8,0x00,0xc7
    };
    CHECK(wubu_net_checksum(ipvec, 20) == 0xb861);
    uint8_t ipver[20];
    memcpy(ipver, ipvec, 20);
    t_wr16(ipver + 10, 0xb861);
    CHECK(wubu_net_checksum(ipver, 20) == 0);      /* verification */

    /* UDP: 10.0.2.15:9000 -> 10.0.2.2:39001 payload 'WUBU' (python
     * one's-complement reference: 0x9299) */
    uint8_t udpseg[12];
    t_wr16(udpseg, 9000); t_wr16(udpseg + 2, 39001);
    t_wr16(udpseg + 4, 12); t_wr16(udpseg + 6, 0);
    memcpy(udpseg + 8, "WUBU", 4);
    CHECK(ns_udp_csum(WUBU_NET_OUR_IP, WUBU_NET_GATEWAY, udpseg, 12) == 0x9299);
    uint8_t pseudo[12 + 12];
    t_wr32(pseudo, WUBU_NET_OUR_IP); t_wr32(pseudo + 4, WUBU_NET_GATEWAY);
    pseudo[8] = 0; pseudo[9] = 17; t_wr16(pseudo + 10, 12);
    memcpy(pseudo + 12, udpseg, 12);
    CHECK(wubu_net_checksum(pseudo, 24) == 0x9299);
    t_wr16(pseudo + 12 + 6, 0x9299);
    CHECK(wubu_net_checksum(pseudo, 24) == 0);

    /* ICMP echo request id 0x1234 seq 1 payload 'WUBU-PING': 0x8e81 */
    uint8_t icmpseg[17];
    icmpseg[0] = 8; icmpseg[1] = 0;
    t_wr16(icmpseg + 2, 0); t_wr16(icmpseg + 4, 0x1234);
    t_wr16(icmpseg + 6, 1);
    memcpy(icmpseg + 8, "WUBU-PING", 9);
    CHECK(wubu_net_checksum(icmpseg, 17) == 0x8e81);
    t_wr16(icmpseg + 2, 0x8e81);
    CHECK(wubu_net_checksum(icmpseg, 17) == 0);

    /* IPv4 header carrying that UDP datagram (tot 32, id 1): 0x62bc */
    uint8_t ipu[20] = {
        0x45,0x00,0x00,0x20,0x00,0x01,0x00,0x00,0x40,0x11,
        0x00,0x00,0x0a,0x00,0x02,0x0f,0x0a,0x00,0x02,0x02
    };
    CHECK(wubu_net_checksum(ipu, 20) == 0x62bc);
    /* odd-length folding: trailing byte pads the LOW octet */
    const uint8_t odd[3] = { 0x00, 0x01, 0xf2 };
    CHECK(wubu_net_checksum(odd, 3) == (uint16_t)~(0x0001u + 0xf200u));

    /* 2. init binds the NIC + MAC ----------------------------------- */
    fake_reset();
    CHECK(wubu_netstack_init((wubu_virtio_net_t *)(uintptr_t)1) == 0);
    CHECK(memcmp(g_mac, nic_mac, 6) == 0);
    CHECK(wubu_netstack_init(NULL) == -1);

    /* 3. ARP round-trip: request encoding + reply learned ----------- */
    uint8_t mac[6];
    CHECK(wubu_net_arp_resolve(WUBU_NET_GATEWAY, mac) == 0);
    CHECK(memcmp(mac, gw_mac, 6) == 0);
    CHECK(last_tx_len == 42);
    int bcast = 1;
    for (int i = 0; i < 6; i++) if (last_tx[i] != 0xFF) bcast = 0;
    CHECK(bcast);
    CHECK(memcmp(last_tx + 6, nic_mac, 6) == 0);
    CHECK(t_rd16(last_tx + 12) == 0x0806);
    CHECK(t_rd16(last_tx + 20) == 1);              /* request */
    CHECK(t_rd32(last_tx + 28) == WUBU_NET_OUR_IP);
    CHECK(t_rd32(last_tx + 38) == WUBU_NET_GATEWAY);
    int nc = tx_count;
    CHECK(wubu_net_arp_resolve(WUBU_NET_GATEWAY, mac) == 0);  /* cache */
    CHECK(tx_count == nc);

    /* 4. ARP request FOR us -> automatic reply ---------------------- */
    uint8_t req[42];
    memcpy(req, nic_mac, 6);
    memcpy(req + 6, gw_mac, 6);
    t_wr16(req + 12, 0x0806);
    t_wr16(req + 14, 1); t_wr16(req + 16, 0x0800);
    req[18] = 6; req[19] = 4;
    t_wr16(req + 20, 1);
    memcpy(req + 22, gw_mac, 6);
    t_wr32(req + 28, WUBU_NET_GATEWAY);
    memset(req + 32, 0, 6);
    t_wr32(req + 38, WUBU_NET_OUR_IP);
    rx_push(req, 42);
    CHECK(wubu_net_poll(NULL, NULL) == 1);
    CHECK(last_tx_len == 42);
    CHECK(memcmp(last_tx, gw_mac, 6) == 0);        /* unicast back */
    CHECK(t_rd16(last_tx + 20) == 2);              /* reply */
    CHECK(memcmp(last_tx + 22, nic_mac, 6) == 0);  /* sha = us */
    CHECK(t_rd32(last_tx + 28) == WUBU_NET_OUR_IP);
    CHECK(memcmp(last_tx + 32, gw_mac, 6) == 0);   /* tha = asker */
    CHECK(t_rd32(last_tx + 38) == WUBU_NET_GATEWAY);

    /* 5. UDP send: full frame verified by the independent oracle ---- */
    CHECK(wubu_net_udp_send(WUBU_NET_GATEWAY, WUBU_NET_GATE_PORT,
                            "WUBU", 4) == 0);
    CHECK(last_tx_len == 14 + 20 + 8 + 4);
    CHECK(memcmp(last_tx, gw_mac, 6) == 0);
    CHECK(t_rd16(last_tx + 12) == 0x0800);
    const uint8_t *ip = last_tx + 14;
    CHECK(ip[0] == 0x45);
    CHECK(t_rd16(ip + 2) == 32);
    CHECK(ip[9] == 17);
    CHECK(t_rd32(ip + 12) == WUBU_NET_OUR_IP);
    CHECK(t_rd32(ip + 16) == WUBU_NET_GATEWAY);
    CHECK(t_csum(ip, 20) == 0);                    /* header verifies */
    const uint8_t *udp = ip + 20;
    CHECK(t_rd16(udp) == WUBU_NET_SRC_PORT);
    CHECK(t_rd16(udp + 2) == WUBU_NET_GATE_PORT);
    CHECK(t_rd16(udp + 4) == 12);
    CHECK(memcmp(udp + 8, "WUBU", 4) == 0);
    CHECK(t_udp_csum(WUBU_NET_OUR_IP, WUBU_NET_GATEWAY, udp, 12) == 0);
    /* the fake echoed it; drain so later checks start clean */
    cap_t drain; memset(&drain, 0, sizeof(drain));
    wubu_net_poll(cap_cb, &drain);
    CHECK(drain.calls == 1 && drain.len == 4 &&
          memcmp(drain.data, "WUBU", 4) == 0);

    /* 6. off-subnet destination: ARP for the GATEWAY, IP dst intact - */
    CHECK(wubu_net_udp_send(WUBU_NET_IP4(8, 8, 8, 8), 53, "x", 1) == 0);
    CHECK(last_arp_tpa == WUBU_NET_GATEWAY);
    CHECK(memcmp(last_tx, gw_mac, 6) == 0);
    CHECK(t_rd32(last_tx + 14 + 16) == WUBU_NET_IP4(8, 8, 8, 8));
    wubu_net_poll(NULL, NULL);                     /* drain the echo */

    /* 7. UDP RX dispatch + checksum policing ------------------------ */
    uint8_t u2[12];
    t_wr16(u2, 39001); t_wr16(u2 + 2, 9000);
    t_wr16(u2 + 4, 12); t_wr16(u2 + 6, 0);
    memcpy(u2 + 8, "ECHO", 4);
    t_wr16(u2 + 6, t_udp_csum(WUBU_NET_GATEWAY, WUBU_NET_OUR_IP, u2, 12));
    uint8_t good[1514];
    uint32_t gl = t_build_ip(good, nic_mac, WUBU_NET_GATEWAY,
                             WUBU_NET_OUR_IP, 17, u2, 12, 42);
    cap_t cap; memset(&cap, 0, sizeof(cap));
    rx_push(good, gl);
    CHECK(wubu_net_poll(cap_cb, &cap) == 1);
    CHECK(cap.calls == 1);
    CHECK(cap.src == WUBU_NET_GATEWAY && cap.sport == 39001 && cap.dport == 9000);
    CHECK(cap.len == 4 && memcmp(cap.data, "ECHO", 4) == 0);

    /* corrupted UDP checksum -> dropped */
    uint8_t bad[1514];
    memcpy(bad, good, gl);
    bad[14 + 20 + 6] ^= 0xFF;
    memset(&cap, 0, sizeof(cap));
    rx_push(bad, gl);
    wubu_net_poll(cap_cb, &cap);
    CHECK(cap.calls == 0);
    /* corrupted IP checksum -> dropped */
    memcpy(bad, good, gl);
    bad[14 + 10] ^= 0xFF;
    rx_push(bad, gl);
    wubu_net_poll(cap_cb, &cap);
    CHECK(cap.calls == 0);
    /* not-for-us destination -> dropped */
    uint8_t other[1514];
    uint32_t ol = t_build_ip(other, nic_mac, WUBU_NET_GATEWAY,
                             WUBU_NET_IP4(10, 0, 2, 99), 17, u2, 12, 43);
    rx_push(other, ol);
    wubu_net_poll(cap_cb, &cap);
    CHECK(cap.calls == 0);

    /* 8. ICMP echo request -> automatic echo reply ------------------ */
    uint8_t ic[17];
    ic[0] = 8; ic[1] = 0;
    t_wr16(ic + 2, 0); t_wr16(ic + 4, 0x4242); t_wr16(ic + 6, 7);
    memcpy(ic + 8, "WUBU-PING", 9);
    t_wr16(ic + 2, t_csum(ic, 17));
    uint8_t icf[1514];
    uint32_t il = t_build_ip(icf, nic_mac, WUBU_NET_GATEWAY,
                             WUBU_NET_OUR_IP, 1, ic, 17, 44);
    rx_push(icf, il);
    CHECK(wubu_net_poll(NULL, NULL) == 1);
    CHECK(last_tx_len == il);
    const uint8_t *rip = last_tx + 14;
    CHECK(t_rd32(rip + 12) == WUBU_NET_OUR_IP);    /* swapped */
    CHECK(t_rd32(rip + 16) == WUBU_NET_GATEWAY);
    CHECK(t_csum(rip, 20) == 0);
    const uint8_t *ric = rip + 20;
    CHECK(ric[0] == 0);                            /* echo reply */
    CHECK(t_rd16(ric + 4) == 0x4242 && t_rd16(ric + 6) == 7);
    CHECK(memcmp(ric + 8, "WUBU-PING", 9) == 0);
    CHECK(t_csum(ric, 17) == 0);

    /* 9. ping: answered vs silent ----------------------------------- */
    CHECK(wubu_net_ping(WUBU_NET_GATEWAY) == 0);   /* fake answers */
    auto_icmp = 0;
    CHECK(wubu_net_ping(WUBU_NET_GATEWAY) == -1);  /* bounded timeout */
    auto_icmp = 1;

    /* 10. selftest: the full boot path against the fake slirp ------- */
    fake_reset();
    CHECK(wubu_netstack_init((wubu_virtio_net_t *)(uintptr_t)1) == 0);
    CHECK(wubu_net_selftest() == 0);               /* prints NET-OK */
    auto_udp_echo = 0;
    CHECK(wubu_net_selftest() == -1);              /* prints NET-FAIL */

    if (failures == 0) printf("test_netstack: ALL PASS\n");
    else printf("test_netstack: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}