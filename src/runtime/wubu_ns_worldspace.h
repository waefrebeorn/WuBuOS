/*
 * wubu_ns_worldspace.h -- the /n/worldspace control subtree.
 *
 *   /n/worldspace/state        the AGI world snapshot (what the body perceives)
 *   /n/worldspace/agent        the federation identity + sponsor + seal
 *   /n/worldspace/portal       the space instance this body connects to
 *   /n/worldspace/transport    the transport the body speaks (SDR relay)
 *   /n/worldspace/capabilities what the body can do in the space
 *
 * With no portal configured and no agent card the files say so; nothing is
 * invented. Called once at boot, after the namespace root exists.
 *
 * C11, minimal includes.
 */
#ifndef WUBU_NS_WORLDSPACE_H
#define WUBU_NS_WORLDSPACE_H

int wubu_ns_publish_worldspace(void);

#endif /* WUBU_NS_WORLDSPACE_H */
