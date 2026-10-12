/*
 * wubu_mutation_bridge.h -- the Brain-archive -> Body-operator bridge
 * (Track C1).
 *
 * Closes the recursive self-improvement loop end-to-end: translates an
 * ACCEPTED Brain fitness cell into a WUBU_TRACE_SELFMOD span the Body's
 * wubu_selfimprove can ingest -> verify (DA-3) -> promote -> operate on.
 * REJECTED cells never bridge (they stay in the graveyard). The downstream
 * safety (C2 held-out, C3 fail-closed gates, C4 budget) is untouched.
 */
#ifndef WUBU_MUTATION_BRIDGE_H
#define WUBU_MUTATION_BRIDGE_H

#include <stdint.h>
#include "wubu_trace.h"   /* wubu_trace_span_t, WUBU_TRACE_SELFMOD */

/* A view of the Brain's mutation record (wubu_fitness_cell_t) -- mirrored so
 * the bridge links only the trace type, not the whole Brain. */
typedef struct {
    uint64_t batch;
    uint32_t epoch;
    float    fitness;   /* held-out fitness */
    float    delta;     /* change vs previous */
    int      verdict;   /* 1 = ACCEPT, 0 = REJECT */
    uint8_t  cell_idx;
} wubu_fitness_cell_view_t;

/* Translate an ACCEPTED fitness cell into a self-mod trace span. Returns 1
 * and fills *span on success; returns 0 (no bridge) for a REJECTED cell or
 * bad args -- rejected mutations never reach the gate. */
int wubu_mutation_bridge_to_span(const wubu_fitness_cell_view_t *cell,
                                 uint64_t span_id, uint64_t parent,
                                 uint64_t ts_ms, int realm_id,
                                 wubu_trace_span_t *span);

#endif /* WUBU_MUTATION_BRIDGE_H */
