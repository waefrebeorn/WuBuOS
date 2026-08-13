/* src/runtime/wubu_ns_dram.c
 *
 * The /n/dram control subtree — the DRAM-refresh hedge over the Styx/9P
 * namespace. Same shape as /n/ec and /n/steaminput: each file wraps the
 * REAL wubu_dram_hedge API via ns_mkdir/ns_write from wubu_ns_fs.c.
 */
#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_dram.h"
#include "wubu_dram_hedge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The hedge instance backing /n/dram. */
static wdh_hedge_t *g_dram = NULL;

/* publish the /n/dram tree (called after wubu_ns_bridge_create). */
int wubu_ns_publish_dram(int elem_size, unsigned replicas)
{
    char sub[128], buf[96];

    if (g_dram) { wdh_destroy(g_dram); g_dram = NULL; }
    if (ns_mkdir("dram") != 0) return -1;

    g_dram = wdh_create((size_t)(elem_size > 0 ? elem_size : 8), replicas);
    if (!g_dram) return -1;

    snprintf(sub, sizeof(sub), "dram/state");
    snprintf(buf, sizeof(buf), "replicas %u, elem %zu, slots %zu, trefi %s\n",
             wdh_replicas(g_dram), wdh_elem_size(g_dram), wdh_slots(g_dram),
             wdh_trefi_periodic(g_dram) ? "periodic" : "none");
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "dram/slots");
    snprintf(buf, sizeof(buf), "%zu\n", wdh_slots(g_dram));
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "dram/replicas");
    snprintf(buf, sizeof(buf), "%u\n", wdh_replicas(g_dram));
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "dram/elem");
    snprintf(buf, sizeof(buf), "%zu\n", wdh_elem_size(g_dram));
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "dram/trefi");
    snprintf(buf, sizeof(buf), "%d\n", wdh_trefi_periodic(g_dram));
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "dram/ctrl");
    if (ns_write(sub, "0:0\n") != 0) return -1;

    snprintf(sub, sizeof(sub), "dram/status");
    snprintf(buf, sizeof(buf), "hedge up: %u replicas x %zu bytes, %zu slots\n",
             wdh_replicas(g_dram), wdh_elem_size(g_dram), wdh_slots(g_dram));
    return ns_write(sub, buf);
}

/* put a replicated slot via the hedge API + refresh state/status.
 * `echo 3:42 > /n/dram/ctrl` stores 42 in slot 3 (all replicas). */
int wubu_ns_dram_put(unsigned long idx, unsigned long value)
{
    char sub[128], buf[64];
    if (!g_dram) return -1;

    /* write the 8-byte value into slot idx on every replica */
    unsigned long v = value;
    if (wdh_put(g_dram, idx, &v) != 0) return -1;

    /* refresh /n/dram/state + /n/dram/status */
    snprintf(sub, sizeof(sub), "dram/state");
    snprintf(buf, sizeof(buf), "replicas %u, elem %zu, slots %zu, trefi %s\n",
             wdh_replicas(g_dram), wdh_elem_size(g_dram), wdh_slots(g_dram),
             wdh_trefi_periodic(g_dram) ? "periodic" : "none");
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "dram/status");
    snprintf(buf, sizeof(buf), "slot %lu <- %lu\n", idx, value);
    return ns_write(sub, buf);
}

/* Refresh the live status lines (trefi result can change per machine). */
int wubu_ns_dram_refresh(void)
{
    char sub[128], buf[96];
    if (!g_dram) return -1;

    snprintf(sub, sizeof(sub), "dram/trefi");
    snprintf(buf, sizeof(buf), "%d\n", wdh_trefi_periodic(g_dram));
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "dram/state");
    snprintf(buf, sizeof(buf), "replicas %u, elem %zu, slots %zu, trefi %s\n",
             wdh_replicas(g_dram), wdh_elem_size(g_dram), wdh_slots(g_dram),
             wdh_trefi_periodic(g_dram) ? "periodic" : "none");
    return ns_write(sub, buf);
}

/* the backing hedge instance (for direct inspection / tests). */
wdh_hedge_t *wubu_ns_dram_hedge(void)
{
    return g_dram;
}

