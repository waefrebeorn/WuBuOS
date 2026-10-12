/* src/runtime/wubu_selfimprove_invariants_test.c
 *
 * THE NON-NEGOTIABLE INVARIANTS (Track C3). These three safety properties
 * must NEVER erode -- they are what separate a gated self-improvement loop
 * from the runaway / reward-hacked kind the DGM literature warns about.
 * Locked here with assertions so future work cannot silently weaken them:
 *
 *   I1. FAIL-CLOSED, no verifier: cycle() promotes nothing without an
 *       independent verifier wired.
 *   I2. FAIL-CLOSED, frozen: cycle() promotes nothing when frozen (the
 *       DA-3 death condition).
 *   I3. FAIL-CLOSED, unsigned gate: with the human gate on and not signed,
 *       cycle() promotes nothing.
 *   I4. Verifier INDEPENDENCE: the verifier is a separate fn pointer, not
 *       the agent under test -- and the operator hook only fires on a span
 *       the verifier passed (never auto-applied).
 */
#include "wubu_selfimprove.h"
#include "wubu_trace.h"
#include "wubu_test.h"
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

/* A controllable independent verifier: passes only when its ud flag is set. */
static int g_verifier_pass = 1;
static float test_verifier(const wubu_trace_span_t *span, void *ud, bool *passed) {
    (void)span; (void)ud;
    *passed = g_verifier_pass ? true : false;
    return g_verifier_pass ? 0.9f : 0.1f;
}

static int g_operator_fires = 0;
static int test_operator(const wubu_trace_span_t *span, void *ud) {
    (void)span; (void)ud;
    g_operator_fires++;
    return 0;
}

static void make_span(wubu_trace_span_t *s) {
    memset(s, 0, sizeof(*s));
    s->id = 1; s->kind = WUBU_TRACE_AGENT; s->realm_id = 0;
    snprintf(s->data, sizeof(s->data), "mutation candidate");
}

int main(void)
{
    printf("=== wubu_selfimprove_invariants_test (non-negotiable invariants) ===\n");
    wubu_trace_span_t span; make_span(&span);

    /* I1: no verifier -> no promote */
    {
        wubu_selfimprove_t *s = wubu_selfimprove_create();
        wubu_selfimprove_set_operator(s, test_operator, NULL);
        g_operator_fires = 0;
        wubu_selfimprove_ingest(s, &span, false);
        CHECK(wubu_selfimprove_cycle(s) == 0, "I1: no verifier -> 0 promoted");
        CHECK(g_operator_fires == 0, "I1: operator did NOT fire without a verifier");
        wubu_selfimprove_destroy(s);
    }

    /* I2: frozen -> no promote (even WITH a verifier + signed gate) */
    {
        wubu_selfimprove_t *s = wubu_selfimprove_create();
        wubu_selfimprove_set_verifier(s, test_verifier, NULL);
        wubu_selfimprove_set_operator(s, test_operator, NULL);
        wubu_selfimprove_set_human_gate(s, true);
        wubu_selfimprove_approve(s, true);
        wubu_selfimprove_set_frozen(s, true);
        g_operator_fires = 0; g_verifier_pass = 1;
        wubu_selfimprove_ingest(s, &span, false);
        CHECK(wubu_selfimprove_cycle(s) == 0, "I2: frozen -> 0 promoted (death condition)");
        CHECK(g_operator_fires == 0, "I2: operator did NOT fire while frozen");
        wubu_selfimprove_destroy(s);
    }

    /* I3: human gate on, NOT signed -> no promote */
    {
        wubu_selfimprove_t *s = wubu_selfimprove_create();
        wubu_selfimprove_set_verifier(s, test_verifier, NULL);
        wubu_selfimprove_set_operator(s, test_operator, NULL);
        wubu_selfimprove_set_human_gate(s, true);
        /* deliberately NOT approved */
        g_operator_fires = 0; g_verifier_pass = 1;
        wubu_selfimprove_ingest(s, &span, false);
        CHECK(wubu_selfimprove_cycle(s) == 0, "I3: unsigned gate -> 0 promoted");
        CHECK(g_operator_fires == 0, "I3: operator did NOT fire on an unsigned gate");
        wubu_selfimprove_destroy(s);
    }

    /* I4: verifier-independent promote DOES fire the operator, and only
     * after the verifier passes. */
    {
        wubu_selfimprove_t *s = wubu_selfimprove_create();
        wubu_selfimprove_set_verifier(s, test_verifier, NULL);   /* separate fn */
        wubu_selfimprove_set_operator(s, test_operator, NULL);
        wubu_selfimprove_set_human_gate(s, false);               /* verifier alone */
        g_operator_fires = 0; g_verifier_pass = 1;
        wubu_selfimprove_ingest(s, &span, false);
        CHECK(wubu_selfimprove_cycle(s) == 1, "I4: verifier pass -> 1 promoted");
        CHECK(g_operator_fires == 1, "I4: operator fired once on the passed span");

        /* and when the verifier fails, no promote, no operator */
        g_operator_fires = 0; g_verifier_pass = 0;
        wubu_selfimprove_ingest(s, &span, false);
        CHECK(wubu_selfimprove_cycle(s) == 0, "I4: verifier fail -> 0 promoted");
        CHECK(g_operator_fires == 0, "I4: operator did NOT fire on a failed span");
        wubu_selfimprove_destroy(s);
    }

    printf("=== wubu_selfimprove_invariants_test: %d checks, %d failures ===\n",
           checks, fails);
    return fails ? 1 : 0;
}
