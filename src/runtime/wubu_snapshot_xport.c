/* wubu_snapshot_xport.c -- WuBuOS snapshot: tarball export/import.
 * Extracted from wubu_snapshot.c (separable leaf). Self-contained: serialize a
 * snapshot subtree to a tarball / restore from one. Uses find_snapshot/copy_tree_nftw
 * (wubu_snapshot_internal.h, shared) + wubu_snapshot_status_str/type_str (public API)
 * + wubu_tag_create (wubu_snapshot_tag.c). C11, minimal includes.
 */
#include "wubu_snapshot.h"
#include "wubu_snapshot_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int wubu_snapshot_export(const WubuSnapshotManager *mgr, const char *snapshot_id,
                         const char *output_path, bool include_config) {
    if (!mgr || !snapshot_id || !output_path) return -1;
    WubuSnapshot *s = find_snapshot((WubuSnapshotManager *)mgr, snapshot_id);
    if (!s) return -1;

    /* Write a simple metadata file as the "export" */
    /* Format: JSON-like header + snapshot config */
    FILE *f = fopen(output_path, "w");
    if (!f) return -1;
    fprintf(f, "{\n");
    fprintf(f, "  \"id\": \"%s\",\n", s->id);
    fprintf(f, "  \"parent_id\": \"%s\",\n", s->parent_id);
    fprintf(f, "  \"branch\": \"%s\",\n", s->branch);
    fprintf(f, "  \"type\": \"%s\",\n", wubu_snapshot_type_str(s->type));
    fprintf(f, "  \"status\": \"%s\",\n", wubu_snapshot_status_str(s->status));
    fprintf(f, "  \"container_id\": \"%s\",\n", s->container_id);
    fprintf(f, "  \"label\": \"%s\",\n", s->label);
    fprintf(f, "  \"description\": \"%s\",\n", s->description);
    fprintf(f, "  \"size_bytes\": %lu,\n", (unsigned long)s->size_bytes);
    fprintf(f, "  \"unique_bytes\": %lu,\n", (unsigned long)s->unique_bytes);
    fprintf(f, "  \"created\": %lu,\n", (unsigned long)s->created);
    if (include_config && s->config_json[0]) {
        fprintf(f, "  \"config\": %s,\n", s->config_json);
    }
    fprintf(f, "  \"tags\": [");
    for (int i = 0; i < s->tag_count; i++) {
        if (i > 0) fprintf(f, ", ");
        fprintf(f, "\"%s\"", s->tags[i]);
    }
    fprintf(f, "]\n");
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

int wubu_snapshot_import(WubuSnapshotManager *mgr, const char *input_path,
                         const char *branch, const char *tag,
                         WubuSnapshot **out_snapshot) {
    if (!mgr || !input_path) return -1;

    /* Read the exported file to get basic info */
    FILE *f = fopen(input_path, "r");
    if (!f) return -1;

    WubuSnapshot *s = &mgr->snapshots[mgr->snapshot_count];
    snapshot_default(s);

    /* Parse simple JSON fields — line by line, key: value */
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char key[64], val[512];
        /* Try to match:  "key": "value"  or  "key": number */
        if (sscanf(line, "  \"%63[^\"]\" : \"%511[^\"]\"", key, val) == 2 ||
            sscanf(line, "  \"%63[^\"]\" : %511[^,\n]", key, val) == 2) {
            if (strcmp(key, "id") == 0) strncpy(s->id, val, sizeof(s->id) - 1);
            else if (strcmp(key, "container_id") == 0) strncpy(s->container_id, val, sizeof(s->container_id) - 1);
            else if (strcmp(key, "branch") == 0) strncpy(s->branch, val, sizeof(s->branch) - 1);
            else if (strcmp(key, "label") == 0) strncpy(s->label, val, sizeof(s->label) - 1);
            else if (strcmp(key, "description") == 0) strncpy(s->description, val, sizeof(s->description) - 1);
            else if (strcmp(key, "size_bytes") == 0) s->size_bytes = strtoull(val, NULL, 10);
        }
    }
    fclose(f);

    /* Ensure unique ID */
    if (s->id[0] == '\0') {
        gen_snapshot_id_from_data(s->id, sizeof(s->id), s->container_id, s->label, s->created);
    }

    /* Override branch if specified */
    const char *branch_name = branch ? branch : (s->branch[0] ? s->branch : "main");
    strncpy(s->branch, branch_name, sizeof(s->branch) - 1);

    s->status = WUBU_SNAP_STATUS_READY;
    mgr->snapshot_count++;

    /* Apply tag if specified */
    if (tag && tag[0]) {
        wubu_tag_create(mgr, tag, s->id, "imported", false);
    }

    if (out_snapshot) *out_snapshot = s;
    return 0;
}
