/*
 * oci_runtime_spec.c  --  OCI Runtime Spec Operations
 * 
 * Extracted from wubu_oci.c (lines 1518-1612).
 */

#include "oci_internal.h"

/* -- OCI Runtime Spec ------------------------------------------------ */

int oci_runtime_spec_create(OciRuntimeSpec *spec, const void *manifest) {
    if (!spec) return -1;
    memset(spec, 0, sizeof(*spec));
    snprintf(spec->oci_version, sizeof(spec->oci_version), "1.0.2");
    snprintf(spec->process.cwd, sizeof(spec->process.cwd), "/");
    if (manifest) {
        const OciImageManifest *m = (const OciImageManifest *)manifest;
        if (m->config.digest[0]) {
            snprintf(spec->root.path, sizeof(spec->root.path), "/var/lib/wubi/oci");
            spec->root.readonly = false;
        }
    } else {
        snprintf(spec->root.path, sizeof(spec->root.path), "/var/lib/wubi/oci");
    }
    return 0;
}

void oci_runtime_spec_free(OciRuntimeSpec *spec) {
    (void)spec;
}

int oci_runtime_spec_to_json(const OciRuntimeSpec *spec, char *out_json, size_t out_size) {
    if (!spec || !out_json || out_size < 512) return -1;
    char *p = out_json;
    size_t rem = out_size;
    int n = snprintf(p, rem, "{\"ociVersion\":\"%s\",", spec->oci_version);
    if (n < 0) return -1;
    p += n; rem -= n;
    n = snprintf(p, rem, "\"process\":{\"args\":[");
    p += n; rem -= n;
    for (int i = 0; i < spec->process.args_count; i++) {
        n = snprintf(p, rem, "%s\"%s\"", i ? "," : "", spec->process.args[i]);
        p += n; rem -= n;
    }
    n = snprintf(p, rem, ",\"cwd\":\"%s\"},\"root\":{\"path\":\"%s\",\"readonly\":false}}",
                 spec->process.cwd, spec->root.path);
    if (n < 0 || (size_t)n >= rem) return -1;
    return 0;
}

int oci_runtime_spec_from_json(const char *json, OciRuntimeSpec *spec) {
    if (!json || !spec) return -1;
    memset(spec, 0, sizeof(*spec));
    oci_json_copy_string_value(json, "ociVersion", spec->oci_version, sizeof(spec->oci_version));
    oci_json_copy_string_value(json, "cwd", spec->process.cwd, sizeof(spec->process.cwd));
    const char *root = strstr(json, "\"root\"");
    if (root) {
        oci_json_copy_string_value(root, "path", spec->root.path, sizeof(spec->root.path));
    }
    const char *args = strstr(json, "\"args\"");
    if (args) {
        const char *bracket = strchr(args, '[');
        if (bracket) {
            const char *scan = bracket;
            while (spec->process.args_count < 32) {
                const char *q = strchr(scan, '"');
                if (!q || q > strchr(scan, ']')) break;
                const char *e = strchr(q + 1, '"');
                if (!e) break;
                size_t len = (size_t)(e - q - 1);
                if (len >= sizeof(spec->process.args[0])) len = sizeof(spec->process.args[0]) - 1;
                memcpy(spec->process.args[spec->process.args_count], q + 1, len);
                spec->process.args[spec->process.args_count][len] = 0;
                spec->process.args_count++;
                scan = e + 1;
            }
        }
    }
    return 0;
}

int oci_runtime_spec_validate(const OciRuntimeSpec *spec) {
    if (!spec) return -1;
    if (!spec->oci_version[0]) return -1;
    if (!spec->process.cwd[0]) return -1;
    return 0;
}