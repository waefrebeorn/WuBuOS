/*
 * wubu_ns_world.c -- the /n/world control subtree (the AGI's eyes).
 *
 *   /n/world/state  -> the one-line world snapshot (read)
 *   /n/world/hw     -> the hardware inventory
 *
 * The AGI (holyd, the colony) reads /n/world/state to perceive what
 * the OS is doing — the training space. The state assembles from the
 * REAL driver state via wubu_world_sample().
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_world.h"

#include <stdio.h>

int wubu_ns_publish_world(void)
{
    char sub[128];
    if (ns_mkdir("world") != 0) return -1;
    snprintf(sub, sizeof(sub), "world/state");
    if (ns_write(sub, "no state\n") != 0) return -1;
    snprintf(sub, sizeof(sub), "world/hw");
    if (ns_write(sub, "no hw\n") != 0) return -1;
    return 0;
}

/* refresh /n/world/state from the real drivers */
int wubu_ns_world_refresh(void)
{
    char sub[128], buf[1024];
    wubu_world_sample();
    snprintf(sub, sizeof(sub), "world/state");
    if (wubu_world_state_str(buf, sizeof(buf)) != 0) return -1;
    return ns_write(sub, buf);
}

/* refresh /n/world/hw — the inventory */
int wubu_ns_world_refresh_hw(void)
{
    char sub[128], buf[1024];
    const wubu_world_t *w = wubu_world_snapshot();
    snprintf(sub, sizeof(sub), "world/hw");
    snprintf(buf, sizeof(buf),
             "storage: %s%s%s%s\nnetwork: %s%s\n"
             "display: %s %ux%u %uMB\npower: %s battery\n",
             w->has_nvme ? "nvme " : "", w->has_sd ? "sd " : "",
             w->has_wifi ? "wifi" : "", w->has_eth ? " eth" : "",
             w->wifi_link ? "wifi-up" : "wifi-off",
             w->eth_link ? " eth-up" : "",
             w->gpu_connector == 2 ? "dsi" : (w->gpu_connector == 1 ? "edp" : "?"),
             w->screen_w, w->screen_h, w->vram_mb,
             w->has_battery ? "has" : "no");
    return ns_write(sub, buf);
}
