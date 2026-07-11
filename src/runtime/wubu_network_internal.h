/* wubu_network_internal.h -- Internal helpers shared by wubu_network sub-modules.
 * Public API + types in wubu_network.h. The port-map/DNS subsystem lives in
 * wubu_network_svc.c; the shared find_endpoint/find_network resolvers are
 * declared here so all submodules link the SAME implementation (no double-coding).
 */

#ifndef WUBU_NETWORK_INTERNAL_H
#define WUBU_NETWORK_INTERNAL_H

#include "wubu_network.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* -- Shared internal resolvers (wubu_network.c) ------------------ */
WubuEndpoint *find_endpoint(WubuNetworkManager *mgr, const char *id);
WubuNetworkProfile *find_network(WubuNetworkManager *mgr, const char *id);
void emit_event(WubuNetworkManager *mgr, const char *event,
               const char *network_id, const char *endpoint_id);

#endif /* WUBU_NETWORK_INTERNAL_H */
