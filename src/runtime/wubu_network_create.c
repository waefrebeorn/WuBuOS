/* wubu_network_create.c -- WuBuOS network: network-create subsystem.
 * Extracted from wubu_network.c (separable leaf). Self-contained: wubu_network_create
 * + 8 create_* variants (bridge/host/none/macvlan/ipvlan/overlay/wireguard/tailscale)
 * + the shared profile_default helper. Uses find_network/emit_event (internal header)
 * + net_cmd (wubu_netlink.h), already shared. C11, minimal includes.
 */
#include "wubu_network.h"
#include "wubu_network_internal.h"
#include "wubu_netlink.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static void profile_default(WubuNetworkProfile *p, const char *name, WubuNetworkMode mode) {
    memset(p, 0, sizeof(*p));
    gen_id(p->id, sizeof(p->id), "net");
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->mode = mode;
    p->ipam_mode = WUBU_IPAM_DHCP;
    strncpy(p->subnet, "10.0.0.0/24", sizeof(p->subnet) - 1);
    strncpy(p->gateway, "10.0.0.1", sizeof(p->gateway) - 1);
    p->enable_dns = true;
    p->dns_server_count = 0;
    p->dns_search_count = 0;
    p->dns_record_count = 0;
    p->firewall_rule_count = 0;
    p->enable_qos = false;
    p->created = false;
    p->enabled = false;
}

int wubu_network_create(WubuNetworkManager *mgr, const WubuNetworkProfile *profile) {
    if (!mgr || !profile) return -1;
    if (mgr->network_count >= WUBU_MAX_NETWORKS) return -1;
    if (find_network(mgr, profile->id)) return -1; /* duplicate */

    WubuNetworkProfile *p = &mgr->networks[mgr->network_count];
    memcpy(p, profile, sizeof(*p));

    if (p->id[0] == '\0') {
        gen_id(p->id, sizeof(p->id), "net-");
    }
    if (p->created_at == 0) {
        p->created_at = (uint64_t)time(NULL);
    }
    p->created = true;
    p->enabled = true;
    p->endpoint_count = 0;

    mgr->network_count++;
    emit_event(mgr, "network-create", p->id, NULL);
    return 0;
}

int wubu_network_create_bridge(WubuNetworkManager *mgr, const char *name, const char *subnet,
                               const char *gateway, bool enable_dns) {
    if (!mgr || !name) return -1;
    if (mgr->network_count >= WUBU_MAX_NETWORKS) return -1;

    WubuNetworkProfile *p = &mgr->networks[mgr->network_count];
    profile_default(p, name, WUBU_NET_BRIDGE);

    gen_id(p->id, sizeof(p->id), "br-");
    if (subnet) strncpy(p->subnet, subnet, sizeof(p->subnet) - 1);
    if (gateway) strncpy(p->gateway, gateway, sizeof(p->gateway) - 1);
    p->enable_dns = enable_dns;
    snprintf(p->bridge_name, sizeof(p->bridge_name), "wubu%d", mgr->network_count);
    if (!subnet) strncpy(p->subnet, "10.0.0.0/24", sizeof(p->subnet) - 1);
    if (!gateway) strncpy(p->gateway, "10.0.0.1", sizeof(p->gateway) - 1);
    p->enabled = true;  /* a created bridge is enabled by default (matches wubu_network_create) */

    /* Create bridge via netlink rtnetlink */
    int rc = netlink_link_create(p->bridge_name, "bridge", NULL, 0, NULL, 0, NULL, false);
    if (rc != 0) {
        fprintf(stderr, "[wubu_net] bridge %s host setup failed (non-fatal): %s\n", 
                p->bridge_name, strerror(errno));
    } else {
        /* Assign gateway IP to the bridge */
        net_iface_up(p->bridge_name, p->gateway);
        /* Enable forwarding for bridge */
        net_cmd("sysctl -w net.ipv4.ip_forward=1");
    }

    p->created = true;
    p->created_at = (uint64_t)time(NULL);
    mgr->network_count++;
    emit_event(mgr, "network-create", p->id, NULL);
    return 0;
}

