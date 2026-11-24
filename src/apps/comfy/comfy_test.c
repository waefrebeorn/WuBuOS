/*
 * comfy_test.c -- Comfy node-graph engine
 *
 * Comfy is the "make the AGI do things" surface: a dataflow graph whose nodes
 * do real work (emit text, compute, launch an app, hand a string to Bonzi,
 * print). It had no test at all, and its documented programmatic API could not
 * be used before a window existed (comfy_add_node() returned -1 until
 * comfy_launch() had run) -- that is pinned here too.
 *
 * The evaluator is a fixpoint loop capped at n_nodes + 1 passes, so a cycle
 * cannot hang: it stops progressing and the run returns 0. That is asserted
 * rather than assumed.
 */

#include "comfy.h"
#include <stdio.h>
#include <string.h>

static int pass = 0, fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { fail++; printf("  FAIL: %s\n", msg); return; } } while (0)
#define OK() do { pass++; printf("  ok\n"); } while (0)

static void test_api_without_a_window(void) {
    printf("\n=== Test: graph API works with no window (as documented) ===\n");
    comfy_reset_graph();
    CHECK(comfy_node_count() == 0, "starts empty");
    int t = comfy_add_node("TEXT", "hello", 10, 10);
    CHECK(t == 0, "first node takes index 0 with no GUI");
    CHECK(comfy_node_count() == 1, "node counted");
    CHECK(comfy_add_node("NOT_A_TYPE", NULL, 0, 0) == -1, "an unknown node type is refused");
    CHECK(comfy_add_edge(0, 0, 99, 0) == -1, "an edge to a missing node is refused");
    CHECK(comfy_edge_count() == 0, "no edge was recorded");
    OK();
}

static void test_pipeline_runs(void) {
    printf("\n=== Test: TEXT -> CALC -> PRINT evaluates and reports work ===\n");
    comfy_reset_graph();   /* one graph per process: start clean */
    /* A CALC node has TWO inputs, so a graph that wires only one of them is
     * legitimately unrunnable -- feed both, as a user would. */
    int a = comfy_add_node("TEXT", "3", 10, 10);
    int b = comfy_add_node("TEXT", "4", 10, 80);
    int c = comfy_add_node("CALC", "sum", 10, 150);
    int d = comfy_add_node("PRINT", "out", 10, 230);
    CHECK(a >= 0 && b >= 0 && c >= 0 && d >= 0, "four nodes created");
    CHECK(comfy_add_edge(a, 0, c, 0) >= 0, "TEXT 3 -> CALC.a");
    CHECK(comfy_add_edge(b, 0, c, 1) >= 0, "TEXT 4 -> CALC.b");
    CHECK(comfy_add_edge(c, 0, d, 0) >= 0, "CALC -> PRINT");
    CHECK(comfy_edge_count() == 3, "three edges recorded");

    int ran = comfy_run_graph();
    printf("  nodes executed: %d\n", ran);
    CHECK(ran == 4, "every node in a fully-wired graph executes");
    OK();
}

static void test_cycle_terminates(void) {
    printf("\n=== Test: a cycle cannot hang the evaluator ===\n");
    comfy_reset_graph();
    /* Two CALC nodes feeding each other, with no source: nothing is ever ready,
     * so the run must stop after its bounded passes and report zero. */
    int a = comfy_add_node("CALC", "c1", 10, 10);
    int b = comfy_add_node("CALC", "c2", 10, 100);
    CHECK(a >= 0 && b >= 0, "two CALC nodes");
    CHECK(comfy_add_edge(a, 0, b, 0) >= 0, "c1 -> c2");
    CHECK(comfy_add_edge(b, 0, a, 0) >= 0, "c2 -> c1 (cycle)");

    int ran = comfy_run_graph();
    printf("  nodes executed in the cycle: %d\n", ran);
    CHECK(ran == 0, "a pure cycle runs nothing and returns instead of hanging");
    CHECK(comfy_node_count() == 2, "and leaves the graph intact");
    OK();
}

int main(void) {
    printf("=== Comfy node-graph tests ===\n");
    test_api_without_a_window();
    test_pipeline_runs();
    test_cycle_terminates();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
