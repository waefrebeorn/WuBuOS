/*
 * wubu_network.h  --  WuBuOS Container Network Profiles
 *
 * Phase 7: Network profiles with:
 *   - Predefined network modes (bridge, host, none, macvlan, ipvlan)
 *   - Custom network profiles with DNS, routing, firewall rules
 *   - Port mapping and exposure
 *   - Network namespaces isolation
 *   - CNI (Container Network Interface) plugin support
 *   - WireGuard / Tailscale integration for secure overlay networks
 *   - Bandwidth shaping (tc) and QoS
 *   - Service discovery (mDNS, DNS-SD)
 *   - Load balancing (round-robin, least-connections)
 */

#ifndef WUBU_NETWORK_H
#define WUBU_NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* -- Limits ------------------------------------------------------- */

#define WUBU_MAX_NETWORKS        64
#define WUBU_MAX_ENDPOINTS       256
#define WUBU_MAX_PORT_MAPS       64
#define WUBU_MAX_DNS_SERVERS     8
#define WUBU_MAX_DNS_SEARCH      8
#define WUBU_MAX_ROUTES          32
#define WUBU_MAX_FIREWALL_RULES  128
#define WUBU_MAX_ALIASES         32
#define WUBU_MAX_PLUGINS         16
#define WUBU_IFACE_NAME_LEN      16
#define WUBU_NETWORK_ID_LEN      64
#define WUBU_MAX_PATH            4096

/* -- Network Modes ------------------------------------------------ */

typedef enum {
    WUBU_NET_BRIDGE       = 0,    /* Default: Linux bridge (docker0 style) */
    WUBU_NET_HOST         = 1,    /* Host networking (no isolation) */
    WUBU_NET_NONE         = 2,    /* No networking (loopback only) */
    WUBU_NET_MACVLAN      = 3,    /* MACVLAN (direct L2 access) */
    WUBU_NET_IPVLAN       = 4,    /* IPVLAN (L3, shared MAC) */
    WUBU_NET_OVERLAY      = 5,    /* VXLAN overlay (multi-host) */
    WUBU_NET_CUSTOM       = 6,    /* Custom CNI plugin */
    WUBU_NET_WIREGUARD    = 7,    /* WireGuard peer-to-peer */
    WUBU_NET_TAILSCALE    = 8,    /* Tailscale mesh */
} WubuNetworkMode;

/* -- IPAM (IP Address Management) --------------------------------- */

typedef enum {
    WUBU_IPAM_DHCP        = 0,    /* DHCP client */
    WUBU_IPAM_STATIC      = 1,    /* Static allocation */
    WUBU_IPAM_POOL        = 2,    /* Pool allocation (first-fit) */
    WUBU_IPAM_HOST_LOCAL  = 3,    /* Host-local plugin */
} WubuIpamMode;

/* -- DNS Record (for embedded DNS) -------------------------------- */

typedef struct {
    char             name[128];
    char             type[16];             /* A, AAAA, CNAME, SRV, TXT */
    char             value[256];
    int              ttl;
    int              priority;             /* For SRV */
    int              weight;               /* For SRV */
    int              port;                 /* For SRV */
} WubuDnsRecord;

/* -- Network Profile ---------------------------------------------- */

