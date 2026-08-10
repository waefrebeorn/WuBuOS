/*
 * wubu_world.h -- the WORLD-STATE bridge (the OS as the AGI's
 * training space).
 */
#ifndef WUBU_WORLD_H
#define WUBU_WORLD_H

#include <stddef.h>
#include <stdint.h>

/* the world snapshot — the AGI's perception of the running OS.
 * Every field derives from a REAL driver's state. */
typedef struct wubu_world {
    /* storage */
    int      has_nvme;
    uint32_t nvme_gb;
    int      has_sd;
    uint32_t sd_gb;
    /* network */
    int      has_wifi;
    int      wifi_link;
    int      has_eth;
    int      eth_link;
    /* display */
    int      has_gpu;
    int      gpu_connector;
    uint16_t screen_w, screen_h;
    uint32_t vram_mb;
    /* power */
    int      has_battery;
    uint8_t  battery_pct;
    int      battery_charging;
    /* thermal */
    uint8_t  cpu_temp;
    uint8_t  gpu_temp;
    uint8_t  fan_duty;
    int      throttled;
} wubu_world_t;

/* W1: init. */
void wubu_world_init(void);

/* W2: sample — assemble the snapshot from the REAL driver state. */
void wubu_world_sample(void);

/* W3: the snapshot (the AGI's in-process read). */
const wubu_world_t *wubu_world_snapshot(void);

/* W4: the one-line world state (the /n read). */
int wubu_world_state_str(char *out, size_t cap);

#endif
