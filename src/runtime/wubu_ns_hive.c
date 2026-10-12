/*
 * wubu_ns_hive.c -- WuBuOS Namespace Bridge: the diagnostic hive control
 * plane. The hive is the AGI's immune system (the closed control system:
 * diagnose -> mutate -> validate -> archive). Exposing it at /n/hive makes
 * the live health of the colony introspectable from the shell, same
 * doctrine as /n/kernel, /n/dram, /n/bonzi and /n/experts.
 *
 *   /n/hive/loss       -- "loss ema slope plateau"
 *   /n/hive/util       -- "min max mean" (expert route utilization)
 *   /n/hive/grad       -- "max mean dead_cells" (gradient health)
 *   /n/hive/fitness    -- "fitness prev_fitness"
 *   /n/hive/ledger     -- "accepted rejected stasis" (mutation gate outcomes)
 *   /n/hive/batch      -- "batch epoch n_experts"
 *
 * The diagnosis loop lives in wubuwizard (the Brain); this module keeps its
 * OWN mirror and the owner pushes the latest wubu_diag_record_t + ledger
 * counters in each batch, then wubu_ns_hive_sync() flushes them to /n.
 * Dependency-light: links only wubu_ns_fs.o (same discipline as the other
 * namespace modules).
 *
 * Reuses g_ns_root/ns_mkdir/ns_write from wubu_ns_fs.c. C11, opaque, minimal.
 */

#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_hive.h"
#include <stdio.h>
#include <string.h>

/* The mirror of the latest diagnosis + ledger (single source of truth). */
static struct {
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
    /* mutation gate outcomes */
    int n_accepted, n_rejected, n_stasis;
} g_h;

void wubu_ns_hive_set_diagnosis(const wubu_ns_hive_diag_t *d) {
    if (!d) return;
    g_h.loss = d->loss; g_h.loss_ema = d->loss_ema; g_h.slope = d->slope;
    g_h.plateau = d->plateau;
    g_h.n_experts = d->n_experts;
    g_h.util_min = d->util_min; g_h.util_max = d->util_max; g_h.util_mean = d->util_mean;
    g_h.grad_norm_max = d->grad_norm_max; g_h.grad_norm_mean = d->grad_norm_mean;
    g_h.n_dead_cells = d->n_dead_cells;
    g_h.fitness = d->fitness; g_h.prev_fitness = d->prev_fitness;
    g_h.batch = d->batch; g_h.epoch = d->epoch;
}

void wubu_ns_hive_set_ledger(int accepted, int rejected, int stasis) {
    g_h.n_accepted = accepted; g_h.n_rejected = rejected; g_h.n_stasis = stasis;
}

/* Flush the mirror into /n/hive/*. Returns 0 on success, -1 if the
 * namespace root isn't set. */
int wubu_ns_hive_sync(void)
{
    if (!g_ns_root) return -1;
    char sub[128], buf[256];

    if (ns_mkdir("hive") != 0) return -1;

    snprintf(sub, sizeof(sub), "hive/loss");
    snprintf(buf, sizeof(buf), "loss %.4f, ema %.4f, slope %.6f, plateau %d\n",
             g_h.loss, g_h.loss_ema, g_h.slope, g_h.plateau);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "hive/util");
    snprintf(buf, sizeof(buf), "min %.4f, max %.4f, mean %.4f\n",
             g_h.util_min, g_h.util_max, g_h.util_mean);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "hive/grad");
    snprintf(buf, sizeof(buf), "max %.4f, mean %.4f, dead_cells %d\n",
             g_h.grad_norm_max, g_h.grad_norm_mean, g_h.n_dead_cells);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "hive/fitness");
    snprintf(buf, sizeof(buf), "fitness %.4f, prev %.4f\n",
             g_h.fitness, g_h.prev_fitness);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "hive/ledger");
    snprintf(buf, sizeof(buf), "accepted %d, rejected %d, stasis %d\n",
             g_h.n_accepted, g_h.n_rejected, g_h.n_stasis);
    if (ns_write(sub, buf) != 0) return -1;

    snprintf(sub, sizeof(sub), "hive/batch");
    snprintf(buf, sizeof(buf), "batch %llu, epoch %u, n_experts %d\n",
             (unsigned long long)g_h.batch, g_h.epoch, g_h.n_experts);
    if (ns_write(sub, buf) != 0) return -1;

    return 0;
}
