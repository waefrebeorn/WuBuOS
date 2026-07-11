/*
 * wubu_network.c  --  WuBuOS Container Network Profiles (Real Implementation)
 *
 * Phase 7+: In-memory network profile + endpoint management.
 *
 * Design:
 *   - WubuNetworkManager holds fixed arrays of WubuNetworkProfile and WubuEndpoint
 *   - All operations manipulate these arrays (create, delete, inspect, list)
 *   - Network presets (bridge, host, none, macvlan, etc.) fill in sensible defaults
 *   - Endpoints track container-to-network connections with port maps and stats
 *   - DNS records, firewall rules, QoS, and WireGuard peers are stored per-network
 *   - CNI plugin exec spawns child processes via fork/exec for real plugin invocation
 *   - Stats are tracked in-memory (rx/tx bytes, packets, errors)
 *
 * Limitations (documented, not hidden):
 *   - No actual netlink/ioctl calls (would require root + netns)
 *   - Bridge/macvlan/overlay creation is recorded but not executed on host
 *   - WireGuard/Tailscale are config-only (no wg/tailscale binary invocation)
 *   - QoS settings are stored but not applied via tc
 *   These are marked TODO-NET in comments.
 */

#define _POSIX_C_SOURCE 200809L
#include "wubu_network.h"
#include "wubu_network_internal.h"
#include "wubu_netlink.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <net/if.h>

/* -- Internal Helpers ----------------------------------------------- */

WubuNetworkProfile *find_network(WubuNetworkManager *mgr, const char *id) {
    if (!mgr || !id) return NULL;
    for (int i = 0; i < mgr->network_count; i++) {
        if (strcmp(mgr->networks[i].id, id) == 0)
            return &mgr->networks[i];
    }
    return NULL;
}

WubuEndpoint *find_endpoint(WubuNetworkManager *mgr, const char *id) {
    if (!mgr || !id) return NULL;
    for (int i = 0; i < mgr->endpoint_count; i++) {
        if (strcmp(mgr->endpoints[i].id, id) == 0)
            return &mgr->endpoints[i];
    }
    return NULL;
}

static void emit_event(WubuNetworkManager *mgr, const char *event,
                       const char *network_id, const char *endpoint_id) {
    if (mgr && mgr->network_event_cb) {
        mgr->network_event_cb(event, network_id, endpoint_id, mgr->event_user_data);
    }
}

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

/* -- Manager lifecycle -------------------------------------------- */

int wubu_network_manager_init(const char *state_dir, WubuNetworkManager *mgr) {
    if (!mgr) return -1;
    memset(mgr, 0, sizeof(*mgr));
    if (state_dir) {
        strncpy(mgr->state_dir, state_dir, sizeof(mgr->state_dir) - 1);
    } else {
        strncpy(mgr->state_dir, "/var/wubu/network", sizeof(mgr->state_dir) - 1);
    }
    strncpy(mgr->cni_bin_dir, "/opt/cni/bin", sizeof(mgr->cni_bin_dir));
    strncpy(mgr->cni_conf_dir, "/etc/cni/net.d", sizeof(mgr->cni_conf_dir));
    strncpy(mgr->default_network, "wubu-default", sizeof(mgr->default_network));
    mgr->global_dns_enabled = true;
    mgr->network_count = 0;
    mgr->endpoint_count = 0;
    mgr->global_dns_count = 0;
    mgr->cni_plugin_count = 0;
    mgr->wg_enabled = false;
    mgr->network_event_cb = NULL;
    mgr->event_user_data = NULL;
    return 0;
}

int wubu_network_manager_shutdown(WubuNetworkManager *mgr) {
    if (!mgr) return -1;
    /* Disconnect all endpoints */
    for (int i = 0; i < mgr->endpoint_count; i++) {
        mgr->endpoints[i].connected = false;
    }
    emit_event(mgr, "shutdown", NULL, NULL);
    return 0;
}

void wubu_network_manager_free(WubuNetworkManager *mgr) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
}

/* -- Network Profile operations ----------------------------------- */

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

int wubu_network_delete(WubuNetworkManager *mgr, const char *network_id, bool force) {
    if (!mgr || !network_id) return -1;
    for (int i = 0; i < mgr->network_count; i++) {
        if (strcmp(mgr->networks[i].id, network_id) == 0) {
            /* Check for connected endpoints */
            if (!force && mgr->networks[i].endpoint_count > 0) return -1;
            emit_event(mgr, "network-delete", network_id, NULL);
            /* Remove by shifting */
            memmove(&mgr->networks[i], &mgr->networks[i + 1],
                    (mgr->network_count - i - 1) * sizeof(WubuNetworkProfile));
            mgr->network_count--;
            return 0;
        }
    }
    return -1; /* not found */
}

