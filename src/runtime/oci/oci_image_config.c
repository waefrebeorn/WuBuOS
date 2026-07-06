/*
 * oci_image_config.c  --  OCI Image Config Operations
 * 
 * Extracted from wubu_oci.c (lines 520-768).
 */

#include "oci_internal.h"

/* -- OCI Image Config ------------------------------------------------ */

int oci_config_create(OciImageConfig *config, const void *wubu_manifest_ptr) {
    if (!config || !wubu_manifest_ptr) return -1;
    memset(config, 0, sizeof(OciImageConfig));

    const WubuImageManifest *wubu = (const WubuImageManifest *)wubu_manifest_ptr;

    time_t now = time(NULL);
    snprintf(config->created, sizeof(config->created), "%ld", now);
    strncpy(config->architecture, wubu_arch_name(wubu->arch), sizeof(config->architecture) - 1);
    strncpy(config->os, wubu_os_name(wubu->os), sizeof(config->os) - 1);

    if (wubu->entrypoint[0]) {
        strncpy(config->entrypoint[0], wubu->entrypoint, sizeof(config->entrypoint[0]) - 1);
        config->entrypoint_count = 1;
    }
    if (wubu->cmd[0]) {
        strncpy(config->cmd[0], wubu->cmd, sizeof(config->cmd[0]) - 1);
        config->cmd_count = 1;
    }
    if (wubu->workdir[0]) strncpy(config->working_dir, wubu->workdir, sizeof(config->working_dir) - 1);
    if (wubu->user[0]) strncpy(config->user, wubu->user, sizeof(config->user) - 1);

    for (int i = 0; i < wubu->port_count && i < 32; i++) {
        config->exposed_ports[i] = wubu->ports[i];
        config->exposed_port_count++;
    }
    for (int i = 0; i < wubu->volume_count && i < 32; i++) {
        strncpy(config->volumes[i], wubu->volumes[i], 255);
        config->volume_count++;
    }
    for (int i = 0; i < wubu->label_count && i < 32; i++) {
        strncpy(config->labels[i], wubu->labels[i], 255);
        config->label_count++;
    }

    config->rootfs.diff_id_count = 0;
    for (int i = 0; i < wubu->layer_count && i < 128; i++) {
        snprintf(config->rootfs.diff_ids[i], sizeof(config->rootfs.diff_ids[i]), "sha256:%s", wubu->layers[i].digest);
        config->rootfs.diff_id_count++;
    }
    strncpy(config->rootfs.type, "layers", sizeof(config->rootfs.type) - 1);

    config->stop_signal = 15;

    return 0;
}

int oci_config_to_json(const OciImageConfig *config, char *out_json, size_t out_size) {
    if (!config || !out_json || out_size < 1024) return -1;

    char *p = out_json;
    size_t rem = out_size;
    int n;

    n = snprintf(p, rem, "{");
    p += n; rem -= n;

    n = snprintf(p, rem, "\"created\":\"%s\",", config->created);
    p += n; rem -= n;

    n = snprintf(p, rem, "\"architecture\":\"%s\",", config->architecture);
    p += n; rem -= n;

    n = snprintf(p, rem, "\"os\":\"%s\",", config->os);
    p += n; rem -= n;

    if (config->entrypoint_count > 0) {
        n = snprintf(p, rem, "\"Entrypoint\":[\"%s\"]", config->entrypoint[0]);
    } else {
        n = snprintf(p, rem, "\"Entrypoint\":null");
    }
    p += n; rem -= n;
    n = snprintf(p, rem, ","); p += n; rem -= n;

    if (config->cmd_count > 0) {
        n = snprintf(p, rem, "\"Cmd\":[\"%s\"]", config->cmd[0]);
    } else {
        n = snprintf(p, rem, "\"Cmd\":null");
    }
    p += n; rem -= n;
    n = snprintf(p, rem, ","); p += n; rem -= n;

    n = snprintf(p, rem, "\"WorkingDir\":\"%s\",", config->working_dir);
    p += n; rem -= n;

    n = snprintf(p, rem, "\"User\":\"%s\",", config->user);
    p += n; rem -= n;

    n = snprintf(p, rem, "\"Env\":[");
    p += n; rem -= n;
    for (int i = 0; i < config->env_count; i++) {
        n = snprintf(p, rem, "%s\"%s\"", i > 0 ? "," : "", config->env[i]);
        p += n; rem -= n;
    }
    n = snprintf(p, rem, "],");
    p += n; rem -= n;

    n = snprintf(p, rem, "\"ExposedPorts\":{");
    p += n; rem -= n;
    for (int i = 0; i < config->exposed_port_count; i++) {
        n = snprintf(p, rem, "%s\"%d/tcp\":{}", i > 0 ? "," : "", config->exposed_ports[i]);
        p += n; rem -= n;
    }
    n = snprintf(p, rem, "},");
    p += n; rem -= n;

    n = snprintf(p, rem, "\"Volumes\":{");
    p += n; rem -= n;
    for (int i = 0; i < config->volume_count; i++) {
        n = snprintf(p, rem, "%s\"%s\":{}", i > 0 ? "," : "", config->volumes[i]);
        p += n; rem -= n;
    }
    n = snprintf(p, rem, "},");
    p += n; rem -= n;

    n = snprintf(p, rem, "\"Labels\":{");
    p += n; rem -= n;
    for (int i = 0; i < config->label_count; i++) {
        char *eq = strchr(config->labels[i], '=');
        if (eq) {
            *eq = '\0';
            n = snprintf(p, rem, "%s\"%s\":\"%s\"", i > 0 ? "," : "", config->labels[i], eq + 1);
            *eq = '=';
            p += n; rem -= n;
        }
    }

    n = snprintf(p, rem, "},");
    p += n; rem -= n;

    n = snprintf(p, rem, "\"rootfs\":{\"type\":\"%s\",\"diff_ids\":[", config->rootfs.type);
    p += n; rem -= n;
    for (int i = 0; i < config->rootfs.diff_id_count; i++) {
        n = snprintf(p, rem, "%s\"%s\"", i > 0 ? "," : "", config->rootfs.diff_ids[i]);
        p += n; rem -= n;
    }
    n = snprintf(p, rem, "]}");
    if (n >= 0) { p += n; rem -= n; }

    n = snprintf(p, rem, "}");
    if (n >= 0) { p += n; rem -= n; }

    return 0;
}

