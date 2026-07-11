/* wubu_snapshot_gc.c -- WuBuOS snapshot: garbage collection + retention rules.
 * Extracted from wubu_snapshot.c (separable leaf). Self-contained: applies
 * WubuRetentionRule policies to prune snapshots. Uses find_snapshot/snapshot_now
 * (wubu_snapshot_internal.h, static inline) + wubu_snapshot_delete (public API).
 * C11, minimal includes.
 */
#include "wubu_snapshot.h"
#include "wubu_snapshot_internal.h"

#include <string.h>

int wubu_snapshot_gc(WubuSnapshotManager *mgr) {
    if (!mgr) return -1;
    int deleted = 0;
    uint64_t total_size = 0;

    /* First pass: calculate total size */
    for (int i = 0; i < mgr->snapshot_count; i++) {
        total_size += mgr->snapshots[i].size_bytes;
    }

    /* Apply retention rules */
    for (int r = 0; r < mgr->retention_rule_count; r++) {
        WubuRetentionRule *rule = &mgr->retention_rules[r];
        if (!rule->enabled) continue;

        switch (rule->type) {
        case WUBU_RETENTION_KEEP_LAST_N: {
            /* Keep only the last N snapshots per branch (or all) */
            for (int b = 0; b < mgr->branch_count; b++) {
                const char *target_branch = rule->branch[0] ? rule->branch : mgr->branches[b].name;
                int branch_count = 0;
                /* Count snapshots in this branch */
                for (int i = mgr->snapshot_count - 1; i >= 0; i--) {
                    if (strcmp(mgr->snapshots[i].branch, target_branch) == 0) {
                        branch_count++;
                        if (branch_count > rule->value &&
                            !mgr->snapshots[i].protected &&
                            mgr->snapshots[i].ref_count == 0) {
                            wubu_snapshot_delete(mgr, mgr->snapshots[i].id, false);
                            deleted++;
                        }
                    }
                }
            }
            break;
        }
        case WUBU_RETENTION_KEEP_DAYS: {
            uint64_t cutoff = snapshot_now() - (rule->value * 86400);
            for (int i = mgr->snapshot_count - 1; i >= 0; i--) {
                if (mgr->snapshots[i].created < cutoff &&
                    !mgr->snapshots[i].protected &&
                    mgr->snapshots[i].ref_count == 0) {
                    wubu_snapshot_delete(mgr, mgr->snapshots[i].id, false);
                    deleted++;
                }
            }
            break;
        }
        case WUBU_RETENTION_KEEP_TAGGED:
            /* Keep all tagged snapshots (mark protected) */
            for (int i = 0; i < mgr->snapshot_count; i++) {
                if (mgr->snapshots[i].tag_count > 0) {
                    mgr->snapshots[i].protected = true;
                }
            }
            break;
        case WUBU_RETENTION_KEEP_BRANCH_HEAD:
            /* Keep all branch heads */
            for (int b = 0; b < mgr->branch_count; b++) {
                if (mgr->branches[b].head_snapshot_id[0]) {
                    WubuSnapshot *s = find_snapshot(mgr, mgr->branches[b].head_snapshot_id);
                    if (s) s->protected = true;
                }
            }
            break;
        case WUBU_RETENTION_KEEP_PROTECTED:
            /* No-op: protected snapshots are already kept */
            break;
        case WUBU_RETENTION_MAX_SIZE: {
            /* Delete oldest unprotected snapshots until under limit */
            if (mgr->max_store_size == 0) break;
            /* Sort by age (oldest first) and delete */
            for (int i = mgr->snapshot_count - 1; i >= 0; i--) {
                if (total_size <= (uint64_t)rule->value) break;
                if (!mgr->snapshots[i].protected && mgr->snapshots[i].ref_count == 0) {
                    total_size -= mgr->snapshots[i].size_bytes;
                    wubu_snapshot_delete(mgr, mgr->snapshots[i].id, false);
                    deleted++;
                }
            }
            break;
        }
        }
    }

    (void)deleted;
    return 0;
}

int wubu_snapshot_gc_add_rule(WubuSnapshotManager *mgr, const WubuRetentionRule *rule) {
    if (!mgr || !rule) return -1;
    if (mgr->retention_rule_count >= WUBU_MAX_RETENTION_RULES) return -1;
    memcpy(&mgr->retention_rules[mgr->retention_rule_count], rule, sizeof(*rule));
    mgr->retention_rule_count++;
    return 0;
}

int wubu_snapshot_gc_remove_rule(WubuSnapshotManager *mgr, int rule_index) {
    if (!mgr) return -1;
    if (rule_index < 0 || rule_index >= mgr->retention_rule_count) return -1;
    memmove(&mgr->retention_rules[rule_index], &mgr->retention_rules[rule_index + 1],
            (mgr->retention_rule_count - rule_index - 1) * sizeof(WubuRetentionRule));
    mgr->retention_rule_count--;
    return 0;
}

int wubu_snapshot_gc_list_rules(WubuSnapshotManager *mgr, WubuRetentionRule *out_rules, int max) {
    if (!mgr || !out_rules || max <= 0) return 0;
    int count = (mgr->retention_rule_count < max) ? mgr->retention_rule_count : max;
    memcpy(out_rules, mgr->retention_rules, count * sizeof(WubuRetentionRule));
    return count;
}

/* -- Inspect ------------------------------------------------------ */

