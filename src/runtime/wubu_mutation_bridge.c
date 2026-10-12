/*
 * wubu_mutation_bridge.c -- the Brain-archive -> Body-operator bridge
 * (Track C1).
 *
 * This closes the recursive self-improvement loop end-to-end. The Brain's
 * diagnostic loop validates mutations and archives the accepted ones as
 * wubu_fitness_cell_t records (verdict ACCEPT, with held-out fitness +
 * delta). The Body's wubu_selfimprove is the gated promotion engine: it
 * ingests a wubu_trace_span_t, scores it via the INDEPENDENT verifier, and
 * (behind the DA-3 human gate) fires the operator to apply the change.
 *
 * The missing link was the translation: a Brain fitness cell is not yet a
 * Body trace span. This module is that translation -- a pure function that
 * turns an ACCEPTED fitness cell into a WUBU_TRACE_SELFMOD span ready to
 * ingest. REJECTED cells are never bridged (they stay in the graveyard).
 * The safety machinery (C2 held-out, C3 fail-closed gates, C4 budget) all
 * lives downstream in the Body and is untouched here.
 *
 * C11, minimal includes. Pure, unit-testable with no pipeline.
 */

#include "wubu_mutation_bridge.h"
#include "wubu_trace.h"
#include <string.h>
#include <stdio.h>

/* Translate an ACCEPTED fitness cell into a self-mod trace span.
 * Returns 1 and fills *span on success; returns 0 (no bridge) for a
 * REJECTED cell or bad args -- rejected mutations never reach the gate. */
int wubu_mutation_bridge_to_span(const wubu_fitness_cell_view_t *cell,
                                 uint64_t span_id, uint64_t parent,
                                 uint64_t ts_ms, int realm_id,
                                 wubu_trace_span_t *span) {
    if (!cell || !span) return 0;
    if (cell->verdict != 1) return 0;   /* only ACCEPTED cells bridge */

    memset(span, 0, sizeof(*span));
    span->id = span_id;
    span->parent = parent;
    span->ts_ms = ts_ms;
    span->kind = WUBU_TRACE_SELFMOD;
    span->realm_id = realm_id;
    span->ver_major = 1;
    /* human-grepable one-line payload: the mutation's provenance */
    snprintf(span->data, sizeof(span->data),
             "selfmod cell=%u batch=%llu epoch=%u fitness=%.4f delta=%+.4f",
             (unsigned)cell->cell_idx,
             (unsigned long long)cell->batch,
             (unsigned)cell->epoch,
             cell->fitness, cell->delta);
    return 1;
}
