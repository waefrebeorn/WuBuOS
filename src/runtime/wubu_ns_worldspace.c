/*
 * wubu_ns_worldspace.c -- the /n/worldspace control subtree.
 *
 * The ~WuBu~ space engine (the WWW submodule, "the space itself") is a
 * self-contained C11 engine with its own build. The body does not embed it;
 * it reaches it. This subtree is that reach: it publishes the state a space
 * client or a portal needs in order to connect to this body --
 *
 *   /n/worldspace/state        the AGI world snapshot (what the body perceives)
 *   /n/worldspace/agent        the federation identity + sponsor + seal
 *   /n/worldspace/portal       the space instance this body connects to
 *   /n/worldspace/transport    the transport the body speaks (SDR relay)
 *   /n/worldspace/capabilities what the body can do in the space
 *
 * Nothing here invents a presence in the space: with no portal configured and
 * no agent card, the files say so.
 *
 * C11, minimal includes, self-contained.
 */
#include "wubu_ns_worldspace.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_agent.h"
#include "../kernel/wubu_world.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WS_PORTAL_FILE "/.config/wubu/worldspace.conf"

/* Read the configured portal line ("portal=<endpoint>") from the home dir.
 * Returns 0 when a portal is configured, -1 when it is not. */
static int ws_read_portal(char *out, size_t cap)
{
    const char *home = getenv("HOME");
    if (!home) return -1;
    char path[512];
    snprintf(path, sizeof(path), "%s%s", home, WS_PORTAL_FILE);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[256];
    int rc = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "portal=", 7) == 0) {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(out, cap, "%s", line + 7);
            rc = 0;
            break;
        }
    }
    fclose(f);
    return rc;
}

int wubu_ns_publish_worldspace(void)
{
    if (ns_mkdir("worldspace") != 0) return -1;

    char sub[128], buf[1024];

    /* the world the body perceives (the same snapshot the tray shows) */
    wubu_world_sample();
    if (wubu_world_state_str(buf, sizeof(buf)) == 0) {
        snprintf(sub, sizeof(sub), "worldspace/state");
        ns_write(sub, buf);
    }

    /* the federation identity: who is acting, and for whom */
    const WubuAgentCard *a = wubu_agent_card();
    snprintf(sub, sizeof(sub), "worldspace/agent");
    if (a) {
        char seal[80];
        if (wubu_agent_seal(seal, sizeof(seal)) != 0) seal[0] = '\0';
        snprintf(buf, sizeof(buf),
                 "aid: %s\nclass: %s\nsponsor: %s\nmodel: %s\nendpoint: %s\nseal: %s\n",
                 a->aid, a->class_code, a->sponsor, a->model, a->endpoint, seal);
    } else {
        snprintf(buf, sizeof(buf), "not enroled\n");
    }
    ns_write(sub, buf);

    /* the space instance this body connects to */
    char portal[256];
    snprintf(sub, sizeof(sub), "worldspace/portal");
    if (ws_read_portal(portal, sizeof(portal)) == 0)
        snprintf(buf, sizeof(buf), "%s\n", portal);
    else
        snprintf(buf, sizeof(buf),
                 "unconfigured\n"
                 "# set one with: echo 'portal=a2a://host:port' > ~/.config/wubu/worldspace.conf\n");
    ns_write(sub, buf);

    /* the transport: SDR relay (fail-closed tickets + routes) and the swarm
     * aggregation ledger. Both are compiled into the body. */
    snprintf(sub, sizeof(sub), "worldspace/transport");
    ns_write(sub,
             "sdr: fail-closed relay (ticket + route, DENY on first failure)\n"
             "swarm: peer-to-peer aggregation ledger (networking/aggregation)\n");

    /* what the body can do in the space */
    snprintf(sub, sizeof(sub), "worldspace/capabilities");
    ns_write(sub,
             "world-sample\n"
             "agent-identity\n"
             "sdr-relay\n"
             "swarm-aggregation\n");

    return 0;
}
