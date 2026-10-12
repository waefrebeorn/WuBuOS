/*
 * wubu_ns_world.h -- the /n/world control subtree.
 */
#ifndef WUBU_NS_WORLD_H
#define WUBU_NS_WORLD_H

/* publish the /n/world tree. */
int wubu_ns_publish_world(void);

/* refresh /n/world/state from the real drivers. */
int wubu_ns_world_refresh(void);

/* refresh /n/world/hw — the inventory. */
int wubu_ns_world_refresh_hw(void);

/* Push the worldsim state string (WORLDSIM P0 #606). The worldsim runtime
 * calls this with ws_sim_state_str() output; /n/world/sim reflects it. */
void wubu_ns_world_set_sim(const char *state_str);

#endif
