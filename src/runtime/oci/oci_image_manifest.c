/*
 * oci_image_manifest.c  --  OCI Image Manifest Operations
 * 
 * Extracted from wubu_oci.c (lines 770-876).
 */

#include "oci_internal.h"

/* -- OCI Image Manifest ---------------------------------------------- */

int oci_manifest_create(OciImageManifest *manifest, const void *wubu_manifest_ptr) {
    if (!manifest || !wubu_manifest_ptr) return -1;
    memset(manifest, 0, sizeof(OciImageManifest));

    const WubuImageManifest *wubu = (const WubuImageManifest *)wubu_manifest_ptr;

    manifest->schema_version = 2;
    strncpy(manifest->media_type, oci_media_type_image_manifest_v2(), sizeof(manifest->media_type) - 1);

    /* Create config first to get digest */
    OciImageConfig config;
    oci_config_create(&config, wubu);

    char config_json[16384];
    oci_config_to_json(&config, config_json, sizeof(config_json));

    char config_digest[65];
    sha256_digest(config_json, strlen(config_json), config_digest);

    oci_create_descriptor(&manifest->config, oci_media_type_image_config_v1(),
                          strlen(config_json), config_digest);

    manifest->layer_count = wubu->layer_count;
    for (int i = 0; i < wubu->layer_count && i < OCI_MAX_LAYERS; i++) {
        oci_create_descriptor(&manifest->layers[i], oci_media_type_layer_v1_gzip(),
                              wubu->layers[i].size, wubu->layers[i].digest);
    }

    return 0;
}

int oci_manifest_to_json(const OciImageManifest *manifest, char *out_json, size_t out_size) {
    if (!manifest || !out_json || out_size < 4096) return -1;

    char *p = out_json;
    size_t rem = out_size;
    int n;

    n = snprintf(p, rem, "{\"schemaVersion\":2,\"mediaType\":\"%s\",", manifest->media_type);
    p += n; rem -= n;

    n = snprintf(p, rem, "\"config\":{\"mediaType\":\"%s\",\"size\":%lu,\"digest\":\"%s\"},",
                 manifest->config.media_type, manifest->config.size, manifest->config.digest);
    p += n; rem -= n;

    n = snprintf(p, rem, "\"layers\":[");
    p += n; rem -= n;

    for (int i = 0; i < manifest->layer_count; i++) {
        n = snprintf(p, rem, "%s{\"mediaType\":\"%s\",\"size\":%lu,\"digest\":\"%s\"}",
                     i > 0 ? "," : "",
                     manifest->layers[i].media_type,
                     manifest->layers[i].size,
                     manifest->layers[i].digest);
        p += n; rem -= n;
    }

    n = snprintf(p, rem, "]}");
    if (n >= 0) { p += n; rem -= n; }

    return 0;
}

int oci_manifest_from_json(const char *json, OciImageManifest *manifest) {
    if (!json || !manifest) return -1;
    memset(manifest, 0, sizeof(*manifest));
    manifest->schema_version = 2;

    oci_json_copy_string_value(json, "mediaType", manifest->media_type, sizeof(manifest->media_type));
    if (!manifest->media_type[0])
        strncpy(manifest->media_type, oci_media_type_image_manifest_v2(), sizeof(manifest->media_type) - 1);

    const char *config_start = strstr(json, "\"config\"");
    if (config_start) {
        oci_json_copy_string_value(config_start, "mediaType", manifest->config.media_type, sizeof(manifest->config.media_type));
        manifest->config.size = (uint64_t)oci_json_find_int(config_start, "size");
        oci_json_copy_string_value(config_start, "digest", manifest->config.digest, sizeof(manifest->config.digest));
    }

    const char *layers = strstr(json, "\"layers\"");
    if (layers) {
        const char *start = strchr(layers, '[');
        if (start) {
            const char *scan = start;
            while (manifest->layer_count < OCI_MAX_LAYERS) {
                const char *layer_start = strstr(scan, "\"mediaType\"");
                if (!layer_start || layer_start > strchr(scan, ']')) break;
                OciDescriptor *layer = &manifest->layers[manifest->layer_count];
                oci_json_copy_string_value(layer_start, "mediaType", layer->media_type, sizeof(layer->media_type));
                layer->size = (uint64_t)oci_json_find_int(layer_start, "size");
                oci_json_copy_string_value(layer_start, "digest", layer->digest, sizeof(layer->digest));
                manifest->layer_count++;
                scan = layer_start + 1;
            }
        }
    }
    return 0;
}

int oci_manifest_compute_digest(const OciImageManifest *manifest, char *out_digest, size_t out_size) {
    char json[32768];
    if (oci_manifest_to_json(manifest, json, sizeof(json)) < 0) return -1;
    sha256_digest(json, strlen(json), out_digest);
    return 0;
}