/*
 * wubu_image_manifest.h  --  WuBuOS Image Manifest API
 *
 * Extracted from wubu_image.c (2026-07-06).
 * C11 only. No god headers.
 */
#ifndef WUBU_IMAGE_MANIFEST_H
#define WUBU_IMAGE_MANIFEST_H

#include "wubu_image.h"

int wubu_manifest_compute_id(WubuImageManifest *manifest);
int wubu_manifest_to_json(const WubuImageManifest *manifest, char *out_json, size_t out_size);
int wubu_manifest_from_json(const char *json, WubuImageManifest *manifest);
int wubu_manifest_save(const WubuImageManifest *manifest, const char *path);
int wubu_manifest_load(const char *path, WubuImageManifest *manifest);

#endif /* WUBU_IMAGE_MANIFEST_H */