typedef struct {
    char             id[WUBU_NETWORK_ID_LEN];
    char             name[64];
    char             description[256];
    
    WubuNetworkMode  mode;
    WubuIpamMode     ipam_mode;
    
    /* Bridge / Overlay config */
    char             bridge_name[WUBU_IFACE_NAME_LEN];    /* e.g., "wubu0" */
    char             subnet[64];                          /* CIDR: 10.0.0.0/24 */
    char             gateway[64];                         /* Gateway IP */
    char             ip_range[64];                        /* Allocation range */
    char             aux_addresses[WUBU_MAX_ALIASES][2][64]; /* Reserved IPs */
    int              aux_address_count;
    
    /* DNS */
    char             dns_servers[WUBU_MAX_DNS_SERVERS][64];
    int              dns_server_count;
    char             dns_search[WUBU_MAX_DNS_SEARCH][64];
    int              dns_search_count;
    char             dns_options[256];
    bool             enable_dns;                          /* Run embedded DNS server */
    WubuDnsRecord    dns_records[64];                     /* Embedded DNS records */
    int              dns_record_count;
    
    /* IPVLAN / MACVLAN */
    char             parent_interface[WUBU_IFACE_NAME_LEN];
    int              vlan_id;
    
    /* Overlay / VXLAN */
    char             vxlan_vni[32];
    int              vxlan_port;
    char             vxlan_remote[WUBU_MAX_ALIASES][64];
    int              vxlan_remote_count;
    bool             vxlan_encrypt;                       /* WireGuard encryption */
    
    /* WireGuard */
    char             wg_private_key[128];
    char             wg_public_key[128];
    char             wg_peers[WUBU_MAX_ALIASES][5][128]; /* endpoint, pubkey, allowed_ips, keepalive, psk */
    int              wg_peer_count;
    int              wg_listen_port;
    
    /* Tailscale */
    char             ts_auth_key[256];
    char             ts_hostname[128];
    char             ts_tags[WUBU_MAX_ALIASES][64];
    int              ts_tag_count;
    
    /* CNI Plugin */
    char             cni_plugin[128];
    char             cni_config_json[4096];
    char             cni_bin_dir[WUBU_MAX_PATH];
    char             cni_conf_dir[WUBU_MAX_PATH];
    
    /* Firewall / Security */
    char             firewall_rules[WUBU_MAX_FIREWALL_RULES][512];
    int              firewall_rule_count;
    bool             enable_icmp;
    bool             enable_ipv6;
    char             default_policy_in[16];   /* ACCEPT, DROP, REJECT */
    char             default_policy_out[16];
    char             default_policy_fwd[16];
    
    /* QoS / Traffic Shaping */
    bool             enable_qos;
    int              ingress_rate_kbps;       /* Ingress limit (0 = unlimited) */
    int              egress_rate_kbps;        /* Egress limit */
    int              ingress_burst_kb;        /* Burst */
    int              egress_burst_kb;
    int              priority;                /* tc priority */
    
    /* Service Discovery */
    bool             enable_mdns;
    bool             enable_dns_sd;
    char             service_domain[64];
    
    /* Labels / Metadata */
    char             labels[32][2][128];
    int              label_count;
    
    /* State */
    bool             created;
    bool             enabled;
    uint64_t         created_at;
    uint64_t         endpoint_count;
} WubuNetworkProfile;

/* -- Endpoint (Container network interface) ----------------------- */

typedef struct {
    char             id[WUBU_NETWORK_ID_LEN];
    char             network_id[WUBU_NETWORK_ID_LEN];
    char             container_id[WUBU_NETWORK_ID_LEN];
    char             container_name[128];
    
    /* Interface */
    char             iface_name[WUBU_IFACE_NAME_LEN];    /* e.g., "eth0" */
    char             iface_host_name[WUBU_IFACE_NAME_LEN]; /* veth host side */
    char             mac_address[32];
    char             ipv4_address[64];           /* CIDR */
    char             ipv6_address[64];           /* CIDR */
    char             gateway[64];
    
    /* Port mappings */
    struct {
        char             container_port[32];     /* 80/tcp */
        char             host_port[32];          /* 8080 */
        char             host_ip[64];            /* 0.0.0.0 or specific */
        char             protocol[8];            /* tcp, udp, sctp */
        char             range[32];              /* 80-90/tcp */
    } port_maps[WUBU_MAX_PORT_MAPS];
    int              port_map_count;
    
    /* DNS aliases */
    char             aliases[WUBU_MAX_ALIASES][64];
    int              alias_count;
    
    /* Routes */
    char             routes[WUBU_MAX_ROUTES][128];
    int              route_count;
    
    /* State */
    bool             connected;
    uint64_t         connected_at;
    uint64_t         bytes_rx;
    uint64_t         bytes_tx;
    uint64_t         packets_rx;
    uint64_t         packets_tx;
    uint64_t         errors_rx;
    uint64_t         errors_tx;
    uint64_t         drops_rx;
    uint64_t         drops_tx;
} WubuEndpoint;

