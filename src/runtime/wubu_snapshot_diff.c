/* wubu_snapshot_diff.c -- Snapshot diff subsystem (self-contained).
 *
 * wubu_snapshot_diff: textual diff between two snapshots. Uses find_snapshot
 * + wubu_snapshot_type_str (wubu_snapshot_internal.h). Minimal includes.
 */

#include "wubu_snapshot.h"
#include "wubu_snapshot_internal.h"

int wubu_snapshot_diff(WubuSnapshotManager *mgr, const char *snapshot_id1,
                       const char *snapshot_id2, char *out_diff, size_t out_size) {
    if (!mgr || !snapshot_id1 || !snapshot_id2 || !out_diff || out_size == 0) return -1;
    WubuSnapshot *s1 = find_snapshot(mgr, snapshot_id1);
    WubuSnapshot *s2 = find_snapshot(mgr, snapshot_id2);
    if (!s1 || !s2) return -1;

    /* Simplified diff: compare upper directories */
    /* A real implementation would walk both directory trees */
    int n = snprintf(out_diff, out_size,
        "diff --snapshot %s %s\n"
        "--- %s (%s, %lu bytes)\n"
        "+++ %s (%s, %lu bytes)\n"
        "@@ branch: %s -> %s, size: %lu -> %lu, type: %s -> %s @@\n",
        snapshot_id1, snapshot_id2,
        s1->label, wubu_snapshot_type_str(s1->type), (unsigned long)s1->size_bytes,
        s2->label, wubu_snapshot_type_str(s2->type), (unsigned long)s2->size_bytes,
        s1->branch, s2->branch,
        (unsigned long)s1->size_bytes, (unsigned long)s2->size_bytes,
        wubu_snapshot_type_str(s1->type), wubu_snapshot_type_str(s2->type));
    (void)mgr;
    return (n >= 0) ? 0 : -1;
}
