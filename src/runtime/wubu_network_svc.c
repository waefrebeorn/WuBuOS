/* wubu_network_svc.c -- Network port-mapping + DNS record subsystem.
 *
 * Self-contained: port map add/remove and DNS record add. Uses the shared
 * find_endpoint/find_network resolvers (declared in wubu_network_internal.h)
 * and the WubuNetworkManager/WubuEndpoint/WubuDnsRecord types (wubu_network.h).
 * Minimal includes.
 */

#include "wubu_network_internal.h"

int wubu_port_map_add(WubuNetworkManager *mgr, const char *endpoint_id, const WubuPortMap *port_map) {
    if (!mgr || !endpoint_id || !port_map) return -1;
    WubuEndpoint *ep = find_endpoint(mgr, endpoint_id);
    if (!ep) return -1;
    if (ep->port_map_count >= WUBU_MAX_PORT_MAPS) return -1;
    memcpy(&ep->port_maps[ep->port_map_count], port_map, sizeof(*port_map));
    ep->port_map_count++;
    return 0;
}

int wubu_port_map_remove(WubuNetworkManager *mgr, const char *endpoint_id,
                         const char *container_port, const char *protocol) {
    if (!mgr || !endpoint_id || !container_port) return -1;
    WubuEndpoint *ep = find_endpoint(mgr, endpoint_id);
    if (!ep) return -1;
    for (int i = 0; i < ep->port_map_count; i++) {
        if (strcmp(ep->port_maps[i].container_port, container_port) == 0 &&
            (protocol == NULL || strcmp(ep->port_maps[i].protocol, protocol) == 0)) {
            memmove(&ep->port_maps[i], &ep->port_maps[i + 1],
                    (ep->port_map_count - i - 1) * sizeof(WubuPortMap));
            ep->port_map_count--;
            return 0;
        }
    }
    return -1; /* not found */
}

int wubu_network_dns_add_record(WubuNetworkManager *mgr, const char *network_id, const WubuDnsRecord *record) {
    if (!mgr || !network_id || !record) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;
    if (net->dns_record_count >= 64) return -1;
    memcpy(&net->dns_records[net->dns_record_count], record, sizeof(WubuDnsRecord));
    net->dns_record_count++;
    return 0;
}
