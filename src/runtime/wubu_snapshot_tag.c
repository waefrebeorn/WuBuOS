/* wubu_snapshot_tag.c -- Snapshot tag operations subsystem.
 *
 * Self-contained: tag create/delete/list. Uses find_snapshot/find_tag/
 * snapshot_now (static inline in wubu_snapshot_internal.h) and the
 * WubuSnapshotManager/WubuTag types (wubu_snapshot.h). Minimal includes.
 */

#include "wubu_snapshot_internal.h"

int wubu_tag_create(WubuSnapshotManager *mgr, const char *name, const char *snapshot_id,
                    const char *message, bool annotated) {
    if (!mgr || !name || !snapshot_id) return -1;
    /* Snapshot must exist */
    if (!find_snapshot(mgr, snapshot_id)) return -1;
    /* Update existing tag or create new */
    WubuTag *tag = find_tag(mgr, name);
    if (tag) {
        strncpy(tag->snapshot_id, snapshot_id, sizeof(tag->snapshot_id) - 1);
        tag->created = snapshot_now();
        if (message) strncpy(tag->message, message, sizeof(tag->message) - 1);
        tag->annotated = annotated;
        return 0;
    }
    if (mgr->tag_count >= WUBU_MAX_TAGS) return -1;
    tag = &mgr->tags[mgr->tag_count];
    memset(tag, 0, sizeof(*tag));
    strncpy(tag->name, name, sizeof(tag->name) - 1);
    strncpy(tag->snapshot_id, snapshot_id, sizeof(tag->snapshot_id) - 1);
    tag->created = snapshot_now();
    if (message) strncpy(tag->message, message, sizeof(tag->message) - 1);
    tag->annotated = annotated;
    mgr->tag_count++;
    return 0;
}

int wubu_tag_delete(WubuSnapshotManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    for (int i = 0; i < mgr->tag_count; i++) {
        if (strcmp(mgr->tags[i].name, name) == 0) {
            memmove(&mgr->tags[i], &mgr->tags[i + 1],
                    (mgr->tag_count - i - 1) * sizeof(WubuTag));
            mgr->tag_count--;
            return 0;
        }
    }
    return -1;
}

int wubu_tag_list(WubuSnapshotManager *mgr, WubuTag *out_tags, int max) {
    if (!mgr || !out_tags || max <= 0) return 0;
    int count = (mgr->tag_count < max) ? mgr->tag_count : max;
    memcpy(out_tags, mgr->tags, count * sizeof(WubuTag));
    return count;
}