/* -- Port Mapping (standalone) ------------------------------------ */

typedef struct {
    char             container_port[32];     /* 80/tcp */
    char             host_port[32];          /* 8080 */
    char             host_ip[64];            /* 0.0.0.0 */
    char             protocol[8];            /* tcp, udp */
    char             range[32];              /* 80-90/tcp */
    char             description[128];
} WubuPortMap;

/* -- Network Manager ---------------------------------------------- */

typedef struct {
    char             state_dir[WUBU_MAX_PATH];
    char             cni_bin_dir[WUBU_MAX_PATH];
    char             cni_conf_dir[WUBU_MAX_PATH];
    
    WubuNetworkProfile networks[WUBU_MAX_NETWORKS];
    int              network_count;
    
    WubuEndpoint     endpoints[WUBU_MAX_ENDPOINTS];
    int              endpoint_count;
    
    /* Global DNS */
    char             global_dns_servers[WUBU_MAX_DNS_SERVERS][64];
    int              global_dns_count;
    bool             global_dns_enabled;
    
    /* Default network */
    char             default_network[WUBU_NETWORK_ID_LEN];
    
    /* WireGuard global */
    bool             wg_enabled;
    char             wg_interface[WUBU_IFACE_NAME_LEN];
    
    /* CNI */
    char             cni_plugin_dirs[WUBU_MAX_PLUGINS][WUBU_MAX_PATH];
    int              cni_plugin_count;
    
    /* Callbacks */
    void (*network_event_cb)(const char *event, const char *network_id, const char *endpoint_id, void *user_data);
    void             *event_user_data;
} WubuNetworkManager;

/* -- Public API --------------------------------------------------- */

/* Manager lifecycle */
int  wubu_network_manager_init(const char *state_dir, WubuNetworkManager *mgr);
int  wubu_network_manager_shutdown(WubuNetworkManager *mgr);

/* Network Profile operations */
int  wubu_network_create(WubuNetworkManager *mgr, const WubuNetworkProfile *profile);
int  wubu_network_delete(WubuNetworkManager *mgr, const char *network_id, bool force);
int  wubu_network_inspect(WubuNetworkManager *mgr, const char *network_id, WubuNetworkProfile *out_profile);
int  wubu_network_list(WubuNetworkManager *mgr, WubuNetworkProfile *out_profiles, int max);
int  wubu_network_enable(WubuNetworkManager *mgr, const char *network_id);
int  wubu_network_disable(WubuNetworkManager *mgr, const char *network_id);

/* Predefined network presets */
int  wubu_network_create_bridge(WubuNetworkManager *mgr, const char *name, const char *subnet,
                                const char *gateway, bool enable_dns);
int  wubu_network_create_host(WubuNetworkManager *mgr, const char *name);
int  wubu_network_create_none(WubuNetworkManager *mgr, const char *name);
int  wubu_network_create_macvlan(WubuNetworkManager *mgr, const char *name, const char *parent_iface,
                                 const char *subnet, const char *gateway, int vlan_id);
int  wubu_network_create_ipvlan(WubuNetworkManager *mgr, const char *name, const char *parent_iface,
                                const char *subnet, const char *gateway);
int  wubu_network_create_overlay(WubuNetworkManager *mgr, const char *name, const char *subnet,
                                 const char *gateway, const char *vni, int port, bool encrypt);
int  wubu_network_create_wireguard(WubuNetworkManager *mgr, const char *name, const char *subnet,
                                   const char *private_key, int listen_port);
int  wubu_network_create_tailscale(WubuNetworkManager *mgr, const char *name, const char *auth_key);

/* Endpoint (Container connection) operations */
int  wubu_endpoint_connect(WubuNetworkManager *mgr, const char *network_id,
                           const char *container_id, const char *container_name,
                           const WubuPortMap *port_maps, int port_map_count,
                           const char *aliases[], int alias_count,
                           WubuEndpoint *out_endpoint);
