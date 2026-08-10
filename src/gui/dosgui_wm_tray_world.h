/*
 * dosgui_wm_tray_world.h -- the world-state tray indicators.
 */
#ifndef WUBU_DOSGUI_WM_TRAY_WORLD_H
#define WUBU_DOSGUI_WM_TRAY_WORLD_H

#include <stdint.h>
#include "wubu_world.h"

/* the world snapshot provider (injectable) */
typedef const wubu_world_t *(*WorldSnapFn)(void);

/* TW1: the provider override (the tests). */
void dosgui_tray_world_set_snapshot(WorldSnapFn fn);

/* TW2: wire the three indicators into the systray (idempotent). */
void dosgui_tray_world_wire(void);

/* TW3: refresh the indicators from the world state. */
void dosgui_tray_world_refresh(void);

/* TW4: the test hooks. */
typedef struct {
    int wired;
    uint32_t battery_color;
    uint32_t network_color;
    uint32_t heat_color;
} dosgui_tray_world_view_t;
int dosgui_tray_world_get(dosgui_tray_world_view_t *out);

#endif
