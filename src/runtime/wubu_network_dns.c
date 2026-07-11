/* wubu_network_dns.c -- WuBuOS network: DNS record management.
 * Extracted from wubu_network.c (separable leaf). Self-contained: add/remove/query
 * DNS records for a network. Uses find_network (wubu_network_internal.h) + net_cmd
 * (wubu_netlink.h), already shared. C11, minimal includes.
 */
#include "wubu_network.h"
#include "wubu_network_internal.h"
#include "wubu_netlink.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
