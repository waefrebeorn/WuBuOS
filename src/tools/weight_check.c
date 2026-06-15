/*
 * weight_check.c  --  WuBuOS Vision Weight Verification Implementation
 *
 * Cell 051: Checks Moondream3 safetensors shards are present.
 */

#include "weight_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- API ----------------------------------------------------- */

int weight_shard_path(int index, char *buf, int bufsz) {
    if (index < 0 || index >= WEIGHT_SHARDS || !buf || bufsz <= 0) return -1;
    snprintf(buf, bufsz, "%s%s", WEIGHT_DIR, WEIGHT_SHARD_NAMES[index]);
    return 0;
}

uint64_t weight_validate_file(const char *path, uint64_t min_size) {
    if (!path) return 0;

    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    if (size < 0) return 0;
    uint64_t fsize = (uint64_t)size;

    if (fsize < min_size) return 0;
    return fsize;
}

int weight_check(weight_check_t *result) {
    if (!result) return -1;

    memset(result, 0, sizeof(*result));
    strncpy(result->dir, WEIGHT_DIR, WEIGHT_MAX_PATH - 1);

    result->all_present = 1;
    result->all_valid = 1;
    result->total_size = 0;

    /* Check each shard */
    for (int i = 0; i < WEIGHT_SHARDS; i++) {
        char path[WEIGHT_MAX_PATH];
        weight_shard_path(i, path, sizeof(path));

        uint64_t size = weight_validate_file(path, WEIGHT_SHARD_MIN_SIZE[i]);
        if (size > 0) {
            result->present[i] = 1;
            result->size[i] = size;
            result->total_size += size;
        } else {
            /* File missing or too small  --  check if it exists at all */
            FILE *f = fopen(path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                result->size[i] = (uint64_t)ftell(f);
                fclose(f);
                result->present[i] = 1;
                result->all_valid = 0;  /* present but wrong size */
            } else {
                result->present[i] = 0;
                result->all_present = 0;
                result->all_valid = 0;
            }
        }
    }

    /* Check index file */
    char idx_path[WEIGHT_MAX_PATH];
    snprintf(idx_path, sizeof(idx_path), "%smodel.safetensors", WEIGHT_DIR);
    FILE *idx = fopen(idx_path, "rb");
    if (idx) {
        result->index_present = 1;
        fclose(idx);
    } else {
        result->all_present = 0;
        result->all_valid = 0;
    }

    if (!result->all_present) result->all_valid = 0;

    return result->all_valid ? 0 : -1;
}
