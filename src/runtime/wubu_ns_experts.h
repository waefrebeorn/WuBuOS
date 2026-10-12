/*
 * wubu_ns_experts.h -- the SSD expert slot-bank /n control plane.
 *
 * The MoE slot-bank (ds4-ssd) is a Brain resource; this exposes its live
 * paging state at /n/experts so the shell can introspect it. Same doctrine
 * as /n/kernel and /n/dram. The owner pushes state via the setters each
 * forward pass, then wubu_ns_experts_sync() flushes to /n. See
 * wubu_ns_experts.c.
 */
#ifndef WUBU_NS_EXPERTS_H
#define WUBU_NS_EXPERTS_H

/* Push the slot-bank configuration (layers/experts/dims/slot count). */
void wubu_ns_experts_set_config(int n_layers, int n_experts, int d_model,
                                int d_ff, int slot_bank);

/* Push cumulative paging stats. */
void wubu_ns_experts_set_stats(long pageins, long hits, long long bytes);

/* Record which expert occupies each slot of one layer. slots[] holds expert
 * ids (-1 = free); lru[] the matching timestamps; n = slot count. */
void wubu_ns_experts_set_layer(int layer, const int *slots, const long *lru,
                               int n);

/* Flush the mirror into /n/experts/*. Returns 0 on success, -1 if the
 * namespace root isn't set. */
int wubu_ns_experts_sync(void);

#endif /* WUBU_NS_EXPERTS_H */
