/* src/runtime/wubu_ns_dram.h
 *
 * The /n/dram control subtree — expose the WuBuOS DRAM-refresh hedge
 * (wubu_dram_hedge) over the Styx/9P namespace, the same pattern as the
 * /n/ec and /n/steaminput control planes:
 *
 *   /n/dram/state     -> "replicas N, elem N, slots N, trefi periodic"
 *   /n/dram/slots     -> capacity (logical entries)
 *   /n/dram/replicas  -> number of channel replicas
 *   /n/dram/elem      -> element size in bytes
 *   /n/dram/trefi     -> 1 if periodic DRAM refresh detected
 *   /n/dram/ctrl      -> write "idx:value" puts a replicated slot;
 *                        read back gives the current slot-0 value
 *   /n/dram/status    -> one-line live summary
 *
 * Each file wraps the REAL wubu_dram_hedge API via ns_mkdir/ns_write from
 * wubu_ns_fs.c (no reimplementation, no new daemon).
 */
#ifndef WUBU_NS_DRAM_H
#define WUBU_NS_DRAM_H

#include "wubu_dram_hedge.h"

/* Publish the /n/dram tree (after wubu_ns_bridge_create). Returns 0 on
 * success, -1 on failure. Creates the hedge with elem_size/replicas. */
int wubu_ns_publish_dram(int elem_size, unsigned replicas);

/* Put a replicated slot via the hedge API + refresh /n/dram/state. */
int wubu_ns_dram_put(unsigned long idx, unsigned long value);

/* Refresh the live /n/dram/status + /n/dram/trefi lines. */
int wubu_ns_dram_refresh(void);

/* the backing hedge instance (for direct inspection / tests). */
wdh_hedge_t *wubu_ns_dram_hedge(void);

#endif /* WUBU_NS_DRAM_H */
