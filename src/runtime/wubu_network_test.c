/*
 * wubu_network_test.c  --  WuBuOS Container Network Profiles Test Suite
 *
 * Tests all network operations: manager lifecycle, CRUD,
 * preset creation, endpoints, port maps, DNS, QoS, firewall,
 * WireGuard, Tailscale, stats, CNI plugin load.
 */

#define _POSIX_C_SOURCE 200809L
#include "wubu_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_run = 0, g_pass = 0;

#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s (line %d)\n", msg, __LINE__); } \
} while(0)

/* WubuNetworkManager is ~12MB — heap-allocate via calloc */
static WubuNetworkManager *nmgr(void) {
    return calloc(1, sizeof(WubuNetworkManager));
}
#define NNMGR_FREE(m) do { wubu_network_manager_free(m); free(m); } while(0)


int main(void) {
    printf("=== WuBuOS Network Profiles Test Suite ===\n\n");

    /* -- Manager lifecycle ---------------------------------------- */
    printf("[Manager Lifecycle]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        T(wubu_network_manager_init("/tmp/wubu-net-test", mgr) == 0, "init");
        T(strcmp(mgr->state_dir, "/tmp/wubu-net-test") == 0, "state_dir set");
        T(mgr->network_count == 0, "initially empty");
        T(mgr->endpoint_count == 0, "no endpoints initially");
        T(wubu_network_manager_shutdown(mgr) == 0, "shutdown");
        NNMGR_FREE(mgr);
    }

    /* -- NULL/invalid handling ------------------------------------ */
    printf("\n[Error Handling]\n");
    {
        T(wubu_network_manager_init(NULL, NULL) == -1, "init NULL mgr");
        T(wubu_network_manager_shutdown(NULL) == -1, "shutdown NULL mgr");
        T(wubu_network_create(NULL, NULL) == -1, "create NULL args");
        T(wubu_network_delete(NULL, "x", false) == -1, "delete NULL mgr");
        T(wubu_network_inspect(NULL, "x", NULL) == -1, "inspect NULL mgr");
        T(wubu_network_enable(NULL, "x") == -1, "enable NULL mgr");
        T(wubu_network_disable(NULL, "x") == -1, "disable NULL mgr");
        T(wubu_network_list(NULL, NULL, 10) == 0, "list NULL mgr returns 0");
    }

    /* -- Network CRUD --------------------------------------------- */
    printf("\n[Network CRUD]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        /* Create via generic create */
        WubuNetworkProfile prof;
        memset(&prof, 0, sizeof(prof));
        strncpy(prof.name, "testnet", sizeof(prof.name));
        prof.mode = WUBU_NET_BRIDGE;
        strncpy(prof.subnet, "10.10.0.0/24", sizeof(prof.subnet));
        strncpy(prof.gateway, "10.10.0.1", sizeof(prof.gateway));
        T(wubu_network_create(mgr, &prof) == 0, "create network");
        T(mgr->network_count == 1, "network count = 1");

        /* ID was auto-generated — get it from the created network */
        WubuNetworkProfile created;
        T(wubu_network_list(mgr, &created, 1) == 1, "list to get ID");
        T(created.id[0] != '\0', "network ID auto-assigned");

        /* Inspect */
        WubuNetworkProfile inspect;
        T(wubu_network_inspect(mgr, created.id, &inspect) == 0, "inspect by ID");
        T(strcmp(inspect.name, "testnet") == 0, "inspect name");
        T(inspect.mode == WUBU_NET_BRIDGE, "inspect mode");

        /* Duplicate should fail */
        WubuNetworkProfile prof2;
        memset(&prof2, 0, sizeof(prof2));
        strncpy(prof2.id, created.id, sizeof(prof2.id));
        strncpy(prof2.name, "dup", sizeof(prof2.name));
        T(wubu_network_create(mgr, &prof2) == -1, "duplicate ID rejected");

        /* Delete */
        T(wubu_network_delete(mgr, created.id, false) == 0, "delete network");
        T(mgr->network_count == 0, "network count after delete");

        /* Delete nonexistent */
        T(wubu_network_delete(mgr, "nope", false) == -1, "delete nonexistent");

        NNMGR_FREE(mgr);
    }

    /* -- Preset: bridge, host, none ------------------------------- */
    printf("\n[Bridge Preset]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        T(wubu_network_create_bridge(mgr, "wubu-default", "10.1.0.0/24", "10.1.0.1", true) == 0,
          "create bridge");
        T(mgr->network_count == 1, "bridge count");

        WubuNetworkProfile *p = &mgr->networks[0];
        T(p->mode == WUBU_NET_BRIDGE, "bridge mode");
        T(p->enable_dns == true, "bridge DNS enabled");
        T(strcmp(p->subnet, "10.1.0.0/24") == 0, "bridge subnet");
        T(strcmp(p->gateway, "10.1.0.1") == 0, "bridge gateway");
        T(p->created == true, "bridge marked created");

        NNMGR_FREE(mgr);
    }

    printf("\n[Host Preset]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        T(wubu_network_create_host(mgr, "host-net") == 0, "create host");
        T(mgr->networks[0].mode == WUBU_NET_HOST, "host mode");
        T(mgr->networks[0].enable_dns == false, "host DNS disabled");

        NNMGR_FREE(mgr);
    }

    printf("\n[None Preset]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        T(wubu_network_create_none(mgr, "null-net") == 0, "create none");
        T(mgr->networks[0].mode == WUBU_NET_NONE, "none mode");

        NNMGR_FREE(mgr);
    }

    /* -- Preset: macvlan, ipvlan ---------------------------------- */
    printf("\n[MACVLAN Preset]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        T(wubu_network_create_macvlan(mgr, "mv-net", "eth0", "10.2.0.0/24", "10.2.0.1", 100) == 0,
          "create macvlan");
        WubuNetworkProfile *p = &mgr->networks[0];
        T(p->mode == WUBU_NET_MACVLAN, "macvlan mode");
        T(strcmp(p->parent_interface, "eth0") == 0, "macvlan parent");
        T(p->vlan_id == 100, "macvlan vlan_id");

        NNMGR_FREE(mgr);
    }

    printf("\n[IPVLAN Preset]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        T(wubu_network_create_ipvlan(mgr, "iv-net", "eth0", "10.3.0.0/24", "10.3.0.1") == 0,
          "create ipvlan");
        T(mgr->networks[0].mode == WUBU_NET_IPVLAN, "ipvlan mode");

        NNMGR_FREE(mgr);
    }

    /* -- Preset: overlay, wireguard, tailscale -------------------- */
    printf("\n[Overlay Preset]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        T(wubu_network_create_overlay(mgr, "vx-net", "10.4.0.0/24", "10.4.0.1", "vni100", 4789, true) == 0,
          "create overlay");
        WubuNetworkProfile *p = &mgr->networks[0];
        T(p->mode == WUBU_NET_OVERLAY, "overlay mode");
        T(p->vxlan_port == 4789, "overlay port");
        T(p->vxlan_encrypt == true, "overlay encrypt");

        NNMGR_FREE(mgr);
    }

    printf("\n[WireGuard Preset]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        T(wubu_network_create_wireguard(mgr, "wg-net", "10.5.0.0/24", "privkey123", 51820) == 0,
          "create wireguard");
        WubuNetworkProfile *p = &mgr->networks[0];
        T(p->mode == WUBU_NET_WIREGUARD, "wireguard mode");
        T(p->wg_listen_port == 51820, "wg listen port");
        T(strcmp(p->wg_private_key, "privkey123") == 0, "wg private key");

        NNMGR_FREE(mgr);
    }

    printf("\n[Tailscale Preset]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        T(wubu_network_create_tailscale(mgr, "ts-net", "tskey-abc") == 0,
          "create tailscale");
        WubuNetworkProfile *p = &mgr->networks[0];
        T(p->mode == WUBU_NET_TAILSCALE, "tailscale mode");
        T(strcmp(p->ts_auth_key, "tskey-abc") == 0, "ts auth key");

        NNMGR_FREE(mgr);
    }

    /* -- Enable/Disable ------------------------------------------- */
    printf("\n[Enable/Disable]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_bridge(mgr, "br0", "10.0.0.0/24", "10.0.0.1", true);

        T(mgr->networks[0].enabled == true, "network enabled by default");
        T(wubu_network_disable(mgr, mgr->networks[0].id) == 0, "disable network");
        T(mgr->networks[0].enabled == false, "network disabled");
        T(wubu_network_enable(mgr, mgr->networks[0].id) == 0, "re-enable network");
        T(mgr->networks[0].enabled == true, "network re-enabled");

        NNMGR_FREE(mgr);
    }

    /* -- Endpoint connect/disconnect ------------------------------ */
    printf("\n[Endpoint CRUD]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_bridge(mgr, "br0", "10.0.0.0/24", "10.0.0.1", true);

        const char *net_id = mgr->networks[0].id;
        WubuEndpoint ep;
        T(wubu_endpoint_connect(mgr, net_id, "container-1", "web",
                                 NULL, 0, NULL, 0, &ep) == 0, "connect endpoint");
        T(mgr->endpoint_count == 1, "endpoint count = 1");
        T(ep.connected == true, "endpoint connected");
        T(strcmp(ep.container_id, "container-1") == 0, "container_id");
        T(strcmp(ep.container_name, "web") == 0, "container_name");
        T(strcmp(ep.network_id, net_id) == 0, "network_id");
        T(ep.mac_address[0] != '\0', "MAC generated");
        T(ep.ipv4_address[0] != '\0', "IPv4 assigned");
        T(ep.gateway[0] != '\0', "gateway set");
        T(strcmp(ep.gateway, "10.0.0.1") == 0, "gateway matches network");

        /* Inspect endpoint */
        WubuEndpoint ep2;
        T(wubu_endpoint_inspect(mgr, ep.id, &ep2) == 0, "inspect endpoint");
        T(strcmp(ep2.id, ep.id) == 0, "inspect returns same ID");

        /* Disconnect */
        T(wubu_endpoint_disconnect(mgr, ep.id) == 0, "disconnect endpoint");
        T(mgr->endpoint_count == 0, "endpoint count after disconnect");

        NNMGR_FREE(mgr);
    }

    /* -- Endpoint with port maps and aliases ---------------------- */
    printf("\n[Endpoint Port Maps/Aliases]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_bridge(mgr, "br0", "10.0.0.0/24", "10.0.0.1", true);

        WubuPortMap maps[2];
        memset(maps, 0, sizeof(maps));
        strncpy(maps[0].container_port, "80/tcp", sizeof(maps[0].container_port));
        strncpy(maps[0].host_port, "8080", sizeof(maps[0].host_port));
        strncpy(maps[0].protocol, "tcp", sizeof(maps[0].protocol));
        strncpy(maps[1].container_port, "443/tcp", sizeof(maps[1].container_port));
        strncpy(maps[1].host_port, "8443", sizeof(maps[1].host_port));
        strncpy(maps[1].protocol, "tcp", sizeof(maps[1].protocol));

        const char *aliases[] = {"web.local", "api.local"};

        WubuEndpoint ep;
        T(wubu_endpoint_connect(mgr, mgr->networks[0].id, "c1", "web",
                                 maps, 2, aliases, 2, &ep) == 0,
          "connect with port maps and aliases");
        T(ep.port_map_count == 2, "port map count = 2");
        T(strcmp(ep.port_maps[0].container_port, "80/tcp") == 0, "port map 0");
        T(strcmp(ep.port_maps[1].host_port, "8443") == 0, "port map 1");
        T(ep.alias_count == 2, "alias count = 2");
        T(strcmp(ep.aliases[0], "web.local") == 0, "alias 0");
        T(strcmp(ep.aliases[1], "api.local") == 0, "alias 1");

        NNMGR_FREE(mgr);
    }

    /* -- Endpoint list -------------------------------------------- */
    printf("\n[Endpoint List]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_bridge(mgr, "br0", "10.0.0.0/24", "10.0.0.1", true);

        WubuEndpoint ep1, ep2;
        wubu_endpoint_connect(mgr, mgr->networks[0].id, "c1", "web",
                               NULL, 0, NULL, 0, &ep1);
        wubu_endpoint_connect(mgr, mgr->networks[0].id, "c2", "api",
                               NULL, 0, NULL, 0, &ep2);

        WubuEndpoint list[16];
        int n = wubu_endpoint_list(mgr, mgr->networks[0].id, list, 16);
        T(n == 2, "list returns 2 endpoints");

        NNMGR_FREE(mgr);
    }

    /* -- Port map add/remove -------------------------------------- */
    printf("\n[Port Map Add/Remove]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_bridge(mgr, "br0", "10.0.0.0/24", "10.0.0.1", true);

        WubuEndpoint ep;
        wubu_endpoint_connect(mgr, mgr->networks[0].id, "c1", "web",
                               NULL, 0, NULL, 0, &ep);

        WubuPortMap pm;
        memset(&pm, 0, sizeof(pm));
        strncpy(pm.container_port, "3306/tcp", sizeof(pm.container_port));
        strncpy(pm.host_port, "3306", sizeof(pm.host_port));
        strncpy(pm.protocol, "tcp", sizeof(pm.protocol));

        T(wubu_port_map_add(mgr, ep.id, &pm) == 0, "add port map");
        T(mgr->endpoints[0].port_map_count == 1, "port map count after add");

        T(wubu_port_map_remove(mgr, ep.id, "3306/tcp", "tcp") == 0, "remove port map");
        T(mgr->endpoints[0].port_map_count == 0, "port map count after remove");

        NNMGR_FREE(mgr);
    }

    /* -- Network list --------------------------------------------- */
    printf("\n[Network List]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_bridge(mgr, "br0", "10.0.0.0/24", "10.0.0.1", true);
        wubu_network_create_host(mgr, "host0");
        wubu_network_create_none(mgr, "null0");

        WubuNetworkProfile list[16];
        int n = wubu_network_list(mgr, list, 16);
        T(n == 3, "list returns 3 networks");
        T(strcmp(list[0].name, "br0") == 0, "first is br0");
        T(strcmp(list[1].name, "host0") == 0, "second is host0");
        T(strcmp(list[2].name, "null0") == 0, "third is null0");

        NNMGR_FREE(mgr);
    }

    /* -- Firewall rules ------------------------------------------- */
    printf("\n[Firewall Rules]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_bridge(mgr, "br0", "10.0.0.0/24", "10.0.0.1", true);

        const char *net_id = mgr->networks[0].id;
        T(wubu_network_firewall_add_rule(mgr, net_id, "-A INPUT -p tcp --dport 22 -j ACCEPT") == 0,
          "add firewall rule");
        T(mgr->networks[0].firewall_rule_count == 1, "rule count = 1");

        T(wubu_network_firewall_add_rule(mgr, net_id, "-A INPUT -p tcp --dport 80 -j ACCEPT") == 0,
          "add second rule");
        T(mgr->networks[0].firewall_rule_count == 2, "rule count = 2");

        char rules[128][512];
        int n = wubu_network_firewall_list(mgr, net_id, rules, 128);
        T(n == 2, "list firewall rules");
        T(strcmp(rules[0], "-A INPUT -p tcp --dport 22 -j ACCEPT") == 0, "rule 0 content");

        T(wubu_network_firewall_remove_rule(mgr, net_id, 0) == 0, "remove rule 0");
        T(mgr->networks[0].firewall_rule_count == 1, "rule count after remove");

        NNMGR_FREE(mgr);
    }

    /* -- QoS ------------------------------------------------------ */
    printf("\n[QoS]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_bridge(mgr, "br0", "10.0.0.0/24", "10.0.0.1", true);

        const char *net_id = mgr->networks[0].id;
        T(wubu_network_qos_set(mgr, net_id, 10000, 5000, 100, 50) == 0, "set network QoS");
        T(mgr->networks[0].enable_qos == true, "QoS enabled");
        T(mgr->networks[0].ingress_rate_kbps == 10000, "ingress rate");
        T(mgr->networks[0].egress_rate_kbps == 5000, "egress rate");

        NNMGR_FREE(mgr);
    }

    /* -- WireGuard peers ------------------------------------------ */
    printf("\n[WireGuard Peers]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_wireguard(mgr, "wg0", "10.5.0.0/24", "privkey", 51820);

        const char *net_id = mgr->networks[0].id;
        T(wubu_wg_peer_add(mgr, net_id, "peerpub1", "1.2.3.4:51820", "10.5.0.2/32", 25, "psk1") == 0,
          "add WG peer");
        T(mgr->networks[0].wg_peer_count == 1, "peer count = 1");

        char peers[32][5][128];
        int n = wubu_wg_peer_list(mgr, net_id, peers, 32);
        T(n == 1, "list WG peers");
        T(strcmp(peers[0][0], "peerpub1") == 0, "peer pubkey");

        T(wubu_wg_peer_remove(mgr, net_id, "peerpub1") == 0, "remove WG peer");
        T(mgr->networks[0].wg_peer_count == 0, "peer count after remove");

        NNMGR_FREE(mgr);
    }

    /* -- Tailscale ops -------------------------------------------- */
    printf("\n[Tailscale Ops]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_tailscale(mgr, "ts0", "tskey-abc");

        const char *net_id = mgr->networks[0].id;
        T(wubu_ts_up(mgr, net_id, "tskey-abc", "myhost") == 0, "ts up");
        T(wubu_ts_down(mgr, net_id) == 0, "ts down");

        char status[256];
        T(wubu_ts_status(mgr, net_id, status, sizeof(status)) == 0, "ts status");
        T(strstr(status, "myhost") != NULL, "status contains hostname");

        NNMGR_FREE(mgr);
    }

    /* -- Endpoint stats ------------------------------------------- */
    printf("\n[Endpoint Stats]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);
        wubu_network_create_bridge(mgr, "br0", "10.0.0.0/24", "10.0.0.1", true);

        WubuEndpoint ep;
        wubu_endpoint_connect(mgr, mgr->networks[0].id, "c1", "web",
                               NULL, 0, NULL, 0, &ep);

        uint64_t rx_bytes, tx_bytes, rx_pkts, tx_pkts, rx_errs, tx_errs;
        T(wubu_endpoint_stats(mgr, ep.id, &rx_bytes, &tx_bytes, &rx_pkts, &tx_pkts, &rx_errs, &tx_errs) == 0,
          "get endpoint stats");
        T(rx_bytes == 0 && tx_bytes == 0, "initial stats are zero");

        NNMGR_FREE(mgr);
    }

    /* -- CNI plugin load ------------------------------------------ */
    printf("\n[CNI Plugin Load]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        T(wubu_cni_plugin_load(mgr, "/opt/cni/bin/bridge", "{\"type\":\"bridge\"}") == 0,
          "load CNI plugin");
        T(mgr->cni_plugin_count == 1, "plugin count = 1");

        NNMGR_FREE(mgr);
    }

    /* -- String helpers ------------------------------------------- */
    printf("\n[String Helpers]\n");
    {
        T(strcmp(wubu_network_mode_str(WUBU_NET_BRIDGE), "bridge") == 0, "mode str: bridge");
        T(strcmp(wubu_network_mode_str(WUBU_NET_HOST), "host") == 0, "mode str: host");
        T(strcmp(wubu_network_mode_str(WUBU_NET_NONE), "none") == 0, "mode str: none");
        T(strcmp(wubu_network_mode_str(WUBU_NET_MACVLAN), "macvlan") == 0, "mode str: macvlan");
        T(strcmp(wubu_network_mode_str(WUBU_NET_IPVLAN), "ipvlan") == 0, "mode str: ipvlan");
        T(strcmp(wubu_network_mode_str(WUBU_NET_OVERLAY), "overlay") == 0, "mode str: overlay");
        T(strcmp(wubu_network_mode_str(WUBU_NET_WIREGUARD), "wireguard") == 0, "mode str: wireguard");
        T(strcmp(wubu_network_mode_str(WUBU_NET_TAILSCALE), "tailscale") == 0, "mode str: tailscale");
        T(strcmp(wubu_network_mode_str(9), "unknown") == 0, "mode str: unknown");

        T(wubu_network_mode_from_string("bridge") == WUBU_NET_BRIDGE, "from_string: bridge");
        T(wubu_network_mode_from_string("host") == WUBU_NET_HOST, "from_string: host");
        T(wubu_network_mode_from_string("none") == WUBU_NET_NONE, "from_string: none");
        T(wubu_network_mode_from_string("macvlan") == WUBU_NET_MACVLAN, "from_string: macvlan");
        T(wubu_network_mode_from_string("ipvlan") == WUBU_NET_IPVLAN, "from_string: ipvlan");
        T(wubu_network_mode_from_string("overlay") == WUBU_NET_OVERLAY, "from_string: overlay");
        T(wubu_network_mode_from_string("wireguard") == WUBU_NET_WIREGUARD, "from_string: wireguard");
        T(wubu_network_mode_from_string("tailscale") == WUBU_NET_TAILSCALE, "from_string: tailscale");
        T(wubu_network_mode_from_string(NULL) == WUBU_NET_BRIDGE, "from_string: NULL");
        T(wubu_network_mode_from_string("bogus") == WUBU_NET_BRIDGE, "from_string: bogus");

        T(strcmp(wubu_ipam_mode_str(WUBU_IPAM_DHCP), "dhcp") == 0, "ipam str: dhcp");
        T(strcmp(wubu_ipam_mode_str(WUBU_IPAM_STATIC), "static") == 0, "ipam str: static");
        T(strcmp(wubu_ipam_mode_str(WUBU_IPAM_POOL), "pool") == 0, "ipam str: pool");
        T(strcmp(wubu_ipam_mode_str(WUBU_IPAM_HOST_LOCAL), "host-local") == 0, "ipam str: host-local");
        T(strcmp(wubu_ipam_mode_str(9), "unknown") == 0, "ipam str: unknown");
    }

    /* -- Event callback ------------------------------------------- */
    printf("\n[Event Callback]\n");
    {
        WubuNetworkManager *mgr = nmgr();
        wubu_network_manager_init("/tmp/wubu-net-test", mgr);

        static int event_count;
        event_count = 0;
        mgr->network_event_cb = NULL; /* set below via typedef */

        /* We can't easily test the callback without a function pointer type,
           but we can verify it doesn't crash when NULL */
        T(wubu_network_create_bridge(mgr, "br0", "10.0.0.0/24", "10.0.0.1", true) == 0,
          "create with NULL callback doesn't crash");

        NNMGR_FREE(mgr);
    }

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
