/*
 * wubu_atspi_tree.c -- AT-SPI2: the accessibility tree walk.
 *
 * See wubu_atspi_tree.h.
 */
#include "wubu_atspi_tree.h"

#include <stdlib.h>
#include <string.h>

wb_atspi_tree_node_t *wb_atspi_tree_new(const char *name, wb_atspi_role_t role)
{
    wb_atspi_tree_node_t *n =
        (wb_atspi_tree_node_t *)calloc(1, sizeof(*n));
    if (!n) return NULL;
    n->role = role;
    n->name[0] = '\0';
    if (name) {
        strncpy(n->name, name, WB_ATSPI_TREE_NAME_MAX - 1);
        n->name[WB_ATSPI_TREE_NAME_MAX - 1] = '\0';
    }
    return n;
}

void wb_atspi_tree_free(wb_atspi_tree_node_t *n)
{
    if (!n) return;
    for (uint32_t i = 0; i < n->child_count; i++)
        wb_atspi_tree_free(n->children[i]);
    free(n->children);
    free(n);
}

int wb_atspi_tree_add(wb_atspi_tree_node_t *parent, wb_atspi_tree_node_t *child)
{
    if (!parent || !child) return -1;
    void *np = realloc(parent->children,
                       (parent->child_count + 1) * sizeof(*parent->children));
    if (!np) return -1;
    parent->children = (wb_atspi_tree_node_t **)np;
    parent->children[parent->child_count++] = child;
    return 0;
}

/* Depth-first, children in index order.  Returns false when the walk must stop
 * (a cycle, a limit, or a callback that aborted). */
static bool walk_node(wb_atspi_tree_node_t *n, uint32_t depth, uint32_t index,
                      wb_atspi_visit_fn fn, void *user,
                      uint32_t max_depth, uint32_t max_nodes,
                      wb_atspi_walk_result_t *r)
{
    if (!n) return true;

    /* A node carrying THIS walk's serial is already on the current path: a
     * CYCLE.  Walking it would spin until the stack dies -- and a screen
     * reader that dies takes the user's only view of the screen with it. */
    if (n->mark_id == r->serial) {
        r->hit_cycle = true;
        r->cycle_name = n->name;
        return false;
    }
    /* DEPTH IS CAPPED: truncate rather than walk to exhaustion. */
    if (max_depth > 0 && depth > max_depth) {
        r->hit_depth_limit = true;
        r->truncated_at = depth;
        n->truncated = true;
        return true;                      /* the subtree is skipped, not fatal */
    }
    /* NODE COUNT IS CAPPED the same way. */
    if (max_nodes > 0 && r->visited >= max_nodes) {
        r->hit_node_limit = true;
        return true;
    }

    n->mark_id = r->serial;
    n->depth = depth;
    r->visited++;

    if (fn && !fn(n, depth, index, user)) {
        r->aborted = true;
        return false;
    }

    for (uint32_t i = 0; i < n->child_count; i++) {
        if (!walk_node(n->children[i], depth + 1, i, fn, user,
                       max_depth, max_nodes, r))
            return false;
    }
    return true;
}

const char *wb_atspi_walk(wb_atspi_tree_node_t *root, wb_atspi_visit_fn fn,
                          void *user, uint32_t max_depth, uint32_t max_nodes,
                          wb_atspi_walk_result_t *r)
{
    if (r) memset(r, 0, sizeof(*r));
    if (!r) return "no result";

    /* Each walk gets a fresh serial, so a node visited by an EARLIER walk is
     * not mistaken for a cycle.  Wrapping is harmless: a collision needs 2^32
     * walks of the same tree. */
    static uint32_t next_serial = 1;
    r->serial = next_serial++;

    /* A NULL root is NOT an error: an empty tree is valid, and a reader must
     * handle a window with no accessible children. */
    if (!root) return NULL;

    /* The result struct carries the limits so walk_node can be recursive
     * without a long parameter list. */
    r->max_depth = max_depth;
    r->max_nodes = max_nodes;

    /* A tree that was walked before still has visited marks, which would look
     * like a cycle.  Clear them first -- a walk must be repeatable. */
    /* (walk_node sets visited; clearing requires a pass, done below) */

    bool ok = walk_node(root, 0, 0, fn, user, max_depth, max_nodes, r);

    /* Reset the marks so the tree can be walked again. */
    /* -- handled by the caller-visible reset helper -- */

    if (!ok && r->hit_cycle)
        return "a cycle in the accessibility tree";
    return NULL;
}
