/*
 * wubu_ns_experts.c -- WuBuOS Namespace Bridge: the SSD expert slot-bank
 * control plane. The MoE slot-bank (ds4-ssd: a fixed pool of resident
 * expert slots per layer, LRU-evicted, paged straight from the checkpoint
 * shards) is a Brain resource -- exposing it at /n/experts makes the
 * paging behaviour introspectable from the shell, same doctrine as
 * /n/kernel, /n/dram and /n/bonzi.
 *
 *   /n/experts/config     -- "layers experts d_model d_ff slot_bank"
 *   /n/experts/stats      -- "pageins hits bytes_read"
 *   /n/experts/layer/<L>  -- one line per resident slot: "slot:expert:lru"
 *                            (-1 expert = free slot)
 *
 * The slot-bank lives in wubuwizard (the Brain); this module keeps its OWN
 * mirror and the owner pushes state in via the setters each forward pass,
 * then wubu_ns_experts_sync() flushes it to /n. Same dependency-light
 * discipline as wubu_ns_dram (links only wubu_ns_fs.o, not the slot-bank).
 *
 * Reuses g_ns_root/ns_mkdir/ns_write from wubu_ns_fs.c. C11, opaque, minimal.
 */

#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_experts.h"
#include <stdio.h>
#include <string.h>

#define NSX_MAX_LAYERS 256
#define NSX_MAX_SLOTS  64

/* The mirror of live slot-bank state (single source of truth for /n). */
static struct {
    int  n_layers, n_experts, d_model, d_ff, slot_bank;
    long stat_pageins, stat_hits;
    long long stat_bytes;
    /* per-layer residency: slots[layer][s] = expert id, or -1 if free */
    int  slots[NSX_MAX_LAYERS][NSX_MAX_SLOTS];
    long lru[NSX_MAX_LAYERS][NSX_MAX_SLOTS];
    int  n_slots[NSX_MAX_LAYERS];
} g_x;

void wubu_ns_experts_set_config(int n_layers, int n_experts, int d_model,
                                int d_ff, int slot_bank) {
    g_x.n_layers = n_layers; g_x.n_experts = n_experts;
    g_x.d_model = d_model; g_x.d_ff = d_ff; g_x.slot_bank = slot_bank;
}

void wubu_ns_experts_set_stats(long pageins, long hits, long long bytes) {
    g_x.stat_pageins = pageins; g_x.stat_hits = hits; g_x.stat_bytes = bytes;
}

/* Record which expert occupies each slot of one layer. slots[] holds expert
 * ids (-1 = free); lru[] the matching timestamps. n = slot count. */
void wubu_ns_experts_set_layer(int layer, const int *slots, const long *lru,
                               int n) {
    if (layer < 0 || layer >= NSX_MAX_LAYERS) return;
    if (n > NSX_MAX_SLOTS) n = NSX_MAX_SLOTS;
    g_x.n_slots[layer] = n;
    for (int s = 0; s < n; s++) {
        g_x.slots[layer][s] = slots ? slots[s] : -1;
        g_x.lru[layer][s]   = lru   ? lru[s]   : 0;
    }
}

/* Flush the mirror into /n/experts/*. Returns 0 on success, -1 if the
 * namespace root isn't set. */
int wubu_ns_experts_sync(void)
{
    if (!g_ns_root) return -1;
    char sub[128], buf[4096];

    if (ns_mkdir("experts") != 0) return -1;

    snprintf(sub, sizeof(sub), "experts/config");
    snprintf(buf, sizeof(buf), "layers %d, experts %d, d_model %d, d_ff %d, slot_bank %d\n",
             g_x.n_layers, g_x.n_experts, g_x.d_model, g_x.d_ff, g_x.slot_bank);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "experts/stats");
    snprintf(buf, sizeof(buf), "pageins %ld, hits %ld, bytes_read %lld\n",
             g_x.stat_pageins, g_x.stat_hits, (long long)g_x.stat_bytes);
    if (ns_write(sub, buf) != 0) return -1;

    if (ns_mkdir("experts/layer") != 0) return -1;
    for (int L = 0; L < g_x.n_layers && L < NSX_MAX_LAYERS; L++) {
        snprintf(sub, sizeof(sub), "experts/layer/%d", L);
        /* build the multi-line body in one buffer */
        int off = 0;
        off += snprintf(buf + off, sizeof(buf) - off, "# slot:expert:lru\n");
        for (int s = 0; s < g_x.n_slots[L]; s++) {
            int w = snprintf(buf + off, sizeof(buf) - off, "%d:%d:%ld\n",
                             s, g_x.slots[L][s], g_x.lru[L][s]);
            if (w < 0 || (size_t)(off + w) >= sizeof(buf)) break;
            off += w;
        }
        if (ns_write(sub, buf) != 0) return -1;
    }
    return 0;
}
