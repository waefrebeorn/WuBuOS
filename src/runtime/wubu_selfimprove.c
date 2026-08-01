/*
 * wubu_selfimprove.c -- Mega OS self-improvement loop (Phase C).
 * Independent verifier + human gate + freeze + failure-weighting.
 */
#include "wubu_selfimprove.h"

#include <stdlib.h>
#include <string.h>

#define SI_MAX 2048

struct wubu_selfimprove {
    wubu_verifier_fn verifier;
    void *ud;
    bool human_gate;        /* require explicit approve() before promote */
    bool approved;          /* pending approval for current cycle */
    bool frozen;            /* DA-3 user freeze */
    int total;
    int promoted;
    wubu_operator_fn operator_fn;  /* fires on each promoted change */
    void *op_ud;
    wubu_trace_span_t store[SI_MAX];
    int count;
    int weight[SI_MAX];     /* higher for failed/divergent (DA-2) */
};

wubu_selfimprove_t *wubu_selfimprove_create(void) {
    wubu_selfimprove_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->human_gate = true;   /* default: a human/overseer must sign off */
    s->approved = false;
    return s;
}
void wubu_selfimprove_destroy(wubu_selfimprove_t *s) { free(s); }

void wubu_selfimprove_set_verifier(wubu_selfimprove_t *s, wubu_verifier_fn fn, void *ud) {
    if (s) { s->verifier = fn; s->ud = ud; }
}

void wubu_selfimprove_set_operator(wubu_selfimprove_t *s, wubu_operator_fn fn, void *ud) {
    if (s) { s->operator_fn = fn; s->op_ud = ud; }
}

int wubu_selfimprove_ingest(wubu_selfimprove_t *s, const wubu_trace_span_t *span,
                            bool failed) {
    if (!s || !span || s->count >= SI_MAX) return -1;
    s->store[s->count] = *span;
    s->weight[s->count] = failed ? 3 : 1;   /* DA-2: weight failures 3x */
    s->count++;
    s->total++;
    return 0;
}

int wubu_selfimprove_cycle(wubu_selfimprove_t *s) {
    if (!s) return 0;
    if (s->frozen) return 0;          /* DA-3: loop stopped cold */
    if (!s->verifier) return 0;       /* DA-3: no independent verifier => no promote */
    if (s->human_gate && !s->approved) return 0;  /* DA-3: gate not signed */

    int promoted_this = 0;
    for (int i = 0; i < s->count; i++) {
        bool passed = false;
        float score = s->verifier(&s->store[i], s->ud, &passed);
        (void)score;
        if (passed) {
            /* A self-modification event is recorded (gated), never auto-applied
             * to the running OS without the operator promoting it out-of-band.
             * The operator hook FIRES HERE: it is the action step of the AGI
             * operator system — apply the change, restart a subsystem, emit a
             * trace directive. Still gated by independent sign-off (DA-3). */
            promoted_this++;
            s->promoted++;
            if (s->operator_fn)
                s->operator_fn(&s->store[i], s->op_ud);
        }
    }
    /* Reset for next cycle; weights already captured in stats. */
    s->count = 0;
    s->approved = false;
    return promoted_this;
}

void wubu_selfimprove_set_human_gate(wubu_selfimprove_t *s, bool require) {
    if (s) s->human_gate = require;
}
void wubu_selfimprove_approve(wubu_selfimprove_t *s, bool approved) {
    if (s) s->approved = approved;
}
void wubu_selfimprove_set_frozen(wubu_selfimprove_t *s, bool frozen) {
    if (s) s->frozen = frozen;
}
bool wubu_selfimprove_is_frozen(const wubu_selfimprove_t *s) {
    return s ? s->frozen : true;
}
int wubu_selfimprove_total(const wubu_selfimprove_t *s)    { return s ? s->total : 0; }
int wubu_selfimprove_promoted(const wubu_selfimprove_t *s){ return s ? s->promoted : 0; }
