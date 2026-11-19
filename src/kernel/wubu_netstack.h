/*
 * wubu_netstack.h -- minimal Ethernet/ARP/IPv4/ICMP/UDP stack (L5).
 *
 * Socket-lite kernel API over the virtio-net driver: enough for
 * 9P-over-UDP to become a later leg. Static addressing matches the
 * QEMU user-netdev (slirp) defaults: guest 10.0.2.15/24, gateway
 * 10.0.2.2 (slirp answers ARP for it and forwards guest UDP to the
 * host loopback). All checksums computed in software (the driver
 * negotiates no offloads).
 *
 * IPs are uint32_t in HOST byte order (WUBU_NET_IP4(10,0,2,15));
 * wire encoding is big-endian inside the stack. C11 freestanding.
 */
#ifndef WUBU_NETSTACK_H
#define WUBU_NETSTACK_H

#include <stdint.h>
#include <stddef.h>
#include "wubu_virtio_net.h"

#define WUBU_NET_IP4(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
     ((uint32_t)(c) << 8) | (uint32_t)(d))

#define WUBU_NET_OUR_IP    WUBU_NET_IP4(10, 0, 2, 15)
#define WUBU_NET_GATEWAY   WUBU_NET_IP4(10, 0, 2, 2)
#define WUBU_NET_NETMASK   WUBU_NET_IP4(255, 255, 255, 0)
#define WUBU_NET_SRC_PORT  9000u     /* our fixed UDP source port */
/* The end-to-end gate's host echo server listens here; the guest
 * selftest sends its marker to gateway:WUBU_NET_GATE_PORT (slirp
 * forwards it to host loopback). Fixed because the metal kernel has
 * no command-line channel; the gate script binds the same constant. */
#define WUBU_NET_GATE_PORT 39001u

/* Bind the stack to a live virtio-net device (reads its MAC).
 * 0 ok, -1 bad device. */
int wubu_netstack_init(wubu_virtio_net_t *nic);

/* Internet checksum (one's complement, big-endian wire bytes).
 * Returns the checksum to WRITE; over a segment INCLUDING its valid
 * checksum field the result is 0 (verification). */
uint16_t wubu_net_checksum(const void *data, uint32_t len);

/* Resolve an IPv4 address to a MAC via ARP (cache first, then
 * request/reply with bounded retries). 0 ok, -1 unresolved. */
int wubu_net_arp_resolve(uint32_t ip, uint8_t mac_out[6]);

/* Send one UDP datagram (ARP-resolves the destination, off-subnet
 * targets go to the gateway). 0 ok, -1 error. len <= 1472. */
int wubu_net_udp_send(uint32_t ip, uint16_t port, const void *buf, uint32_t len);

/* UDP datagram delivered to the poll callback. */
typedef void (*wubu_net_rx_cb)(uint32_t src_ip, uint16_t src_port,
                               uint16_t dst_port, const uint8_t *payload,
                               uint32_t len, void *ctx);

/* Pump the NIC: dispatch ARP (cache + auto-reply), ICMP (auto echo
 * reply + ping completion) and UDP (to cb). Returns frames processed
 * (bounded; safe to call in a loop). cb may be NULL to pump silently. */
int wubu_net_poll(wubu_net_rx_cb cb, void *ctx);

/* ICMP echo request to `ip`, wait for the matching reply (bounded).
 * 0 answered, -1 timeout/unresolved. */
int wubu_net_ping(uint32_t ip);

/* Boot-time end-to-end selftest (guarded on virtio-net presence by the
 * caller): ARP for the gateway, ICMP ping (informational), then a real
 * UDP round-trip against the gate's host echo server. Prints
 * 'NET-OK: udp round-trip <n> bytes' on success. 0 ok, -1 failed. */
int wubu_net_selftest(void);

#endif /* WUBU_NETSTACK_H */