int wubu_network_create_host(WubuNetworkManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    if (mgr->network_count >= WUBU_MAX_NETWORKS) return -1;

    WubuNetworkProfile *p = &mgr->networks[mgr->network_count];
    profile_default(p, name, WUBU_NET_HOST);
    gen_id(p->id, sizeof(p->id), "host-");
    p->enable_dns = false;
    p->created = true;
    p->enabled = true;  /* a created host network is enabled by default */
    p->created_at = (uint64_t)time(NULL);
    mgr->network_count++;
    emit_event(mgr, "network-create", p->id, NULL);
    return 0;
}

int wubu_network_create_none(WubuNetworkManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    if (mgr->network_count >= WUBU_MAX_NETWORKS) return -1;

    WubuNetworkProfile *p = &mgr->networks[mgr->network_count];
    profile_default(p, name, WUBU_NET_NONE);
    gen_id(p->id, sizeof(p->id), "none-");
    p->enable_dns = false;
    p->created = true;
    p->created_at = (uint64_t)time(NULL);
    mgr->network_count++;
    emit_event(mgr, "network-create", p->id, NULL);
    return 0;
}

int wubu_network_create_macvlan(WubuNetworkManager *mgr, const char *name, const char *parent_iface,
                                const char *subnet, const char *gateway, int vlan_id) {
    if (!mgr || !name || !parent_iface) return -1;
    if (mgr->network_count >= WUBU_MAX_NETWORKS) return -1;

    WubuNetworkProfile *p = &mgr->networks[mgr->network_count];
    profile_default(p, name, WUBU_NET_MACVLAN);
    gen_id(p->id, sizeof(p->id), "macvlan-");
    strncpy(p->parent_interface, parent_iface, sizeof(p->parent_interface) - 1);
    if (subnet) strncpy(p->subnet, subnet, sizeof(p->subnet) - 1);
    if (gateway) strncpy(p->gateway, gateway, sizeof(p->gateway) - 1);
    p->vlan_id = vlan_id;

    /* Create macvlan via netlink rtnetlink */
    int rc = netlink_link_create(p->name, "macvlan", p->parent_interface, 0, NULL, 0, NULL, false);
    if (rc != 0) {
        fprintf(stderr, "[wubu_net] macvlan %s host setup failed (non-fatal): %s\n", p->name, strerror(errno));
    } else {
        net_iface_up(p->name, p->subnet);
    }

    p->created = true;
    p->created_at = (uint64_t)time(NULL);
    mgr->network_count++;
    emit_event(mgr, "network-create", p->id, NULL);
    return 0;
}

int wubu_network_create_ipvlan(WubuNetworkManager *mgr, const char *name, const char *parent_iface,
                               const char *subnet, const char *gateway) {
    if (!mgr || !name || !parent_iface) return -1;
    if (mgr->network_count >= WUBU_MAX_NETWORKS) return -1;

    WubuNetworkProfile *p = &mgr->networks[mgr->network_count];
    profile_default(p, name, WUBU_NET_IPVLAN);
    gen_id(p->id, sizeof(p->id), "ipvlan-");
    strncpy(p->parent_interface, parent_iface, sizeof(p->parent_interface) - 1);
    if (subnet) strncpy(p->subnet, subnet, sizeof(p->subnet) - 1);
    if (gateway) strncpy(p->gateway, gateway, sizeof(p->gateway) - 1);

    /* Create ipvlan via netlink rtnetlink */
    int rc = netlink_link_create(p->name, "ipvlan", p->parent_interface, 0, NULL, 0, NULL, false);
    if (rc != 0) {
        fprintf(stderr, "[wubu_net] ipvlan %s host setup failed (non-fatal): %s\n", p->name, strerror(errno));
    } else {
        net_iface_up(p->name, p->subnet);
    }

    p->created = true;
    p->created_at = (uint64_t)time(NULL);
    mgr->network_count++;
    emit_event(mgr, "network-create", p->id, NULL);
    return 0;
}

