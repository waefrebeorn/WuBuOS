/*
 * wubu_ns_bonzi.c -- WuBuOS Namespace Bridge: the Bonzi companion control
 * plane. The AGI face is a first-class OS citizen, inspectable and drivable
 * from the shell through /n, exactly like the kernel scheduler (/n/kernel)
 * and the DRAM hedge (/n/dram). Meta-layer property: what the agent IS is
 * readable as files.
 *
 *   /n/bonzi/enabled   -- "1"/"0"  (the orb is live)
 *   /n/bonzi/pos       -- "x y"    (desktop position)
 *   /n/bonzi/size      -- "w h"
 *   /n/bonzi/bubble    -- the current speech-bubble text
 *   /n/bonzi/agi       -- "1"/"0"  (the AGI panel is open)
 *
 * The GUI owns no dependency here: this module keeps its OWN mirror of the
 * companion state. The renderer pushes the live state in via the setters
 * (wubu_ns_bonzi_set_*) each tick, then wubu_ns_bonzi_sync() flushes it to
 * /n. That keeps the namespace test lightweight -- it links only this module
 * and the FS core, not the wubu_bonzi renderer (same discipline as
 * wubu_ns_dram linking only wubu_dram_hedge, not the DRAM controller).
 *
 * Reuses g_ns_root/ns_mkdir/ns_write from wubu_ns_fs.c. C11, opaque, minimal.
 */

#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_bonzi.h"
#include <stdio.h>
#include <string.h>

/* The mirror of live companion state (single source of truth for /n). */
static struct {
    int  enabled;
    int  x, y, w, h;
    int  agi_open;
    char bubble[192];
} g_bonzi;

void wubu_ns_bonzi_set_enabled(int on)          { g_bonzi.enabled = on ? 1 : 0; }
void wubu_ns_bonzi_set_pos(int x, int y)        { g_bonzi.x = x; g_bonzi.y = y; }
void wubu_ns_bonzi_set_size(int w, int h)       { g_bonzi.w = w; g_bonzi.h = h; }
void wubu_ns_bonzi_set_agi(int open)            { g_bonzi.agi_open = open ? 1 : 0; }
void wubu_ns_bonzi_set_bubble(const char *text) {
    if (!text) text = "";
    snprintf(g_bonzi.bubble, sizeof(g_bonzi.bubble), "%s\n", text);
}

int wubu_ns_bonzi_enabled(void)  { return g_bonzi.enabled; }
int wubu_ns_bonzi_agi_open(void) { return g_bonzi.agi_open; }

/* Flush the mirror into the /n/bonzi tree. Returns 0 on success, -1 if the
 * namespace root isn't set (wubu_ns_bridge_create not called yet). */
int wubu_ns_bonzi_sync(void)
{
    if (!g_ns_root) return -1;
    char sub[128], buf[256];

    if (ns_mkdir("bonzi") != 0) return -1;

    snprintf(sub, sizeof(sub), "bonzi/enabled");
    snprintf(buf, sizeof(buf), "%d\n", g_bonzi.enabled);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "bonzi/pos");
    snprintf(buf, sizeof(buf), "%d %d\n", g_bonzi.x, g_bonzi.y);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "bonzi/size");
    snprintf(buf, sizeof(buf), "%d %d\n", g_bonzi.w, g_bonzi.h);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "bonzi/bubble");
    if (ns_write(sub, g_bonzi.bubble) != 0) return -1;

    snprintf(sub, sizeof(sub), "bonzi/agi");
    snprintf(buf, sizeof(buf), "%d\n", g_bonzi.agi_open);
    if (ns_write(sub, buf) != 0) return -1;

    return 0;
}
