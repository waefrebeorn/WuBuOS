/*
 * oci_image_index.c  --  OCI Image Index (Multi-arch) Operations
 * 
 * Extracted from wubu_oci.c (lines 878-981).
 */

#include "oci_internal.h"

/* -- OCI Image Index ------------------------------------------------- */

int oci_index_create(OciImageIndex *index) {
    if (!index) return -1;
    memset(index, 0, sizeof(OciImageIndex));
    index->schema_version = 2;
    strncpy(index->media_type, oci_media_type_image_index_v1(), sizeof(index->media_type) - 1);
    return 0;
}

int oci_index_add_manifest(OciImageIndex *index, const OciDescriptor *desc, const OciPlatform *platform) {
    if (!index || !desc || index->manifest_count >= 16) return -1;
    index->manifests[index->manifest_count] = *desc;
    if (platform) {
        if (platform->architecture[0]) strncpy(index->platforms[index->manifest_count].architecture, platform->architecture, 31);
        if (platform->os[0]) strncpy(index->platforms[index->manifest_count].os, platform->os, 31);
        if (platform->variant[0]) strncpy(index->platforms[index->manifest_count].variant, platform->variant, 31);
    }
    index->manifest_count++;
    return 0;
}

int oci_index_to_json(const OciImageIndex *index, char *out_json, size_t out_size) {
    if (!index || !out_json || out_size < 4096) return -1;

    char *p = out_json;
    size_t rem = out_size;
    int n;

    n = snprintf(p, rem, "{\"schemaVersion\":2,\"mediaType\":\"%s\",\"manifests\":[", index->media_type);
    p += n; rem -= n;

    for (int i = 0; i < index->manifest_count; i++) {
        const OciDescriptor *d = &index->manifests[i];
        const OciPlatform *pf = &index->platforms[i];

        n = snprintf(p, rem, "%s{\"mediaType\":\"%s\",\"size\":%lu,\"digest\":\"%s\"",
                     i > 0 ? "," : "", d->media_type, d->size, d->digest);
        p += n; rem -= n;

        if (pf->architecture[0]) {
            n = snprintf(p, rem, ",\"platform\":{\"architecture\":\"%s\",\"os\":\"%s\"",
                         pf->architecture, pf->os);
            p += n; rem -= n;
            if (pf->variant[0]) {
                n = snprintf(p, rem, ",\"variant\":\"%s\"", pf->variant);
                p += n; rem -= n;
            }
            n = snprintf(p, rem, "}");
            p += n; rem -= n;
        }
        n = snprintf(p, rem, "}");
        p += n; rem -= n;
    }

    n = snprintf(p, rem, "]}");
    if (n >= 0) { p += n; rem -= n; }

    return 0;
}

int oci_index_from_json(const char *json, OciImageIndex *index) {
    if (!json || !index) return -1;
    memset(index, 0, sizeof(*index));
    index->schema_version = 2;

    oci_json_copy_string_value(json, "mediaType", index->media_type, sizeof(index->media_type));
    if (!index->media_type[0])
        strncpy(index->media_type, oci_media_type_image_index_v1(), sizeof(index->media_type) - 1);

    const char *manifests = strstr(json, "\"manifests\"");
    if (manifests) {
        const char *bracket = strchr(manifests, '[');
        if (bracket) {
            const char *scan = bracket;
            while (index->manifest_count < 16) {
                const char *mt = strstr(scan, "\"mediaType\"");
                if (!mt || mt > strchr(scan, ']')) break;
                oci_json_copy_string_value(mt, "mediaType", index->manifests[index->manifest_count].media_type,
                                           sizeof(index->manifests[0].media_type));
                index->manifests[index->manifest_count].size = (uint64_t)oci_json_find_int(mt, "size");
                oci_json_copy_string_value(mt, "digest", index->manifests[index->manifest_count].digest,
                                           sizeof(index->manifests[0].digest));

                /* Parse platform if present */
                const char *platform = strstr(mt, "\"platform\"");
                if (platform && platform < strchr(scan, '}')) {
                    oci_json_copy_string_value(platform, "architecture",
                                               index->platforms[index->manifest_count].architecture, 31);
                    oci_json_copy_string_value(platform, "os",
                                               index->platforms[index->manifest_count].os, 31);
                    oci_json_copy_string_value(platform, "variant",
                                               index->platforms[index->manifest_count].variant, 31);
                }

                index->manifest_count++;
                scan = mt + 1;
            }
        }
    }
    return 0;
}