int wubu_network_create_overlay(WubuNetworkManager *mgr, const char *name, const char *subnet,
                                const char *gateway, const char *vni, int port, bool encrypt) {
    if (!mgr || !name) return -1;
    if (mgr->network_count >= WUBU_MAX_NETWORKS) return -1;

    WubuNetworkProfile *p = &mgr->networks[mgr->network_count];
    profile_default(p, name, WUBU_NET_OVERLAY);
    gen_id(p->id, sizeof(p->id), "overlay-");
    if (subnet) strncpy(p->subnet, subnet, sizeof(p->subnet) - 1);
    if (gateway) strncpy(p->gateway, gateway, sizeof(p->gateway) - 1);
    if (vni) strncpy(p->vxlan_vni, vni, sizeof(p->vxlan_vni) - 1);
    p->vxlan_port = port ? port : 4789;
    p->vxlan_encrypt = encrypt;

    /* Create VXLAN via netlink rtnetlink */
    const char *vxlan_id = p->vxlan_vni[0] ? p->vxlan_vni : "100";
    int rc = netlink_link_create(p->name, "vxlan", "eth0", 0, vxlan_id, p->vxlan_port, NULL, p->vxlan_encrypt);
    if (rc != 0) {
        fprintf(stderr, "[wubu_net] VXLAN %s host setup failed (non-fatal): %s\n", p->name, strerror(errno));
    } else {
        net_iface_up(p->name, p->subnet);
    }

    p->created = true;
    p->created_at = (uint64_t)time(NULL);
    mgr->network_count++;
    emit_event(mgr, "network-create", p->id, NULL);
    return 0;
}

int wubu_network_create_wireguard(WubuNetworkManager *mgr, const char *name, const char *subnet,
                                  const char *private_key, int listen_port) {
    if (!mgr || !name) return -1;
    if (mgr->network_count >= WUBU_MAX_NETWORKS) return -1;

    WubuNetworkProfile *p = &mgr->networks[mgr->network_count];
    profile_default(p, name, WUBU_NET_WIREGUARD);
    gen_id(p->id, sizeof(p->id), "wg-");
    if (subnet) strncpy(p->subnet, subnet, sizeof(p->subnet) - 1);
    if (private_key) strncpy(p->wg_private_key, private_key, sizeof(p->wg_private_key) - 1);
    p->wg_listen_port = listen_port;

    /* Attempt WireGuard interface setup via netlink (best-effort) */
    /* Create a dummy interface for the WireGuard endpoint */
    int rc = netlink_link_create(p->name, "dummy", NULL, 0, NULL, 0, NULL, false);
    if (rc != 0) {
        fprintf(stderr, "[wubu_net] WG dummy iface %s failed (non-fatal): %s\n", p->name, strerror(errno));
    } else {
        /* Bring it up via netlink */
        netlink_link_set_up(p->name, true);
        /* If wg tool is available, configure the WireGuard interface (best-effort) */
        if (p->wg_private_key[0]) {
            char wg_cmd[1024];
            snprintf(wg_cmd, sizeof(wg_cmd), "wg set %s private-key <(echo %s)", p->name, p->wg_private_key);
            net_cmd(wg_cmd);
            if (p->wg_listen_port > 0) {
                snprintf(wg_cmd, sizeof(wg_cmd), "wg set %s listen-port %d", p->name, p->wg_listen_port);
                net_cmd(wg_cmd);
            }
        }
    }

    p->created = true;
    p->created_at = (uint64_t)time(NULL);
    mgr->network_count++;
    emit_event(mgr, "network-create", p->id, NULL);
    return 0;
}

int wubu_network_create_tailscale(WubuNetworkManager *mgr, const char *name, const char *auth_key) {
    if (!mgr || !name) return -1;
    if (mgr->network_count >= WUBU_MAX_NETWORKS) return -1;

    WubuNetworkProfile *p = &mgr->networks[mgr->network_count];
    profile_default(p, name, WUBU_NET_TAILSCALE);
    gen_id(p->id, sizeof(p->id), "ts-");
    if (auth_key) strncpy(p->ts_auth_key, auth_key, sizeof(p->ts_auth_key) - 1);

    /* Attempt Tailscale invocation (best-effort; binary may not exist) */
    if (p->ts_auth_key[0]) {
        char ts_cmd[1024];
        snprintf(ts_cmd, sizeof(ts_cmd), "tailscale up --authkey=%s --accept-routes 2>/dev/null", p->ts_auth_key);
        if (net_cmd(ts_cmd) != 0) {
            fprintf(stderr, "[wubu_net] Tailscale up failed (non-fatal, binary may be absent)\n");
        }
    }

    p->created = true;
    p->created_at = (uint64_t)time(NULL);
    mgr->network_count++;
    emit_event(mgr, "network-create", p->id, NULL);
    return 0;
}