int oci_config_from_json(const char *json, OciImageConfig *config) {
    if (!json || !config) return -1;
    memset(config, 0, sizeof(*config));

    oci_json_copy_string_value(json, "architecture", config->architecture, sizeof(config->architecture));
    oci_json_copy_string_value(json, "os", config->os, sizeof(config->os));
    oci_json_copy_string_value(json, "created", config->created, sizeof(config->created));
    oci_json_copy_string_value(json, "WorkingDir", config->working_dir, sizeof(config->working_dir));
    oci_json_copy_string_value(json, "User", config->user, sizeof(config->user));

    /* Entrypoint / Cmd arrays */
    const char *scan = json;
    while (config->entrypoint_count < 16) {
        const char *start = strstr(scan, "\"Entrypoint\"");
        if (!start) break;
        const char *bracket = strchr(start, '[');
        if (!bracket) break;
        const char *q = strchr(bracket, '"');
        if (!q || q > strchr(bracket, ']')) break;
        const char *end = strchr(q + 1, '"');
        if (!end) break;
        size_t len = (size_t)(end - q - 1);
        if (len >= sizeof(config->entrypoint[0])) len = sizeof(config->entrypoint[0]) - 1;
        memcpy(config->entrypoint[config->entrypoint_count], q + 1, len);
        config->entrypoint[config->entrypoint_count][len] = 0;
        config->entrypoint_count++;
        scan = end + 1;
    }
    scan = json;
    while (config->cmd_count < 16) {
        const char *start = strstr(scan, "\"Cmd\"");
        if (!start) break;
        const char *bracket = strchr(start, '[');
        if (!bracket) break;
        const char *q = strchr(bracket, '"');
        if (!q || q > strchr(bracket, ']')) break;
        const char *end = strchr(q + 1, '"');
        if (!end) break;
        size_t len = (size_t)(end - q - 1);
        if (len >= sizeof(config->cmd[0])) len = sizeof(config->cmd[0]) - 1;
        memcpy(config->cmd[config->cmd_count], q + 1, len);
        config->cmd[config->cmd_count][len] = 0;
        config->cmd_count++;
        scan = end + 1;
    }

    /* Env */
    const char *env = strstr(json, "\"Env\"");
    if (env) {
        const char *bracket = strchr(env, '[');
        if (bracket) {
            const char *scan = bracket;
            while (config->env_count < OCI_MAX_ENV) {
                const char *q = strchr(scan, '"');
                if (!q || q > strchr(scan, ']')) break;
                const char *end = strchr(q + 1, '"');
                if (!end) break;
                size_t len = (size_t)(end - q - 1);
                if (len >= sizeof(config->env[0])) len = sizeof(config->env[0]) - 1;
                memcpy(config->env[config->env_count], q + 1, len);
                config->env[config->env_count][len] = 0;
                config->env_count++;
                scan = end + 1;
            }
        }
    }

    /* ExposedPorts */
    const char *ports = strstr(json, "\"ExposedPorts\"");
    if (ports) {
        const char *bracket = strchr(ports, '{');
        if (bracket) {
            const char *scan = bracket;
            while (config->exposed_port_count < OCI_MAX_PORTS) {
                const char *q = strchr(scan, '"');
                if (!q || q > strchr(scan, '}')) break;
                const char *end = strchr(q + 1, '"');
                if (!end) break;
                size_t len = (size_t)(end - q - 1);
                char port_str[32];
                if (len >= sizeof(port_str)) len = sizeof(port_str) - 1;
                memcpy(port_str, q + 1, len);
                port_str[len] = 0;
                char *slash = strchr(port_str, '/');
                if (slash) *slash = 0;
                config->exposed_ports[config->exposed_port_count++] = atoi(port_str);
                scan = end + 1;
            }
        }
    }

    config->stop_signal = 15;
    return 0;
}

int oci_config_compute_digest(const OciImageConfig *config, char *out_digest, size_t out_size) {
    char json[16384];
    if (oci_config_to_json(config, json, sizeof(json)) < 0) return -1;
    sha256_digest(json, strlen(json), out_digest);
    return 0;
}