int  wubu_endpoint_disconnect(WubuNetworkManager *mgr, const char *endpoint_id);
int  wubu_endpoint_inspect(WubuNetworkManager *mgr, const char *endpoint_id, WubuEndpoint *out_endpoint);
int  wubu_endpoint_list(WubuNetworkManager *mgr, const char *network_id,
                        WubuEndpoint *out_endpoints, int max);

/* Port mapping */
int  wubu_port_map_add(WubuNetworkManager *mgr, const char *endpoint_id,
                       const WubuPortMap *port_map);
int  wubu_port_map_remove(WubuNetworkManager *mgr, const char *endpoint_id,
                          const char *container_port, const char *protocol);

/* DNS / Service Discovery */
int  wubu_network_dns_add_record(WubuNetworkManager *mgr, const char *network_id,
                                 const WubuDnsRecord *record);
int  wubu_network_dns_remove_record(WubuNetworkManager *mgr, const char *network_id,
                                    const char *name, const char *rtype);
int  wubu_network_dns_query(WubuNetworkManager *mgr, const char *network_id,
                            const char *name, const char *rtype,
                            WubuDnsRecord *out_records, int max);

/* Traffic Stats */
int  wubu_endpoint_stats(WubuNetworkManager *mgr, const char *endpoint_id,
                         uint64_t *rx_bytes, uint64_t *tx_bytes,
                         uint64_t *rx_packets, uint64_t *tx_packets,
                         uint64_t *rx_errors, uint64_t *tx_errors);

/* QoS / Traffic Control */
int  wubu_network_qos_set(WubuNetworkManager *mgr, const char *network_id,
                          int ingress_kbps, int egress_kbps,
                          int ingress_burst_kb, int egress_burst_kb);
int  wubu_endpoint_qos_set(WubuNetworkManager *mgr, const char *endpoint_id,
                           int ingress_kbps, int egress_kbps);

/* Firewall */
int  wubu_network_firewall_add_rule(WubuNetworkManager *mgr, const char *network_id,
                                    const char *rule);  /* iptables/nftables compatible */
int  wubu_network_firewall_remove_rule(WubuNetworkManager *mgr, const char *network_id,
                                       int rule_index);
int  wubu_network_firewall_list(WubuNetworkManager *mgr, const char *network_id,
                                char rules[][512], int max);

/* CNI Plugin support */
int  wubu_cni_plugin_load(WubuNetworkManager *mgr, const char *plugin_path, const char *config_json);
int  wubu_cni_plugin_exec(WubuNetworkManager *mgr, const char *plugin_name,
                          const char *command, const char *stdin_data,
                          char *stdout_out, size_t stdout_size,
                          char *stderr_out, size_t stderr_size);

/* WireGuard peer management */
int  wubu_wg_peer_add(WubuNetworkManager *mgr, const char *network_id,
                      const char *peer_pubkey, const char *endpoint,
                      const char *allowed_ips, int keepalive, const char *psk);
int  wubu_wg_peer_remove(WubuNetworkManager *mgr, const char *network_id,
                         const char *peer_pubkey);
int  wubu_wg_peer_list(WubuNetworkManager *mgr, const char *network_id,
                       char peers[][5][128], int max);

/* Tailscale */
int  wubu_ts_up(WubuNetworkManager *mgr, const char *network_id,
                const char *auth_key, const char *hostname);
int  wubu_ts_down(WubuNetworkManager *mgr, const char *network_id);
int  wubu_ts_status(WubuNetworkManager *mgr, const char *network_id,
                    char *status_out, size_t status_size);

/* Helpers */
const char *wubu_network_mode_str(WubuNetworkMode mode);
const char *wubu_ipam_mode_str(WubuIpamMode mode);
WubuNetworkMode wubu_network_mode_from_string(const char *str);

/* Cleanup */
void wubu_network_manager_free(WubuNetworkManager *mgr);

#endif /* WUBU_NETWORK_H */
