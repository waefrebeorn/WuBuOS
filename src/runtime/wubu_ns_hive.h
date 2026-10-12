/*
 * wubu_ns_hive.h -- the diagnostic hive /n control plane.
 *
 * The hive is the AGI's immune system (diagnose -> mutate -> validate ->
 * archive). This exposes its live health at /n/hive so the shell can
 * introspect the colony, same doctrine as /n/kernel and /n/dram. The owner
 * pushes the latest diagnosis + ledger each batch, then wubu_ns_hive_sync()
 * flushes to /n. See wubu_ns_hive.c.
 */
#ifndef WUBU_NS_HIVE_H
#define WUBU_NS_HIVE_H

#include <stdint.h>

/* The diagnosis snapshot the owner pushes each batch (mirrors the Brain's
 * wubu_diag_record_t fields the namespace cares about). */
typedef struct {
    /* loss surface */
    float loss, loss_ema, slope; int plateau;
    /* expert utilization */
    int n_experts; float util_min, util_max, util_mean;
    /* gradient health */
    float grad_norm_max, grad_norm_mean; int n_dead_cells;
    /* fitness */
    float fitness, prev_fitness;
    /* provenance */
    uint64_t batch; uint32_t epoch;
} wubu_ns_hive_diag_t;

/* Push the latest batch diagnosis into the mirror. */
void wubu_ns_hive_set_diagnosis(const wubu_ns_hive_diag_t *d);

/* Push the mutation-gate ledger counters. */
void wubu_ns_hive_set_ledger(int accepted, int rejected, int stasis);

/* Flush the mirror into /n/hive/*. Returns 0 on success, -1 if the
 * namespace root isn't set. */
int wubu_ns_hive_sync(void);

#endif /* WUBU_NS_HIVE_H */
