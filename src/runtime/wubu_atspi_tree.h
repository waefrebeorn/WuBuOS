/*
 * wubu_atspi_tree.h -- AT-SPI2: the accessibility tree walk.
 *
 * A screen reader walks the accessibility tree to build its view of the screen.
 * A MALFORMED tree must not hang it, so the walk has hard guards:
 *
 *   * A CYCLE is detected, not walked forever.  A child that points back to an
 *     ancestor would spin a naive recursion until the stack dies -- and a
 *     screen reader that dies takes the user's only view of the screen with it.
 *   * DEPTH IS CAPPED.  A tree deeper than the limit is truncated and reported,
 *     never walked to exhaustion.
 *   * NODE COUNT IS CAPPED.  A pathologically wide tree is bounded the same
 *     way.
 *   * A NULL root is NOT an error: an empty tree is a valid tree, and a reader
 *     must be able to handle a window with no accessible children.
 *   * The callback may ABORT the walk by returning false -- a reader that has
 *     found what it is looking for should stop, not traverse the whole screen.
 *   * Traversal is DEPTH-FIRST with children in index order, because that is
 *     the order a user tabs through the interface.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_ATSPI_TREE_H
#define WUBU_ATSPI_TREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wubu_atspi.h"

#define WB_ATSPI_TREE_NAME_MAX 128

typedef struct wb_atspi_tree_node {
    char                          name[WB_ATSPI_TREE_NAME_MAX];
    wb_atspi_role_t               role;
    uint32_t                      child_count;
    struct wb_atspi_tree_node   **children;
    /* walk bookkeeping.  A SERIAL rather than a bool: a sticky 'visited' flag
     * would make a SECOND walk of the same tree look like a cycle, and the
     * marks cannot simply be cleared because a cyclic tree cannot be walked
     * to clear them. */
    uint32_t                      depth;
    uint32_t                      mark_id;
    bool                          truncated;
} wb_atspi_tree_node_t;

wb_atspi_tree_node_t *wb_atspi_tree_new(const char *name, wb_atspi_role_t role);
void wb_atspi_tree_free(wb_atspi_tree_node_t *n);

/* Append a child.  Returns -1 on a full array or a null node. */
int wb_atspi_tree_add(wb_atspi_tree_node_t *parent, wb_atspi_tree_node_t *child);

typedef struct {
    uint32_t max_depth;
    uint32_t max_nodes;
    uint32_t visited;          /* nodes actually visited */
    uint32_t truncated_at;     /* depth at which a cut happened, or 0 */
    bool     hit_cycle;
    bool     hit_depth_limit;
    bool     hit_node_limit;
    bool     aborted;          /* the callback returned false */
    uint32_t serial;           /* identifies this walk */
    const char *cycle_name;    /* the name where a cycle was detected */
} wb_atspi_walk_result_t;

/* Visit a node.  Return false to ABORT the walk. */
typedef bool (*wb_atspi_visit_fn)(const wb_atspi_tree_node_t *node,
                                  uint32_t depth, uint32_t index,
                                  void *user);

/* Walk the tree.  Returns NULL on success, else a static reason (a cycle or a
 * malformed tree).  The result struct is always filled when non-NULL. */
const char *wb_atspi_walk(wb_atspi_tree_node_t *root, wb_atspi_visit_fn fn,
                          void *user, uint32_t max_depth, uint32_t max_nodes,
                          wb_atspi_walk_result_t *out);

#endif /* WUBU_ATSPI_TREE_H */
