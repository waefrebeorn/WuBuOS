/*
 * wubu_world.c -- the WORLD-STATE bridge (the OS as the AGI's
 * training space).
 *
 * WuBuOS is an AI operating system: the user plays games, browses,
 * interacts with the AGI, and makes things. Every one of those acts
 * is LIVE TRAINING DATA — the OS is a world the AGI perceives. This
 * module is the perception bridge: a compact, stable world snapshot
 * assembled from the REAL driver state (the registry + the drivers),
 * which the AGI (holyd, the colony) reads to know what the world is
 * doing right now.
 *
 * The snapshot (wubu_world_t) is the AGI's "senses":
 *   - what hardware exists (storage/net/gpu/audio/power/temp)
 *   - what the user is doing (the session + the active game)
 *   - the world health (battery, thermal, throttling)
 *
 * The bridge:
 *   wubu_world_sample()   — assemble the snapshot from the drivers
 *   wubu_world_snapshot() — the C struct (the AGI's in-process read)
 *   /n/world/state        — the one-line world state (the 9P read)
 *
 * The AGI trains on the DELTAS between samples — the world's motion.
 * C11.
 */
#include "libc.h"   /* snprintf lives here in a freestanding TU: without a declaration it was
                     * implicit, returning int -- the same landmine that segfaulted getenv. */
#include "wubu_world.h"
#include "wubu_drv.h"
#include "wubu_drv_nvme.h"
#include "wubu_drv_net.h"
#include "wubu_drv_gpu.h"
#include "wubu_drv_battery.h"
#include "wubu_drv_sd.h"
#include "wubu_drv_thermal.h"

#include <string.h>

static wubu_world_t g_world;

/* The display a session published (0 = none). Survives re-sampling, see wubu_world_set_display(). */
static uint16_t g_published_w, g_published_h;
static int      g_published_conn;
static int g_initialized;

/* W1: init. */
void wubu_world_init(void)
{
    memset(&g_world, 0, sizeof(g_world));
    g_initialized = 1;
}

/* W2: sample — assemble the world snapshot from the REAL driver
 * state. Every field derives from a driver's actual state (the
 * circular-metrics ban: never from counters). */
void wubu_world_sample(void)
{
    if (!g_initialized) wubu_world_init();

    /* the storage: the NVMe + the SD card */
    const wubu_drv_dev_t *nvme = wubu_drv_find("nvme");
    g_world.has_nvme = (nvme != NULL) && wubu_nvme_ready();
    g_world.nvme_gb = (uint32_t)(wubu_nvme_nsze() * wubu_nvme_block_size()
                                 / (1024ULL * 1024 * 1024));
    g_world.has_sd = wubu_sd_card_present();
    g_world.sd_gb = (uint32_t)(wubu_sd_capacity_mb() / 1024);

    /* the network */
    g_world.has_wifi = wubu_net_wifi_present();
    g_world.wifi_link = wubu_net_wifi_link();
    g_world.has_eth = wubu_net_eth_present();
    g_world.eth_link = wubu_net_eth_link();

    /* the display — auto-detect WSL2 first so the GPU isn't missed.
     * On bare metal, wubu_gpu_present_wsl() returns 0 (no /dev/dxg)
     * and the PCI probe fills in from real hardware. On WSL2, it
     * primes the GPU state from /dev/dxg and returns nonzero. */
    g_world.has_gpu = wubu_gpu_present_wsl() || wubu_gpu_present();
    g_world.gpu_connector = wubu_gpu_connector();
    /* A session that PUBLISHED its display (wubu_world_set_display) outranks a probe that reports nothing:
     * re-sampling used to zero the hosted surface back to 0x0 and the Display page then read "not reported"
     * on a machine whose compositor was presenting 1400x880. The probe only wins when it has an answer. */
    uint16_t pw = (uint16_t)wubu_gpu_width(), ph = (uint16_t)wubu_gpu_height();
    if (pw && ph) { g_published_w = pw; g_published_h = ph; g_published_conn = g_world.gpu_connector; }
    g_world.screen_w = g_published_w;
    g_world.screen_h = g_published_h;
    if (g_published_conn) g_world.gpu_connector = g_published_conn;
    g_world.vram_mb = (uint32_t)wubu_gpu_vram_mb();

    /* the power */
    g_world.has_battery = wubu_battery_present();
    g_world.battery_pct = (uint8_t)wubu_battery_percent();
    g_world.battery_charging = wubu_battery_charging();

    /* the thermal */
    g_world.cpu_temp = (uint8_t)wubu_thermal_temp(WUBU_THERMAL_CPU);
    g_world.gpu_temp = (uint8_t)wubu_thermal_temp(WUBU_THERMAL_GPU);
    g_world.fan_duty = (uint8_t)wubu_thermal_fan_duty();
    g_world.throttled = wubu_thermal_throttled();
}

/* W3: the snapshot (the AGI's in-process read). */
const wubu_world_t *wubu_world_snapshot(void)
{
    return &g_world;
}

/* W3b: the display this session presents (see the header). */
void wubu_world_set_display(uint16_t w, uint16_t h, int connector, uint32_t vram_mb)
{
    if (w) g_published_w = g_world.screen_w = w;
    if (h) g_published_h = g_world.screen_h = h;
    g_published_conn = g_world.gpu_connector = connector;
    g_world.vram_mb = vram_mb;
}

/* W4: the one-line world state (the /n read). */
int wubu_world_state_str(char *out, size_t cap)
{
    if (!out || cap == 0) return -1;
    snprintf(out, cap,
             "hw[%s%s%s%s%s] net[wifi:%d eth:%d] scr[%ux%u %s vram:%uMB] "
             "power[bat:%d%% %s] heat[cpu:%dC gpu:%dC fan:%d%% thr:%d]",
             g_world.has_nvme ? "nvme" : "",
             g_world.has_sd ? "+sd" : "",
             g_world.has_gpu ? "+gpu" : "",
             g_world.has_wifi ? "+wifi" : "",
             g_world.has_battery ? "+bat" : "",
             g_world.wifi_link, g_world.eth_link,
             g_world.screen_w, g_world.screen_h,
             g_world.gpu_connector == WUBU_GPU_CONNECTOR_DSI ? "dsi" :
             g_world.gpu_connector == WUBU_GPU_CONNECTOR_EDP ? "edp" : "?",
             g_world.vram_mb,
             g_world.battery_pct,
             g_world.battery_charging ? "charging" : "discharging",
             g_world.cpu_temp, g_world.gpu_temp,
             g_world.fan_duty, g_world.throttled);
    return 0;
}
