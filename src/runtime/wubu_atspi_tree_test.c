/*
 * wubu_atspi_tree_test.c -- the AT-SPI2 tree walk.
 *
 * The load-bearing rules: a CYCLE is detected rather than walked forever, DEPTH
 * and NODE COUNT are capped, a NULL root is valid, and the callback may abort
 * the walk.
 */
#include "wubu_atspi_tree.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

typedef struct {
    char order[64][WB_ATSPI_TREE_NAME_MAX];
    int  n;
    int  depths[64];
    int  indices[64];
    int  abort_after;      /* -1 = never */
} walk_rec_t;

static bool rec_fn(const wb_atspi_tree_node_t *n, uint32_t depth,
                   uint32_t index, void *user)
{
    walk_rec_t *r = (walk_rec_t *)user;
    if (r && r->n < 64) {
        strncpy(r->order[r->n], n->name, WB_ATSPI_TREE_NAME_MAX - 1);
        r->depths[r->n] = (int)depth;
        r->indices[r->n] = (int)index;
        r->n++;
    }
    if (r && r->abort_after >= 0 && r->n > r->abort_after) return false;
    return true;
}

int main(void)
{
    printf("=== test_wubu_atspi_tree ===\n");
    wb_atspi_walk_result_t r;

    /* -- a simple tree -- */
    {
        wb_atspi_tree_node_t *root = wb_atspi_tree_new("win", WB_ATSPI_ROLE_WINDOW);
        wb_atspi_tree_node_t *a = wb_atspi_tree_new("btn", WB_ATSPI_ROLE_PUSH_BUTTON);
        wb_atspi_tree_node_t *b = wb_atspi_tree_new("lbl", WB_ATSPI_ROLE_LABEL);
        wb_atspi_tree_node_t *a1 = wb_atspi_tree_new("inner", WB_ATSPI_ROLE_LABEL);
        CHECK(wb_atspi_tree_add(root, a) == 0, "a child is added");
        CHECK(wb_atspi_tree_add(root, b) == 0, "another child");
        CHECK(wb_atspi_tree_add(a, a1) == 0, "a grandchild");
        CHECK(root->child_count == 2, "the root has two children");

        walk_rec_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.abort_after = -1;
        CHECK(wb_atspi_walk(root, rec_fn, &rec, 0, 0, &r) == NULL, "the tree walks");
        CHECK(!r.hit_cycle && !r.hit_depth_limit && !r.hit_node_limit,
              "with nothing truncated");
        CHECK(r.visited == 4, "four nodes visited");
        CHECK(strcmp(rec.order[0], "win") == 0, "the root is first");
        CHECK(strcmp(rec.order[1], "btn") == 0, "then the first child");
        CHECK(strcmp(rec.order[2], "inner") == 0,
              "then its child (DEPTH-FIRST, in index order)");
        CHECK(strcmp(rec.order[3], "lbl") == 0, "then the second child");
        CHECK(rec.depths[0] == 0 && rec.depths[1] == 1 && rec.depths[2] == 2 &&
              rec.depths[3] == 1, "and the depths are correct");
        CHECK(rec.indices[1] == 0 && rec.indices[3] == 1,
              "and the child indices are correct");

        /* a second walk of the SAME tree must work (the marks are a serial) */
        memset(&rec, 0, sizeof(rec));
        rec.abort_after = -1;
        CHECK(wb_atspi_walk(root, rec_fn, &rec, 0, 0, &r) == NULL,
              "a SECOND walk of the same tree works");
        CHECK(r.visited == 4, "and visits all four again");

        wb_atspi_tree_free(root);
        wb_atspi_tree_free(NULL);
        CHECK(wb_atspi_tree_add(NULL, a) == -1, "a null parent is refused");
        CHECK(wb_atspi_tree_add(a, NULL) == -1, "a null child is refused");
    }

    /* -- a cycle is DETECTED, not walked forever -- */
    {
        wb_atspi_tree_node_t *root = wb_atspi_tree_new("root", WB_ATSPI_ROLE_WINDOW);
        wb_atspi_tree_node_t *child = wb_atspi_tree_new("child", WB_ATSPI_ROLE_SECTION);
        wb_atspi_tree_add(root, child);
        /* a malformed app makes the child point back at the root */
        wb_atspi_tree_add(child, root);

        CHECK(wb_atspi_walk(root, rec_fn, NULL, 0, 0, &r) != NULL,
              "a CYCLE is detected");
        CHECK(r.hit_cycle, "and flagged as a cycle");
        CHECK(r.cycle_name && strcmp(r.cycle_name, "root") == 0,
              "reported at the node where it closes");
    }
    {
        /* a self-cycle */
        wb_atspi_tree_node_t *n = wb_atspi_tree_new("self", WB_ATSPI_ROLE_SECTION);
        wb_atspi_tree_add(n, n);
        CHECK(wb_atspi_walk(n, rec_fn, NULL, 0, 0, &r) != NULL,
              "a SELF-cycle is detected");
        CHECK(r.hit_cycle, "and flagged");
    }

    /* -- depth is capped -- */
    {
        wb_atspi_tree_node_t *root = wb_atspi_tree_new("d0", WB_ATSPI_ROLE_WINDOW);
        wb_atspi_tree_node_t *cur = root;
        wb_atspi_tree_node_t *at_depth4 = NULL;
        for (int i = 1; i < 10; i++) {
            char nm[32];
            snprintf(nm, sizeof(nm), "d%d", i);
            wb_atspi_tree_node_t *c = wb_atspi_tree_new(nm, WB_ATSPI_ROLE_SECTION);
            wb_atspi_tree_add(cur, c);
            cur = c;
            if (i == 4) at_depth4 = c;    /* the first node past the limit */
        }
        walk_rec_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.abort_after = -1;
        CHECK(wb_atspi_walk(root, rec_fn, &rec, 3, 0, &r) == NULL,
              "a walk with a depth limit completes (it truncates, it does not fail)");
        CHECK(r.hit_depth_limit, "and flags the depth limit");
        CHECK(r.visited == 4, "visiting only depths 0..3");
        CHECK(strcmp(rec.order[3], "d3") == 0, "and stopping at depth 3");
        CHECK(at_depth4 && at_depth4->truncated,
              "the node AT the cut is marked truncated (not the whole subtree)");
        CHECK(!cur->truncated,
              "and the deeper nodes are never visited, let alone marked");
        wb_atspi_tree_free(root);
    }

    /* -- node count is capped -- */
    {
        wb_atspi_tree_node_t *root = wb_atspi_tree_new("wide", WB_ATSPI_ROLE_WINDOW);
        for (int i = 0; i < 20; i++) {
            char nm[32];
            snprintf(nm, sizeof(nm), "c%d", i);
            wb_atspi_tree_add(root, wb_atspi_tree_new(nm, WB_ATSPI_ROLE_LABEL));
        }
        walk_rec_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.abort_after = -1;
        CHECK(wb_atspi_walk(root, rec_fn, &rec, 0, 5, &r) == NULL,
              "a walk with a node limit completes");
        CHECK(r.hit_node_limit, "and flags the node limit");
        CHECK(r.visited == 5, "visiting exactly 5 nodes");
        wb_atspi_tree_free(root);
    }

    /* -- a NULL root is valid -- */
    {
        CHECK(wb_atspi_walk(NULL, rec_fn, NULL, 0, 0, &r) == NULL,
              "a NULL root is NOT an error (an empty tree is valid)");
        CHECK(r.visited == 0, "and visits nothing");
        CHECK(wb_atspi_walk(NULL, NULL, NULL, 0, 0, NULL) != NULL,
              "but a null result struct is refused");
    }

    /* -- the callback may abort -- */
    {
        wb_atspi_tree_node_t *root = wb_atspi_tree_new("a", WB_ATSPI_ROLE_WINDOW);
        wb_atspi_tree_add(root, wb_atspi_tree_new("b", WB_ATSPI_ROLE_LABEL));
        wb_atspi_tree_add(root, wb_atspi_tree_new("c", WB_ATSPI_ROLE_LABEL));
        walk_rec_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.abort_after = 1;          /* stop after the 2nd node */
        CHECK(wb_atspi_walk(root, rec_fn, &rec, 0, 0, &r) == NULL,
              "an aborting callback does not fail the walk");
        CHECK(r.aborted, "and is flagged as an abort");
        CHECK(r.visited == 2, "visiting exactly two nodes");
        wb_atspi_tree_free(root);
    }

    if (failures == 0) printf("ALL WUBU_ATSPI_TREE TESTS PASSED\n");
    else printf("%d WUBU_ATSPI_TREE FAILURES\n", failures);
    return failures ? 1 : 0;
}