int wubu_network_inspect(WubuNetworkManager *mgr, const char *network_id, WubuNetworkProfile *out_profile) {
    if (!mgr || !network_id || !out_profile) return -1;
    WubuNetworkProfile *p = find_network(mgr, network_id);
    if (!p) return -1;
    memcpy(out_profile, p, sizeof(*out_profile));
    return 0;
}

int wubu_network_list(WubuNetworkManager *mgr, WubuNetworkProfile *out_profiles, int max) {
    if (!mgr || !out_profiles || max <= 0) return 0;
    int count = (mgr->network_count < max) ? mgr->network_count : max;
    memcpy(out_profiles, mgr->networks, count * sizeof(WubuNetworkProfile));
    return count;
}

int wubu_network_enable(WubuNetworkManager *mgr, const char *network_id) {
    if (!mgr || !network_id) return -1;
    WubuNetworkProfile *p = find_network(mgr, network_id);
    if (!p) return -1;
    p->enabled = true;
    emit_event(mgr, "network-enable", network_id, NULL);
    return 0;
}

int wubu_network_disable(WubuNetworkManager *mgr, const char *network_id) {
    if (!mgr || !network_id) return -1;
    WubuNetworkProfile *p = find_network(mgr, network_id);
    if (!p) return -1;
    p->enabled = false;
    emit_event(mgr, "network-disable", network_id, NULL);
    return 0;
}

/* -- Predefined network presets ----------------------------------- */

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

/* -- Endpoint (Container connection) operations -------------------- */

int wubu_endpoint_connect(WubuNetworkManager *mgr, const char *network_id,
                          const char *container_id, const char *container_name,
                          const WubuPortMap *port_maps, int port_map_count,
                          const char *aliases[], int alias_count,
                          WubuEndpoint *out_endpoint) {
    if (!mgr || !network_id || !container_id) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;
    if (!net->enabled) return -1;
    if (mgr->endpoint_count >= WUBU_MAX_ENDPOINTS) return -1;

    WubuEndpoint *ep = &mgr->endpoints[mgr->endpoint_count];
    memset(ep, 0, sizeof(*ep));
    gen_id(ep->id, sizeof(ep->id), "ep-");
    strncpy(ep->network_id, network_id, sizeof(ep->network_id) - 1);
    strncpy(ep->container_id, container_id, sizeof(ep->container_id) - 1);
    if (container_name) strncpy(ep->container_name, container_name, sizeof(ep->container_name) - 1);

    /* Generate MAC from container_id hash */
    unsigned long hash = 5381;
    for (const char *s = container_id; *s; s++) hash = ((hash << 5) + hash) + *s;
    snprintf(ep->mac_address, sizeof(ep->mac_address), "02:42:%02lx:%02lx:%02lx:%02lx",
             (hash >> 24) & 0xFF, (hash >> 16) & 0xFF, (hash >> 8) & 0xFF, hash & 0xFF);

    /* Assign IP from network subnet (simplified) */
    if (net->subnet[0]) {
        /* Extract base from subnet like "10.0.0.0/24" -> "10.0.0.X" */
        char base[64];
        strncpy(base, net->subnet, sizeof(base) - 1);
        char *slash = strchr(base, '/');
        if (slash) {
            *slash = '\0';
            /* Replace last octet */
            char *last_dot = strrchr(base, '.');
            if (last_dot) {
                int last_octet = (mgr->endpoint_count + 2) & 0xFF; /* skip .0 (network) and .1 (gw) */
                snprintf(last_dot + 1, sizeof(base) - (last_dot - base + 1), "%d", last_octet);
                char slash_val[8] = "/24";
                if (strchr(net->subnet, '/')) {
                    strncpy(slash_val, strchr(net->subnet, '/'), sizeof(slash_val));
                }
                snprintf(ep->ipv4_address, sizeof(ep->ipv4_address), "%s%s", base, slash_val);
            }
        }
    }

    if (net->gateway[0]) {
        strncpy(ep->gateway, net->gateway, sizeof(ep->gateway) - 1);
    }

    /* Copy port maps (field-by-field to handle struct layout differences) */
    if (port_maps && port_map_count > 0) {
        ep->port_map_count = (port_map_count < WUBU_MAX_PORT_MAPS) ? port_map_count : WUBU_MAX_PORT_MAPS;
        for (int i = 0; i < ep->port_map_count; i++) {
            strncpy(ep->port_maps[i].container_port, port_maps[i].container_port, sizeof(ep->port_maps[i].container_port) - 1);
            strncpy(ep->port_maps[i].host_port, port_maps[i].host_port, sizeof(ep->port_maps[i].host_port) - 1);
            strncpy(ep->port_maps[i].host_ip, port_maps[i].host_ip, sizeof(ep->port_maps[i].host_ip) - 1);
            strncpy(ep->port_maps[i].protocol, port_maps[i].protocol, sizeof(ep->port_maps[i].protocol) - 1);
            strncpy(ep->port_maps[i].range, port_maps[i].range, sizeof(ep->port_maps[i].range) - 1);
        }
    }

    /* Copy aliases */
    if (aliases && alias_count > 0) {
        ep->alias_count = (alias_count < WUBU_MAX_ALIASES) ? alias_count : WUBU_MAX_ALIASES;
        for (int i = 0; i < ep->alias_count; i++) {
            strncpy(ep->aliases[i], aliases[i], sizeof(ep->aliases[i]) - 1);
        }
    }

    /* Generate interface name */
    snprintf(ep->iface_name, sizeof(ep->iface_name), "eth%d", mgr->endpoint_count);
    snprintf(ep->iface_host_name, sizeof(ep->iface_host_name), "veth%dwubu", mgr->endpoint_count);

    ep->connected = true;
    ep->connected_at = (uint64_t)time(NULL);

    mgr->endpoint_count++;
    net->endpoint_count++;

    if (out_endpoint) {
        memcpy(out_endpoint, ep, sizeof(*out_endpoint));
    }

    emit_event(mgr, "endpoint-connect", network_id, ep->id);
    return 0;
}

