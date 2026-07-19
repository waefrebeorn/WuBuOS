/*
 * wubu_cap_revoke.c -- WuBuOS capability core revocation cascade.
 * Ported from GrahaOS kernel/cap/revoke.c (klog/audit dropped for host).
 */
#include "wubu_cap_internal.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#define WUBU_CAP_REVOKE_CASCADE_MAX 1024

/* BFS walk of the descendant tree; bump generation on every non-immortal
 * descendant. Returns number of descendants invalidated (excludes root). */
int wubu_cap_revoke_cascade(uint32_t root_idx) {
    if (root_idx == 0 || root_idx >= WUBU_CAP_OBJECT_CAPACITY) return 0;
    uint32_t queue[WUBU_CAP_REVOKE_CASCADE_MAX];
    int head = 0, tail = 0;
    bool truncated = false;

    wubu_cap_lock_acquire();
    wubu_cap_object_t *root = g_wubu_cap_ptrs[root_idx];
    if (!root) { wubu_cap_lock_release(); return 0; }
    uint32_t cur = root->first_child_idx;
    while (cur != WUBU_CAP_IDX_NONE && tail < WUBU_CAP_REVOKE_CASCADE_MAX) {
        queue[tail++] = cur;
        wubu_cap_object_t *c = g_wubu_cap_ptrs[cur];
        if (!c) break;
        cur = c->next_sibling_idx;
    }
    if (cur != WUBU_CAP_IDX_NONE) truncated = true;
    wubu_cap_lock_release();

    int count = 0;
    while (head < tail) {
        uint32_t idx = queue[head++];
        wubu_cap_lock_acquire();
        wubu_cap_object_t *obj = g_wubu_cap_ptrs[idx];
        if (!obj) { wubu_cap_lock_release(); continue; }
        if (obj->flags & WUBU_CAP_FLAG_IMMORTAL) { wubu_cap_lock_release(); continue; }
        if (!obj->deleted) { obj->deleted = 1; obj->generation++; count++; }

        uint32_t c = obj->first_child_idx;
        while (c != WUBU_CAP_IDX_NONE) {
            if (tail >= WUBU_CAP_REVOKE_CASCADE_MAX) { truncated = true; break; }
            queue[tail++] = c;
            wubu_cap_object_t *cobj = g_wubu_cap_ptrs[c];
            if (!cobj) break;
            c = cobj->next_sibling_idx;
        }
        wubu_cap_lock_release();
    }

    if (truncated) {
        wubu_cap_lock_acquire();
        wubu_cap_object_t *root2 = g_wubu_cap_ptrs[root_idx];
        if (root2) root2->flags |= WUBU_CAP_FLAG_CASCADE_TRUNCATED;
        wubu_cap_lock_release();
    }
    return count;
}
