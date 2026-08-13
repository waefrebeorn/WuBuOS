/*
 * wubu_theme_bridge_test.c -- verifies the kernel /theme -> GUI render
 * bridge: when the AGI/console writes a /theme node, wubu_theme_sync_
 * from_kernel() overlays it onto the GUI's live render colors so the next
 * frame re-renders with it (the "AGI writes a node, the display follows"
 * promise). The kernel node layer is stubbed here exactly as the real
 * kernel theme engine behaves (test_theme_hid pattern) — the GUI theme
 * reads it via a weak reference, so this proves the wiring without needing
 * both engines linked in one binary (they collide on wubu_theme_get).
 */
#include "wubu_theme.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

/* -- Stub the kernel /theme namespace (what wubu_theme_node_get reads) -- */
static uint32_t s_win_title_active = 0x000080;  /* Win98 navy (GUI RGB pack) */
static int      s_known = 1;                     /* known path flag */

/* The GUI bridge calls this via weak ref. Model it on the real kernel
 * node_get: return 0 + value for known paths, -1 for unknown. */
int wubu_theme_node_get(const char *path, uint32_t *out) {
    if (!s_known) return -1;
    if (path && out) {
        if (path[0] == '/' && path[1] == 't') {  /* "/theme/..." */
            *out = s_win_title_active;
            return 0;
        }
    }
    return -1;
}

int main(void) {
    wubu_theme_init();               /* GUI engine seeds WIN98 live colors */
    const WubuThemeColors *c = wubu_theme_colors();
    printf("base win_title_active = 0x%06X\n", c->win_title_active);

    /* Initially (no sync) the base theme's title stands. */
    assert(c->win_title_active == 0x000080);     /* Win98 navy */

    /* The AGI writes a node; sync overlays it onto the live colors. */
    s_win_title_active = 0x00FF00;               /* AGI recolors title green */
    int applied = wubu_theme_sync_from_kernel();
    printf("applied %d nodes; win_title_active -> 0x%06X\n",
           applied, c->win_title_active);
    assert(applied >= 1);
    assert(c->win_title_active == 0x00FF00);     /* live color followed */

    /* A full re-set re-seeds from the base (fresh canvas for the AGI). */
    wubu_theme_set(THEME_WIN98_CLASSIC);
    assert(c->win_title_active == 0x000080);
    /* ...and a second sync re-applies the AGI write. */
    wubu_theme_sync_from_kernel();
    assert(c->win_title_active == 0x00FF00);

    printf("[ok] wubu_theme_bridge: kernel /theme -> GUI render sync works\n");
    return 0;
}
