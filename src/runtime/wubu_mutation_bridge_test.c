/* src/runtime/wubu_mutation_bridge_test.c
 *
 * The Brain-archive -> Body-operator bridge, END-TO-END (Track C1).
 * Verifies the recursive self-improvement loop actually closes:
 *   1. an ACCEPTED Brain fitness cell bridges to a self-mod trace span
 *   2. that span, ingested by wubu_selfimprove + passed by the independent
 *      verifier behind the gate, FIRES the operator (the change is applied)
 *   3. a REJECTED cell never bridges (stays in the graveyard)
 */
#include "wubu_mutation_bridge.h"
#include "wubu_selfimprove.h"
#include "wubu_test.h"
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

static float test_verifier(const wubu_trace_span_t *span, void *ud, bool *passed) {
    (void)span; (void)ud; *passed = true; return 0.9f;
}
static int g_operator_fires = 0;
static int test_operator(const wubu_trace_span_t *span, void *ud) {
    (void)span; (void)ud; g_operator_fires++; return 0;
}

int main(void)
{
    printf("=== wubu_mutation_bridge_test (archive -> operator, end-to-end) ===\n");

    /* 1. an ACCEPTED cell bridges to a self-mod span */
    wubu_fitness_cell_view_t accepted = {
        .batch = 2000, .epoch = 12, .fitness = 0.71f, .delta = 0.03f,
        .verdict = 1, .cell_idx = 5,
    };
    wubu_trace_span_t span;
    CHECK(wubu_mutation_bridge_to_span(&accepted, 1, 0, 123456, 0, &span) == 1,
          "ACCEPTED cell bridges to a span");
    CHECK(span.kind == WUBU_TRACE_SELFMOD, "span kind is SELFMOD");
    CHECK(strstr(span.data, "cell=5") != NULL, "span payload carries cell_idx");
    CHECK(strstr(span.data, "fitness=0.7100") != NULL, "span payload carries fitness");

    /* 2. that span -> ingest -> verifier pass -> operator fires (loop closes) */
    wubu_selfimprove_t *s = wubu_selfimprove_create();
    wubu_selfimprove_set_verifier(s, test_verifier, NULL);
    wubu_selfimprove_set_operator(s, test_operator, NULL);
    wubu_selfimprove_set_human_gate(s, false);   /* verifier alone decides */
    g_operator_fires = 0;
    wubu_selfimprove_ingest(s, &span, false);
    int promoted = wubu_selfimprove_cycle(s);
    CHECK(promoted == 1, "the bridged mutation is promoted through the gate");
    CHECK(g_operator_fires == 1, "the operator FIRED -- the change is applied");
    wubu_selfimprove_destroy(s);

    /* 3. a REJECTED cell never bridges */
    wubu_fitness_cell_view_t rejected = {
        .batch = 2001, .epoch = 12, .fitness = 0.40f, .delta = -0.05f,
        .verdict = 0, .cell_idx = 9,
    };
    wubu_trace_span_t span2;
    CHECK(wubu_mutation_bridge_to_span(&rejected, 2, 0, 123457, 0, &span2) == 0,
          "REJECTED cell does NOT bridge (stays in the graveyard)");

    printf("=== wubu_mutation_bridge_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
