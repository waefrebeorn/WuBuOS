/* wubu_network_wg.c -- WuBuOS network: WireGuard peer management backend.
 * Extracted from wubu_network.c (separable leaf). Self-contained: uses the
 * shared resolvers find_network (wubu_network_internal.h) + net_cmd (wubu_netlink.h).
 * C11, minimal includes.
 */
#include "wubu_network.h"
#include "wubu_network_internal.h"
#include "wubu_netlink.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int wubu_wg_peer_add(WubuNetworkManager *mgr, const char *network_id,
                     const char *peer_pubkey, const char *endpoint,
                     const char *allowed_ips, int keepalive, const char *psk) {
    if (!mgr || !network_id || !peer_pubkey) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;
    if (net->wg_peer_count >= WUBU_MAX_ALIASES) return -1;

    int idx = net->wg_peer_count;
    strncpy(net->wg_peers[idx][0], peer_pubkey, 127);
    if (endpoint) strncpy(net->wg_peers[idx][1], endpoint, 127);
    if (allowed_ips) strncpy(net->wg_peers[idx][2], allowed_ips, 127);
    snprintf(net->wg_peers[idx][3], 128, "%d", keepalive);
    if (psk) strncpy(net->wg_peers[idx][4], psk, 127);
    net->wg_peer_count++;

    /* Apply via wg set (best-effort; non-fatal if wg unavailable) */
    char wg_cmd[1024];
    snprintf(wg_cmd, sizeof(wg_cmd), "wg set %s peer %s", net->name, peer_pubkey);
    if (endpoint && endpoint[0]) {
        strncat(wg_cmd, " endpoint ", sizeof(wg_cmd) - strlen(wg_cmd) - 1);
        strncat(wg_cmd, endpoint, sizeof(wg_cmd) - strlen(wg_cmd) - 1);
    }
    if (allowed_ips && allowed_ips[0]) {
        strncat(wg_cmd, " allowed-ips ", sizeof(wg_cmd) - strlen(wg_cmd) - 1);
        strncat(wg_cmd, allowed_ips, sizeof(wg_cmd) - strlen(wg_cmd) - 1);
    }
    if (keepalive > 0) {
        char ka[64];
        snprintf(ka, sizeof(ka), " persistent-keepalive %d", keepalive);
        strncat(wg_cmd, ka, sizeof(wg_cmd) - strlen(wg_cmd) - 1);
    }
    if (psk && psk[0]) {
        strncat(wg_cmd, " psk ", sizeof(wg_cmd) - strlen(wg_cmd) - 1);
        strncat(wg_cmd, psk, sizeof(wg_cmd) - strlen(wg_cmd) - 1);
    }
    /* Redirect stderr to /dev/null since wg may not be installed */
    strncat(wg_cmd, " 2>/dev/null", sizeof(wg_cmd) - strlen(wg_cmd) - 1);
    net_cmd(wg_cmd);
    return 0;
}

int wubu_wg_peer_remove(WubuNetworkManager *mgr, const char *network_id, const char *peer_pubkey) {
    if (!mgr || !network_id || !peer_pubkey) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;
    for (int i = 0; i < net->wg_peer_count; i++) {
        if (strcmp(net->wg_peers[i][0], peer_pubkey) == 0) {
            memmove(&net->wg_peers[i], &net->wg_peers[i + 1],
                    (net->wg_peer_count - i - 1) * 5 * 128);
            net->wg_peer_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_wg_peer_list(WubuNetworkManager *mgr, const char *network_id,
                      char peers[][5][128], int max) {
    if (!mgr || !network_id || !peers || max <= 0) return 0;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return 0;
    int count = (net->wg_peer_count < max) ? net->wg_peer_count : max;
    memcpy(peers, net->wg_peers, count * 5 * 128);
    return count;
}
