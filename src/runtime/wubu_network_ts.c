/* wubu_network_ts.c -- WuBuOS network: Tailscale up/down/status backend.
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

int wubu_ts_up(WubuNetworkManager *mgr, const char *network_id,
               const char *auth_key, const char *hostname) {
    if (!mgr || !network_id) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;
    if (auth_key) strncpy(net->ts_auth_key, auth_key, sizeof(net->ts_auth_key) - 1);
    if (hostname) strncpy(net->ts_hostname, hostname, sizeof(net->ts_hostname) - 1);

    /* Invoke tailscale up (best-effort; binary may not exist) */
    {
        char ts_cmd[1024];
        snprintf(ts_cmd, sizeof(ts_cmd), "tailscale up --authkey=%s", net->ts_auth_key);
        if (hostname && hostname[0]) {
            strncat(ts_cmd, " --hostname=", sizeof(ts_cmd) - strlen(ts_cmd) - 1);
            strncat(ts_cmd, hostname, sizeof(ts_cmd) - strlen(ts_cmd) - 1);
        }
        strncat(ts_cmd, " --accept-routes 2>/dev/null", sizeof(ts_cmd) - strlen(ts_cmd) - 1);
        net_cmd(ts_cmd);
    }
    return 0;
}

int wubu_ts_down(WubuNetworkManager *mgr, const char *network_id) {
    if (!mgr || !network_id) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;

    /* Invoke tailscale down (best-effort) */
    net_cmd("tailscale down 2>/dev/null");
    (void)net;
    return 0;
}

int wubu_ts_status(WubuNetworkManager *mgr, const char *network_id,
                   char *status_out, size_t status_size) {
    if (!mgr || !network_id || !status_out || status_size == 0) return -1;
    WubuNetworkProfile *net = find_network(mgr, network_id);
    if (!net) return -1;

    /* Attempt to get real tailscale status, fall back to config summary */
    {
        char ts_cmd[512];
        snprintf(ts_cmd, sizeof(ts_cmd), "tailscale status --json 2>/dev/null");
        FILE *fp = popen(ts_cmd, "r");
        if (fp) {
            size_t n = fread(status_out, 1, status_size - 1, fp);
            if (n > 0) {
                status_out[n] = '\0';
                pclose(fp);
                return 0;
            }
            pclose(fp);
        }
    }
    /* Fallback: report from stored config */
    snprintf(status_out, status_size, "Tailscale: network=%s, hostname=%s, status=config-only",
             net->name, net->ts_hostname);
    return 0;
}

/* -- Helpers (already implemented in stub, kept as-is) ------------- */

const char *wubu_network_mode_str(WubuNetworkMode mode) {
    static const char *names[] = {"bridge", "host", "none", "macvlan", "ipvlan", "overlay", "custom", "wireguard", "tailscale"};
    if (mode >= 0 && mode < 9) return names[mode];
    return "unknown";
}

const char *wubu_ipam_mode_str(WubuIpamMode mode) {
    static const char *names[] = {"dhcp", "static", "pool", "host-local"};
    if (mode >= 0 && mode < 4) return names[mode];
    return "unknown";
}

WubuNetworkMode wubu_network_mode_from_string(const char *str) {
    if (!str) return WUBU_NET_BRIDGE;
    if (strcmp(str, "bridge") == 0) return WUBU_NET_BRIDGE;
    if (strcmp(str, "host") == 0) return WUBU_NET_HOST;
    if (strcmp(str, "none") == 0) return WUBU_NET_NONE;
    if (strcmp(str, "macvlan") == 0) return WUBU_NET_MACVLAN;
    if (strcmp(str, "ipvlan") == 0) return WUBU_NET_IPVLAN;
    if (strcmp(str, "overlay") == 0) return WUBU_NET_OVERLAY;
    if (strcmp(str, "custom") == 0) return WUBU_NET_CUSTOM;
    if (strcmp(str, "wireguard") == 0) return WUBU_NET_WIREGUARD;
    if (strcmp(str, "tailscale") == 0) return WUBU_NET_TAILSCALE;
    return WUBU_NET_BRIDGE;
}




