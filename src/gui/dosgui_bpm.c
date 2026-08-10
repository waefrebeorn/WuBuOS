/*
 * dosgui_bpm.c -- BIG PICTURE MODE (the SteamOS Big Picture shell,
 * in our code).
 *
 * SteamOS's Big Picture Mode is the fullscreen gamepad-first shell:
 * a grid of games, the d-pad moves the selection, A launches, B goes
 * back. WuBuOS's BPM is the SAME shell over the era-apps registry
 * (CP/M 1974 → HolyC 2020 — the "everything runs on the Colonel"
 * grid):
 *
 *   - the fullscreen grid (4 columns) of the era apps
 *   - the gamepad navigation: the d-pad / the arrow keys
 *   - A / Enter launches the selected app (dosgui_era_apps_launch)
 *   - B / Esc exits BPM back to the desktop
 *   - the runnable apps are live; the gaps are greyed (shown, never
 *     hidden — the AGI sees what it CANNOT run yet, too)
 *
 * The launch hook is injectable (the desktop wires the real era-apps
 * launcher; the test injects a recorder).
 * C11.
 */
#include "dosgui_era_apps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the grid geometry */
#define BPM_COLS 4
#define BPM_TILE_W 220
#define BPM_TILE_H 120

/* the launch hook (injectable) */
typedef int (*BpmLaunchFn)(int idx);

typedef struct {
    BpmLaunchFn launch;
    int         selection;
    int         open;          /* is BPM active? */
    int         tile_w, tile_h;
} BpmState;

static BpmState g_bpm;

/* the default launch: the real era-apps launcher */
static int bpm_default_launch(int idx)
{
    return dosgui_era_apps_launch(idx);
}

/* BP1: init — the fullscreen shell. */
void dosgui_bpm_init(void)
{
    memset(&g_bpm, 0, sizeof(g_bpm));
    g_bpm.launch = bpm_default_launch;
    g_bpm.selection = 0;
    g_bpm.open = 0;
    g_bpm.tile_w = BPM_TILE_W;
    g_bpm.tile_h = BPM_TILE_H;
    dosgui_era_apps_register();
}

/* BP2: the launch hook override (the tests). */
void dosgui_bpm_set_launch(BpmLaunchFn fn)
{
    g_bpm.launch = fn ? fn : bpm_default_launch;
}

/* BP3: open BPM (the desktop calls it). */
void dosgui_bpm_open(void) { g_bpm.open = 1; g_bpm.selection = 0; }
void dosgui_bpm_close(void) { g_bpm.open = 0; }
int  dosgui_bpm_is_open(void) { return g_bpm.open; }

/* BP4: the grid geometry. */
int dosgui_bpm_rows(void)
{
    int n = dosgui_era_apps_total_count();
    return (n + BPM_COLS - 1) / BPM_COLS;
}

/* BP5: the gamepad navigation. Returns 1 if the event was consumed. */
int dosgui_bpm_input(uint32_t key, uint32_t mods, int down)
{
    (void)mods;
    if (!g_bpm.open || !down) return 0;
    int n = dosgui_era_apps_total_count();
    switch (key) {
    case 0x48:   /* Up */
        g_bpm.selection = (g_bpm.selection - BPM_COLS + n) % n;
        return 1;
    case 0x50:   /* Down */
        g_bpm.selection = (g_bpm.selection + BPM_COLS) % n;
        return 1;
    case 0x4B:   /* Left */
        g_bpm.selection = (g_bpm.selection + n - 1) % n;
        return 1;
    case 0x4D:   /* Right */
        g_bpm.selection = (g_bpm.selection + 1) % n;
        return 1;
    case 0x1C:   /* A / Enter: launch the selection */
        if (g_bpm.launch)
            g_bpm.launch(g_bpm.selection);
        return 1;
    case 0x01:   /* B / Esc: back to the desktop */
        g_bpm.open = 0;
        return 1;
    default:
        return 0;
    }
}

/* BP6: the current selection. */
int dosgui_bpm_selection(void) { return g_bpm.selection; }

/* BP7: the test hooks */
typedef struct {
    int open;
    int selection;
    int rows;
} dosgui_bpm_view_t;

int dosgui_bpm_get(dosgui_bpm_view_t *out)
{
    if (!out) return -1;
    out->open = g_bpm.open;
    out->selection = g_bpm.selection;
    out->rows = dosgui_bpm_rows();
    return 0;
}
