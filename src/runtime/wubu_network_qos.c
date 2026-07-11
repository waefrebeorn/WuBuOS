/* wubu_network_qos.c -- WuBuOS network: QoS / traffic shaping.
 * Extracted from wubu_network.c (separable leaf). Self-contained: tc-based QoS
 * for networks + endpoints. Uses find_network (wubu_network_internal.h) + net_cmd
 * (wubu_netlink.h), already shared. C11, minimal includes.
 */
#include "wubu_network.h"
#include "wubu_network_internal.h"
#include "wubu_netlink.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
