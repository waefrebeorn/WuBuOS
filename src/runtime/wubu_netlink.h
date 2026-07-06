/*
 * wubu_netlink.h  --  WuBuOS Netlink rtnetlink API
 *
 * Extracted from wubu_network.c (2026-07-06): all netlink socket management,
 * link/address operations, and low-level helpers.
 *
 * C11 only. No globals exposed (g_nl_sock is static in .c).
 * Internal helpers are not declared here — use only the public API.
 */
#ifndef WUBU_NETLINK_H
#define WUBU_NETLINK_H

#include "wubu_network.h"

/* -- Netlink socket lifecycle -------------------------------------- */

/* Opens (or returns existing) netlink socket. Returns fd or -1. */
int nl_socket_open(void);

/* Closes netlink socket if open. */
void nl_socket_close(void);

/* -- Link (interface) operations ----------------------------------- */

/* Creates a link (bridge, macvlan, ipvlan, vxlan, dummy, etc.).
 *   name        - interface name (e.g., "br0", "macvlan0")
 *   kind        - link kind ("bridge", "macvlan", "ipvlan", "vxlan", "dummy", "vlan")
 *   parent      - parent interface name (for macvlan/ipvlan/vxlan/vlan), or NULL
 *   vlan_id     - VLAN ID (for kind="vlan"), 0 for others
 *   vxlan_id    - VXLAN VNI string (for kind="vxlan"), or NULL
 *   vxlan_port  - VXLAN destination port (for kind="vxlan"), 0 for default
 * for default
 *   vxlan_remote- VXLAN remote IP for multicast/remote (kind="vxlan"), or NULL
 *   vxlan_encrypt- whether VXLAN encryption is requested (kernel 5.11+)
 * Returns 0 on success, -1 on error. */
int netlink_link_create(const char *name, const char *kind,
                        const char *parent, int vlan_id,
                        const char *vxlan_id, int vxlan_port,
                        const char *vxlan_remote, bool vxlan_encrypt);

/* Deletes a link by name. Returns 0 on success, -1 on error. */
int netlink_link_delete(const char *name);

/* Sets link up (true) or down (false). Returns 0 on success, -1 on error. */
int netlink_link_set_up(const char *name, bool up);

/* -- Address operations -------------------------------------------- */

/* Adds an IPv4 address with CIDR prefix to an interface.
 * Returns 0 on success, -1 on error. */
int netlink_addr_add(const char *iface, const char *ip_with_cidr);

/* -- Composite helpers --------------------------------------------- */

/* Brings interface up and assigns IP address. Returns 0 on success. */
int net_iface_up(const char *iface, const char *ip_with_cidr);

/* -- Utility ------------------------------------------------------- */

/* Runs shell command. Returns system() exit code. */
int net_cmd(const char *cmd);

/* Returns current monotonic time in milliseconds. */
uint64_t now_ms(void);

/* Generates a unique ID with prefix. Output written to 'out' (size 'len'). */
void gen_id(char *out, size_t len, const char *prefix);

#endif /* WUBU_NETLINK_H */