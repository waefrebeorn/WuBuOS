/* wubu_network_fw.c -- Network firewall rule subsystem (self-contained).
 *
 * wubu_network_firewall_add_rule/remove_rule/list. Uses find_network
 * (wubu_network_internal.h) + WubuNetworkProfile/Manager (wubu_network.h).
 * Minimal includes.
 */

#include "wubu_network.h"
#include "wubu_network_internal.h"
#include <string.h>

int wubu_network_firewall_add_rule(WubuNetworkManager *mgr, const char *network_id, const char *rule) {
    if (!mgr || !network_id || !rule) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;
    if (net->firewall_rule_count >= WUBU_MAX_FIREWALL_RULES) return -1;
    strncpy(net->firewall_rules[net->firewall_rule_count], rule, 511);
    net->firewall_rule_count++;
    return 0;
}

int wubu_network_firewall_remove_rule(WubuNetworkManager *mgr, const char *network_id, int rule_index) {
    if (!mgr || !network_id) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;
    if (rule_index < 0 || rule_index >= net->firewall_rule_count) return -1;
    memmove(&net->firewall_rules[rule_index], &net->firewall_rules[rule_index + 1],
            (net->firewall_rule_count - rule_index - 1) * 512);
    net->firewall_rule_count--;
    return 0;
}

int wubu_network_firewall_list(WubuNetworkManager *mgr, const char *network_id,
                               char rules[][512], int max) {
    if (!mgr || !network_id || !rules || max <= 0) return 0;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return 0;
    int count = (net->firewall_rule_count < max) ? net->firewall_rule_count : max;
    memcpy(rules, net->firewall_rules, count * 512);
    return count;
}