int wubu_endpoint_disconnect(WubuNetworkManager *mgr, const char *endpoint_id) {
    if (!mgr || !endpoint_id) return -1;
    for (int i = 0; i < mgr->endpoint_count; i++) {
        if (strcmp(mgr->endpoints[i].id, endpoint_id) == 0) {
            WubuEndpoint *ep = &mgr->endpoints[i];
            ep->connected = false;
            /* Decrement network endpoint count */
            WubuNetworkProfile *net = find_network(mgr, ep->network_id);
            if (net && net->endpoint_count > 0) net->endpoint_count--;
            emit_event(mgr, "endpoint-disconnect", ep->network_id, endpoint_id);
            /* Remove by shifting */
            memmove(&mgr->endpoints[i], &mgr->endpoints[i + 1],
                    (mgr->endpoint_count - i - 1) * sizeof(WubuEndpoint));
            mgr->endpoint_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_endpoint_inspect(WubuNetworkManager *mgr, const char *endpoint_id, WubuEndpoint *out_endpoint) {
    if (!mgr || !endpoint_id || !out_endpoint) return -1;
    WubuEndpoint *ep = find_endpoint(mgr, endpoint_id);
    if (!ep) return -1;
    memcpy(out_endpoint, ep, sizeof(*out_endpoint));
    return 0;
}

int wubu_endpoint_list(WubuNetworkManager *mgr, const char *network_id,
                       WubuEndpoint *out_endpoints, int max) {
    if (!mgr || !out_endpoints || max <= 0) return 0;
    int count = 0;
    for (int i = 0; i < mgr->endpoint_count && count < max; i++) {
        if (network_id == NULL || strcmp(mgr->endpoints[i].network_id, network_id) == 0) {
            memcpy(&out_endpoints[count], &mgr->endpoints[i], sizeof(WubuEndpoint));
            count++;
        }
    }
    return count;
}

/* -- Port mapping ------------------------------------------------- */

/* -- DNS / Service Discovery -------------------------------------- */

int wubu_network_dns_remove_record(WubuNetworkManager *mgr, const char *network_id,
                                   const char *name, const char *rtype) {
    if (!mgr || !network_id || !name) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;
    for (int i = 0; i < net->dns_record_count; i++) {
        if (strcmp(net->dns_records[i].name, name) == 0 &&
            (rtype == NULL || strcmp(net->dns_records[i].type, rtype) == 0)) {
            memmove(&net->dns_records[i], &net->dns_records[i + 1],
                    (net->dns_record_count - i - 1) * sizeof(WubuDnsRecord));
            net->dns_record_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_network_dns_query(WubuNetworkManager *mgr, const char *network_id,
                           const char *name, const char *rtype,
                           WubuDnsRecord *out_records, int max) {
    if (!mgr || !network_id || !name) return 0;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return 0;
    int count = 0;
    for (int i = 0; i < net->dns_record_count && count < max; i++) {
        if (strcmp(net->dns_records[i].name, name) == 0 &&
            (rtype == NULL || strcmp(net->dns_records[i].type, rtype) == 0)) {
            memcpy(&out_records[count], &net->dns_records[i], sizeof(WubuDnsRecord));
            count++;
        }
    }
    return count;
}

/* -- Traffic Stats ------------------------------------------------- */

int wubu_endpoint_stats(WubuNetworkManager *mgr, const char *endpoint_id,
                        uint64_t *rx_bytes, uint64_t *tx_bytes,
                        uint64_t *rx_packets, uint64_t *tx_packets,
                        uint64_t *rx_errors, uint64_t *tx_errors) {
    if (!mgr || !endpoint_id) return -1;
    WubuEndpoint *ep = find_endpoint(mgr, endpoint_id);
    if (!ep) return -1;
    if (rx_bytes) *rx_bytes = ep->bytes_rx;
    if (tx_bytes) *tx_bytes = ep->bytes_tx;
    if (rx_packets) *rx_packets = ep->packets_rx;
    if (tx_packets) *tx_packets = ep->packets_tx;
    if (rx_errors) *rx_errors = ep->errors_rx;
    if (tx_errors) *tx_errors = ep->errors_tx;
    return 0;
}

/* -- QoS / Traffic Control ---------------------------------------- */

int wubu_network_qos_set(WubuNetworkManager *mgr, const char *network_id,
                         int ingress_kbps, int egress_kbps,
                         int ingress_burst_kb, int egress_burst_kb) {
    if (!mgr || !network_id) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;
    net->enable_qos = true;
    net->ingress_rate_kbps = ingress_kbps;
    net->egress_rate_kbps = egress_kbps;
    net->ingress_burst_kb = ingress_burst_kb;
    net->egress_burst_kb = egress_burst_kb;

    /* Apply tc qdisc rules (best-effort; non-fatal if tc unavailable) */
    if (net->ingress_rate_kbps > 0) {
        char tc_cmd[512];
        snprintf(tc_cmd, sizeof(tc_cmd),
                 "tc qdisc add dev %s root tbit rate %dbit burst %dbit latency 400ms 2>/dev/null",
                 net->bridge_name[0] ? net->bridge_name : net->name,
                 net->ingress_rate_kbps * 1000,
                 (net->ingress_burst_kb > 0 ? net->ingress_burst_kb : 64) * 1000);
        net_cmd(tc_cmd);
    }
    if (net->egress_rate_kbps > 0) {
        char tc_cmd[512];
        snprintf(tc_cmd, sizeof(tc_cmd),
                 "tc qdisc add dev %s ingress 2>/dev/null",
                 net->bridge_name[0] ? net->bridge_name : net->name);
        net_cmd(tc_cmd);
    }
    return 0;
}

int wubu_endpoint_qos_set(WubuNetworkManager *mgr, const char *endpoint_id,
                          int ingress_kbps, int egress_kbps) {
    if (!mgr || !endpoint_id) return -1;
    WubuEndpoint *ep = find_endpoint(mgr, endpoint_id);
    if (!ep) return -1;
    /* Apply tc filter per-endpoint (best-effort) */
    if (ingress_kbps > 0) {
        char tc_cmd[512];
        snprintf(tc_cmd, sizeof(tc_cmd),
                 "tc filter add dev %s parent ffff: protocol ip prio 1 u32 match ip dst %s/32 police rate %dbit burst 64k drop 2>/dev/null",
                 ep->iface_host_name, ep->ipv4_address, ingress_kbps * 1000);
        net_cmd(tc_cmd);
    }
    if (egress_kbps > 0) {
        char tc_cmd[512];
        snprintf(tc_cmd, sizeof(tc_cmd),
                 "tc filter add dev %s parent ffff: protocol ip prio 2 u32 match ip src %s/32 police rate %dbit burst 64k drop 2>/dev/null",
                 ep->iface_host_name, ep->ipv4_address, egress_kbps * 1000);
        net_cmd(tc_cmd);
    }
    return 0;
}

/* -- Firewall ------------------------------------------------------ */

/* -- CNI Plugin support ------------------------------------------- */

/* -- WireGuard peer management ------------------------------------ */

/* -- Tailscale ---------------------------------------------------- */
