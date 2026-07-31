/*
 * wubu_selfimprove.h -- Mega OS self-improvement loop (Phase C of MEGA_COMPAT_PLANNING.md).
 *
 * trace -> eval dataset -> INDEPENDENT verifier -> promotion gate -> next cycle.
 * Answers DA-3 (alignment/agentic-risk):
 *   - The verifier is a SEPARATE component (never the self-improving agent). Here it is
 *     a pluggable fn pointer so the operator can wire a strong-model overseer (debate/
 *     weak-to-strong). Same-agent grading = rubber stamp = drift.
 *   - A promotion gate requires independent sign-off before any change reaches "production".
 *   - The loop is freezable (user switch) and weights FAILED/divergent traces (86.7%
 *     multi-agent failure reality) so it does not overfit to the 13% that worked.
 *
 * Opaque struct, C11, minimal includes.
 */
#ifndef WUBU_SELFIMPROVE_H
#define WUBU_SELFIMPROVE_H

#include "wubu_trace.h"

/* Independent verifier signature. Must NOT be the agent under test.
 * Returns a score in [0,1] (1 = good) and sets *passed (true = promote). */
typedef float (*wubu_verifier_fn)(const wubu_trace_span_t *span, void *ud, bool *passed);

typedef struct wubu_selfimprove wubu_selfimprove_t;

wubu_selfimprove_t *wubu_selfimprove_create(void);
void wubu_selfimprove_destroy(wubu_selfimprove_t *s);

/* Wire the independent verifier (DA-3). NULL => loop refuses to promote. */
void wubu_selfimprove_set_verifier(wubu_selfimprove_t *s, wubu_verifier_fn fn, void *ud);

/* Ingest a trace span into the eval store. failed=true weights it higher (DA-2). */
int wubu_selfimprove_ingest(wubu_selfimprove_t *s, const wubu_trace_span_t *span,
                            bool failed);

/* Run one improvement cycle. For each ingested span it scores via the INDEPENDENT
 * verifier; promoted changes are recorded (gated). Returns #promoted this cycle.
 * If frozen or no verifier, returns 0 and changes nothing. */
int wubu_selfimprove_cycle(wubu_selfimprove_t *s);

/* Promotion gate (DA-3): when true, a human/overseer must approve before promote().
 * When false, the verifier alone decides (still independent of the agent). */
void wubu_selfimprove_set_human_gate(wubu_selfimprove_t *s, bool require);
/* Call to supply the human/overseer approval for the pending cycle. */
void wubu_selfimprove_approve(wubu_selfimprove_t *s, bool approved);

/* Freeze (DA-3): user can stop the loop cold. */
void wubu_selfimprove_set_frozen(wubu_selfimprove_t *s, bool frozen);
bool wubu_selfimprove_is_frozen(const wubu_selfimprove_t *s);

/* Stats for the operator dashboard. */
int wubu_selfimprove_total(const wubu_selfimprove_t *s);
int wubu_selfimprove_promoted(const wubu_selfimprove_t *s);

#endif /* WUBU_SELFIMPROVE_H */
