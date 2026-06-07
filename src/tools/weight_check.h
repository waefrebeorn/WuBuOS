/*
 * weight_check.h — WuBuOS Vision Weight Verification Tool
 *
 * Cell 051: Verifies Moondream3 vision model weights are present
 * and valid. Checks for all 4 safetensors shards plus the
 * index JSON, validates file sizes, and optionally checksums.
 *
 * The weights live at /home/wubu/models/moondream3/ and are
 * consumed by wubu_vision_moondream.c for inference.
 *
 * All C11, no external deps.
 */

#ifndef WUBU_WEIGHT_CHECK_H
#define WUBU_WEIGHT_CHECK_H

#include <stdint.h>
#include <stddef.h>

/* ── Constants ─────────────────────────────────────────────── */

#define WEIGHT_DIR        "/home/wubu/models/moondream3/"
#define WEIGHT_SHARDS     4
#define WEIGHT_MAX_PATH   256

/* Expected shard names */
static const char *WEIGHT_SHARD_NAMES[4] = {
    "model-00001-of-00004.safetensors",
    "model-00002-of-00004.safetensors",
    "model-00003-of-00004.safetensors",
    "model-00004-of-00004.safetensors",
};

/* Expected sizes (approximate — actual may vary slightly) */
static const uint64_t WEIGHT_SHARD_MIN_SIZE[4] = {
    4900000000ULL,  /* ~4.9GB */
    4900000000ULL,
    4900000000ULL,
    4900000000ULL,
};

/* ── Weight Check Results ──────────────────────────────────── */

typedef struct {
    int      present[WEIGHT_SHARDS];  /* 1 = shard present */
    uint64_t size[WEIGHT_SHARDS];      /* File size in bytes */
    int      index_present;            /* 1 = model.safetensors index present */
    int      all_present;              /* 1 = all shards + index present */
    int      all_valid;                /* 1 = all present + sizes OK */
    uint64_t total_size;              /* Sum of all shard sizes */
    char     dir[WEIGHT_MAX_PATH];     /* Directory path */
} weight_check_t;

/* ── API ───────────────────────────────────────────────────── */

/* Check for Moondream3 vision weights.
 * Scans WEIGHT_DIR for the 4 shards + index.
 * Returns 0 if all present and valid, -1 otherwise. */
int  weight_check(weight_check_t *result);

/* Get the full path for a shard (0-3).
 * Returns 0 on success, -1 if index out of range. */
int  weight_shard_path(int index, char *buf, int bufsz);

/* Validate a single shard file: check it exists and size >= min_size.
 * Returns file size, or 0 if invalid. */
uint64_t weight_validate_file(const char *path, uint64_t min_size);

/* ── Vision Weights Extraction ─────────────────────────────── */

/* The C vision encoder (wubu_vision_moondream.c) needs:
 *   data/moondream3_vision_weights.bin   — extracted weights
 *   data/moondream3_vision_index.json    — weight index
 *
 * These are generated from the safetensors shards via
 * tools/dump_moondream3_weights.py. This check verifies
 * the source shards exist for that pipeline. */

#endif /* WUBU_WEIGHT_CHECK_H */
