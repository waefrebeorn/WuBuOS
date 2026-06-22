/*
 * wubu_oci.c  --  WuBuOS OCI (Open Container Initiative) Compatibility Layer
 *
 * Phase 7: Minimal OCI image spec v1.0+ compatibility
 * - Basic OCI image manifest v2
 * - Image config
 * - Blob store (SHA256 content-addressable)
 * - Convert .wubu to OCI layout
 * - Registry auth stubs
 * - Runtime spec stubs
 */

#include "wubu_oci.h"
#include "wubu_image.h"
#include "wubu_container.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>

/* -- Internal Helpers --------------------------------------------- */

#define OCI_BLOB_DIR    "/var/lib/wubu/oci/blobs/sha256"
#define OCI_LAYOUT_FILE "oci-layout"

/* Forward declared structs - define here for implementation */
struct OciRegistryClient {
    char registry[256];
    char auth_token[512];
    char username[128];
    char password[128];
};

static void sha256_digest(const void *data, size_t size, char *out_hex) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
    uint32_t h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;
    uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t w[64];
    size_t i;
    for (i = 0; i < size; i++) {
        w[i / 4] = (w[i / 4] << 8) | p[i];
    }
    for (i = size / 4; i < 16; i++) w[i] = 0;
    w[size / 4] |= (uint32_t)1 << (24 - (size % 4) * 8);
    w[15] |= (uint32_t)(size * 8);
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ((w[i-15] >> 7) | (w[i-15] << 25)) ^ ((w[i-15] >> 18) | (w[i-15] << 14)) ^ (w[i-15] >> 3);
        uint32_t s1 = ((w[i-2] >> 17) | (w[i-2] << 15)) ^ ((w[i-2] >> 19) | (w[i-2] << 13)) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
    for (i = 0; i < 64; i++) {
        uint32_t S1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
    }
    uint32_t vals[8] = {h0+a,h1+b,h2+c,h3+d,h4+e,h5+f,h6+g,h7+h};
    for (i = 0; i < 8; i++) {
        sprintf(out_hex + i * 8, "%08x", vals[i]);
    }
    out_hex[64] = 0;
}

static void sha256_file(const char *path, char *out_hex) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { out_hex[0] = 0; return; }
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); out_hex[0] = 0; return; }
    if (st.st_size <= 0) { close(fd); sha256_digest(NULL, 0, out_hex); return; }
    void *buf = malloc((size_t)st.st_size);
    if (!buf) { close(fd); out_hex[0] = 0; return; }
    ssize_t n = read(fd, buf, (size_t)st.st_size);
    close(fd);
    if (n != st.st_size) { free(buf); out_hex[0] = 0; return; }
    sha256_digest(buf, (size_t)n, out_hex);
    free(buf);
}

static const char *json_find_string(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) return NULL;
    pos = strchr(pos + strlen(search), ':');
    if (!pos) return NULL;
    pos = strchr(pos, '"');
    if (!pos) return NULL;
    return pos + 1;
}

/* Copy a JSON string value (between quotes) into dst, null-terminated.
 * Returns the number of characters copied (excluding null terminator), or -1. */
static int json_copy_string_value(const char *json, const char *key, char *dst, size_t dst_size) {
    if (!dst || dst_size == 0) return -1;
    const char *start = json_find_string(json, key);
    if (!start) { dst[0] = '\0'; return -1; }
    const char *end = strchr(start, '"');
    if (!end) { dst[0] = '\0'; return -1; }
    size_t len = (size_t)(end - start);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
    return (int)len;
}

static int json_find_int(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) return 0;
    const char *colon = strchr(pos + strlen(search), ':');
    if (!colon) return 0;
    while (*colon && !isdigit((unsigned char)*colon) && *colon != '-') colon++;
    return atoi(colon);
}

static void ensure_oci_dirs(void) {
    mkdir(OCI_BLOB_DIR, 0755);
}

/* -- Media Type Helpers ------------------------------------------- */

const char *oci_media_type_image_manifest_v1(void) { return "application/vnd.oci.image.manifest.v1+json"; }
const char *oci_media_type_image_manifest_v2(void) { return "application/vnd.oci.image.manifest.v2+json"; }
const char *oci_media_type_image_index_v1(void) { return "application/vnd.oci.image.index.v1+json"; }
const char *oci_media_type_image_config_v1(void) { return "application/vnd.oci.image.config.v1+json"; }
const char *oci_media_type_layer_v1(void) { return "application/vnd.oci.image.layer.v1.tar"; }
const char *oci_media_type_layer_v1_gzip(void) { return "application/vnd.oci.image.layer.v1.tar+gzip"; }
const char *oci_media_type_layer_v1_zstd(void) { return "application/vnd.oci.image.layer.v1.tar+zstd"; }
const char *oci_media_type_empty_json(void) { return "application/vnd.oci.empty.v1+json"; }

/* -- Descriptor Operations ---------------------------------------- */

int oci_create_descriptor(OciDescriptor *desc, const char *media_type, uint64_t size, const char *sha256_digest) {
    if (!desc || !media_type || !sha256_digest) return -1;
    memset(desc, 0, sizeof(OciDescriptor));
    desc->schema_version = 2;
    strncpy(desc->media_type, media_type, sizeof(desc->media_type) - 1);
    desc->size = size;
    snprintf(desc->digest, sizeof(desc->digest), "sha256:%s", sha256_digest);
    return 0;
}

/* -- OCI Image Config --------------------------------------------- */



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

    json_copy_string_value(json, "architecture", config->architecture, sizeof(config->architecture));
    json_copy_string_value(json, "os", config->os, sizeof(config->os));
    json_copy_string_value(json, "created", config->created, sizeof(config->created));
    json_copy_string_value(json, "WorkingDir", config->working_dir, sizeof(config->working_dir));
    json_copy_string_value(json, "User", config->user, sizeof(config->user));

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

/* -- OCI Image Manifest ------------------------------------------- */

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

    json_copy_string_value(json, "mediaType", manifest->media_type, sizeof(manifest->media_type));
    if (!manifest->media_type[0])
        strncpy(manifest->media_type, oci_media_type_image_manifest_v2(), sizeof(manifest->media_type) - 1);

    const char *config_start = strstr(json, "\"config\"");
    if (config_start) {
        json_copy_string_value(config_start, "mediaType", manifest->config.media_type, sizeof(manifest->config.media_type));
        manifest->config.size = (uint64_t)json_find_int(config_start, "size");
        json_copy_string_value(config_start, "digest", manifest->config.digest, sizeof(manifest->config.digest));
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
                json_copy_string_value(layer_start, "mediaType", layer->media_type, sizeof(layer->media_type));
                layer->size = (uint64_t)json_find_int(layer_start, "size");
                json_copy_string_value(layer_start, "digest", layer->digest, sizeof(layer->digest));
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

/* -- OCI Image Index ---------------------------------------------- */

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

/* -- Blob Store --------------------------------------------------- */

int oci_blob_store_init(const char *root_path) {
    if (!root_path) return -1;
    char path[1024];
    /* Create root directory */
    if (mkdir(root_path, 0755) < 0 && errno != EEXIST) return -1;
    /* Create blobs subdirectory */
    snprintf(path, sizeof(path), "%s/blobs", root_path);
    if (mkdir(path, 0755) < 0 && errno != EEXIST) return -1;
    /* Create sha256 algorithm directory */
    snprintf(path, sizeof(path), "%s/blobs/sha256", root_path);
    if (mkdir(path, 0755) < 0 && errno != EEXIST) return -1;
    /* Create oci-layout file to mark this as a valid OCI layout */
    snprintf(path, sizeof(path), "%s/oci-layout", root_path);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *layout = "{\"imageLayoutVersion\":\"1.0.0\"}";
        write(fd, layout, strlen(layout));
        close(fd);
    }
    return 0;
}

int oci_blob_put(const char *root_path, const char *digest, const void *data, size_t size) {
    if (!root_path || !digest || !data) return -1;
    
    char blob_path[1024];
    snprintf(blob_path, sizeof(blob_path), "%s/blobs/sha256/%s", root_path, digest + 7);
    
    char *slash = strrchr(blob_path, '/');
    if (slash) { *slash = '\0'; mkdir(blob_path, 0755); *slash = '/'; }
    
    int fd = open(blob_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    
    ssize_t n = write(fd, data, size);
    close(fd);
    
    return n == (ssize_t)size ? 0 : -1;
}

int oci_blob_get(const char *root_path, const char *digest, void *out_data, size_t *out_size) {
    if (!root_path || !digest) return -1;
    
    char blob_path[1024];
    snprintf(blob_path, sizeof(blob_path), "%s/blobs/sha256/%s", root_path, digest + 7);
    
    int fd = open(blob_path, O_RDONLY);
    if (fd < 0) return -1;
    
    struct stat st;
    fstat(fd, &st);
    
    if (out_size) *out_size = st.st_size;
    if (out_data) {
        ssize_t n = read(fd, out_data, st.st_size);
        close(fd);
        return n == st.st_size ? 0 : -1;
    }
    
    close(fd);
    return 0;
}

bool oci_blob_exists(const char *root_path, const char *digest) {
    if (!root_path || !digest) return false;
    char path[1024];
    snprintf(path, sizeof(path), "%s/blobs/sha256/%s", root_path, digest + 7);
    return access(path, F_OK) == 0;
}

/* -- Convert .wubu <-> OCI ---------------------------------------- */

int oci_image_to_wubu(const char *oci_dir, const char *wubu_output) {
    if (!oci_dir || !wubu_output) return -1;
    char index_path[512];
    snprintf(index_path, sizeof(index_path), "%s/index.json", oci_dir);
    FILE *f = fopen(index_path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long index_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *index_json = malloc(index_size + 1);
    if (!index_json) { fclose(f); return -1; }
    fread(index_json, 1, index_size, f);
    index_json[index_size] = '\0';
    fclose(f);

    const char *manifest_ref = strstr(index_json, "\"digest\"");
    if (!manifest_ref) { free(index_json); return -1; }
    manifest_ref = strchr(manifest_ref, ':');
    if (!manifest_ref) { free(index_json); return -1; }
    const char *digest_start = strchr(manifest_ref, '"');
    if (!digest_start) { free(index_json); return -1; }
    digest_start++;
    const char *digest_end = strchr(digest_start, '"');
    if (!digest_end) { free(index_json); return -1; }
    char manifest_digest[128];
    size_t digest_len = (size_t)(digest_end - digest_start);
    if (digest_len >= sizeof(manifest_digest)) digest_len = sizeof(manifest_digest) - 1;
    memcpy(manifest_digest, digest_start, digest_len);
    manifest_digest[digest_len] = '\0';
    char blob_name[128] = {0};
    if (strncmp(manifest_digest, "sha256:", 7) == 0)
        memcpy(blob_name, manifest_digest + 7, digest_len - 7);
    else
        memcpy(blob_name, manifest_digest, digest_len);

    char manifest_path[512];
    snprintf(manifest_path, sizeof(manifest_path), "%s/blobs/sha256/%s", oci_dir, blob_name);
    f = fopen(manifest_path, "rb");
    if (!f) { free(index_json); return -1; }
    fseek(f, 0, SEEK_END);
    long manifest_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *manifest_json = malloc(manifest_size + 1);
    if (!manifest_json) { fclose(f); free(index_json); return -1; }
    fread(manifest_json, 1, manifest_size, f);
    manifest_json[manifest_size] = '\0';
    fclose(f);

    OciImageManifest oci_manifest;
    if (oci_manifest_from_json(manifest_json, &oci_manifest) < 0) { free(manifest_json); free(index_json); return -1; }
    char config_blob[128] = {0};
    if (strncmp(oci_manifest.config.digest, "sha256:", 7) == 0)
        memcpy(config_blob, oci_manifest.config.digest + 7, strlen(oci_manifest.config.digest) - 7);
    else
        memcpy(config_blob, oci_manifest.config.digest, strlen(oci_manifest.config.digest));
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/blobs/sha256/%s", oci_dir, config_blob);
    char config_json[8192] = {0};
    f = fopen(config_path, "rb");
    if (f) {
        long n = fread(config_json, 1, sizeof(config_json) - 1, f);
        if (n > 0) config_json[n] = '\0';
        fclose(f);
    }

    WubuImageManifest wubu_manifest;
    if (oci_manifest_to_wubu(&oci_manifest, oci_dir, &wubu_manifest) < 0) { free(manifest_json); free(index_json); return -1; }
    int ret = wubu_image_export_wubu(&wubu_manifest, wubu_output);
    free(manifest_json);
    free(index_json);
    return ret;
}

/* -- OCI Manifest -> Wubu Manifest Conversion ---------------------- */

int oci_manifest_to_wubu(const OciImageManifest *oci_manifest, const char *oci_dir,
                         WubuImageManifest *wubu_manifest) {
    if (!oci_manifest || !wubu_manifest) return -1;

    char config_blob[128] = {0};
    if (strncmp(oci_manifest->config.digest, "sha256:", 7) == 0)
        memcpy(config_blob, oci_manifest->config.digest + 7, strlen(oci_manifest->config.digest) - 7);
    else
        memcpy(config_blob, oci_manifest->config.digest, strlen(oci_manifest->config.digest));

    char config_path[512];
    if (oci_dir) {
        snprintf(config_path, sizeof(config_path), "%s/blobs/sha256/%s", oci_dir, config_blob);
    } else {
        config_path[0] = '\0';
    }
    char config_json[8192] = {0};
    if (oci_dir && config_path[0]) {
        FILE *f = fopen(config_path, "rb");
        if (f) {
            long n = fread(config_json, 1, sizeof(config_json) - 1, f);
            if (n > 0) config_json[n] = '\0';
            fclose(f);
        }
    }

    memset(wubu_manifest, 0, sizeof(*wubu_manifest));
    OciImageConfig oci_config;
    oci_config_from_json(config_json, &oci_config);
    if (oci_config.entrypoint_count > 0) strncpy(wubu_manifest->entrypoint, oci_config.entrypoint[0], WUBU_MAX_CMD_LEN - 1);
    if (oci_config.cmd_count > 0) strncpy(wubu_manifest->cmd, oci_config.cmd[0], WUBU_MAX_CMD_LEN - 1);
    strncpy(wubu_manifest->workdir, oci_config.working_dir, sizeof(wubu_manifest->workdir) - 1);
    strncpy(wubu_manifest->user, oci_config.user, sizeof(wubu_manifest->user) - 1);
    for (int i = 0; i < oci_config.env_count && i < WUBU_MAX_ENVS; i++) strncpy(wubu_manifest->envs[i], oci_config.env[i], sizeof(wubu_manifest->envs[i]) - 1);
    wubu_manifest->env_count = oci_config.env_count;
    for (int i = 0; i < oci_config.exposed_port_count && i < WUBU_MAX_PORTS; i++) wubu_manifest->ports[i] = oci_config.exposed_ports[i];
    wubu_manifest->port_count = oci_config.exposed_port_count;
    wubu_manifest->layer_count = oci_manifest->layer_count;
    for (int i = 0; i < oci_manifest->layer_count && i < WUBU_MAX_LAYERS; i++) {
        wubu_manifest->layers[i].size = oci_manifest->layers[i].size;
        strncpy(wubu_manifest->layers[i].digest, oci_manifest->layers[i].digest, WUBU_LAYER_DIGEST_LEN - 1);
        wubu_manifest->layers[i].created = time(NULL);
    }
    wubu_manifest->arch = wubu_arch_from_string(oci_config.architecture);
    wubu_manifest->os = wubu_os_from_string(oci_config.os);
    wubu_manifest->created = atol(oci_config.created);
    wubu_manifest->stop_signal = oci_config.stop_signal;
    wubu_manifest_compute_id(wubu_manifest);
    return 0;
}

int oci_image_from_wubu(const char *wubu_path, const char *output_dir) {
    if (!wubu_path || !output_dir) return -1;
    
    WubuImageManifest manifest;
    if (wubu_image_import_wubu(wubu_path, &manifest) < 0) return -1;
    
    return oci_image_from_manifest(&manifest, output_dir);
}

int oci_image_from_manifest(const void *wubu_manifest_ptr, const char *output_dir) {
    if (!wubu_manifest_ptr || !output_dir) return -1;
    
    const WubuImageManifest *manifest = (const WubuImageManifest *)wubu_manifest_ptr;
    
    ensure_oci_dirs();
    mkdir(output_dir, 0755);
    
    /* Create OCI config */
    OciImageConfig config;
    oci_config_create(&config, manifest);
    
    char config_json[16384];
    oci_config_to_json(&config, config_json, sizeof(config_json));
    
    char config_digest[65];
    sha256_digest(config_json, strlen(config_json), config_digest);
    
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/blobs/sha256/%s", output_dir, config_digest);
    mkdir("/var/lib/wubu/oci/blobs/sha256", 0755);
    
    int fd = open(config_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    write(fd, config_json, strlen(config_json));
    close(fd);
    
    /* Create manifest */
    OciImageManifest oci_manifest;
    oci_manifest_create(&oci_manifest, manifest);
    
    char manifest_json[32768];
    oci_manifest_to_json(&oci_manifest, manifest_json, sizeof(manifest_json));
    
    char manifest_digest[65];
    sha256_digest(manifest_json, strlen(manifest_json), manifest_digest);
    
    char manifest_path[1024];
    snprintf(manifest_path, sizeof(manifest_path), "%s/blobs/sha256/%s", output_dir, manifest_digest);
    fd = open(manifest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    write(fd, manifest_json, strlen(manifest_json));
    close(fd);
    
    /* Write oci-layout */
    char layout_json[256];
    snprintf(layout_json, sizeof(layout_json), "{\"imageLayoutVersion\":\"1.0.0\"}");
    
    char layout_path[1024];
    snprintf(layout_path, sizeof(layout_path), "%s/oci-layout", output_dir);
    fd = open(layout_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) { write(fd, layout_json, strlen(layout_json)); close(fd); }
    
    /* Write index.json */
    OciImageIndex index;
    oci_index_create(&index);
    
    OciDescriptor desc = oci_manifest.config;
    OciPlatform platform = {0};
    strncpy(platform.architecture, wubu_arch_name(manifest->arch), 31);
    strncpy(platform.os, wubu_os_name(manifest->os), 31);
    
    oci_index_add_manifest(&index, &desc, &platform);
    
    char index_json[8192];
    oci_index_to_json(&index, index_json, sizeof(index_json));
    
    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/index.json", output_dir);
    fd = open(index_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) { write(fd, index_json, strlen(index_json)); close(fd); }
    
    /* Copy layer blobs */
    for (int i = 0; i < manifest->layer_count; i++) {
        char src_path[1024];
        snprintf(src_path, sizeof(src_path), "/var/cache/wubu/layers/%s", manifest->layers[i].digest);
        
        char dst_path[1024];
        snprintf(dst_path, sizeof(dst_path), "%s/blobs/sha256/%s", output_dir, manifest->layers[i].digest);
        mkdir("/var/lib/wubu/oci/blobs/sha256", 0755);
        
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "cp '%s' '%s' 2>/dev/null", src_path, dst_path);
        system(cmd);
    }
    
    return 0;
}

/* -- Registry Operations ------------------------------------------ */

OciRegistryClient *oci_registry_client_new(const char *registry, const char *username, const char *password) {
    if (!registry) return NULL;
    
    OciRegistryClient *client = calloc(1, sizeof(OciRegistryClient));
    if (!client) return NULL;
    
    strncpy(client->registry, registry, 255);
    if (username) strncpy(client->username, username, 127);
    if (password) strncpy(client->password, password, 127);
    
    return client;
}

void oci_registry_client_free(OciRegistryClient *client) {
    if (client) free(client);
}

/* OCI registry, runtime spec, hooks implemented using curl CLI for transport */
static int curl_to_file(const char *url, const char *out, const char *extra_headers) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "curl -sS -L -o '%s' -H 'Accept: application/vnd.oci.image.manifest.v2+json' %s '%s' >/dev/null 2>&1", out, extra_headers ? extra_headers : "", url);
    return system(cmd) == 0 ? 0 : -1;
}

static int curl_to_mem(const char *url, char *out, size_t out_size, const char *extra_headers) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "curl -sS -L -H 'Accept: application/vnd.oci.image.manifest.v2+json' %s '%s' > /tmp/oci_curl_out.bin", extra_headers ? extra_headers : "", url);
    if (system(cmd) != 0) return -1;
    FILE *f = fopen("/tmp/oci_curl_out.bin", "rb");
    if (!f) return -1;
    size_t n = fread(out, 1, out_size - 1, f);
    out[n] = 0;
    fclose(f);
    return 0;
}

int oci_registry_ping(OciRegistryClient *client) {
    if (!client || !client->registry[0]) return -1;
    char url[512];
    snprintf(url, sizeof(url), "https://%s/v2/", client->registry);
    return curl_to_file(url, "/dev/null", NULL);
}

int oci_registry_get_manifest(OciRegistryClient *client, const char *repo, const char *tag_or_digest,
                              OciImageManifest *out_manifest, char *out_raw_json, size_t raw_size) {
    if (!client || !repo || !tag_or_digest || !out_manifest) return -1;
    char url[512];
    snprintf(url, sizeof(url), "https://%s/v2/%s/manifests/%s", client->registry, repo, tag_or_digest);
    char hdr[128];
    snprintf(hdr, sizeof(hdr), "-H 'Accept: application/vnd.oci.image.manifest.v2+json'");
    if (out_raw_json && raw_size) {
        if (curl_to_mem(url, out_raw_json, raw_size, hdr) < 0) return -1;
        return oci_manifest_from_json(out_raw_json, out_manifest);
    }
    char tmp[65536];
    if (curl_to_mem(url, tmp, sizeof(tmp), hdr) < 0) return -1;
    if (out_raw_json && raw_size) {
        size_t n = strlen(tmp);
        if (n >= raw_size) n = raw_size - 1;
        memcpy(out_raw_json, tmp, n);
        out_raw_json[n] = 0;
    }
    return oci_manifest_from_json(tmp, out_manifest);
}

int oci_registry_put_manifest(OciRegistryClient *client, const char *repo, const char *tag,
                              const OciImageManifest *manifest, const char *media_type) {
    if (!client || !repo || !manifest) return -1;
    char json[65536];
    if (oci_manifest_to_json(manifest, json, sizeof(json)) < 0) return -1;
    char path[256];
    snprintf(path, sizeof(path), "/tmp/oci_put_manifest_%d.json", getpid());
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fwrite(json, 1, strlen(json), f);
    fclose(f);
    char url[512];
    snprintf(url, sizeof(url), "https://%s/v2/%s/manifests/%s", client->registry, repo, tag ? tag : "latest");
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "curl -sS -L -X PUT -H 'Content-Type: %s' --data-binary @%s '%s' >/dev/null 2>&1", media_type ? media_type : "application/vnd.oci.image.manifest.v2+json", path, url);
    int ret = system(cmd);
    unlink(path);
    return ret == 0 ? 0 : -1;
}

int oci_registry_get_blob(OciRegistryClient *client, const char *repo, const char *digest,
                          void *out_data, size_t *out_size) {
    if (!client || !repo || !digest) return -1;
    char url[512];
    snprintf(url, sizeof(url), "https://%s/v2/%s/blobs/%s", client->registry, repo, digest);
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/oci_get_blob_%d.bin", getpid());
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "curl -sS -L -o '%s' '%s' >/dev/null 2>&1", tmp_path, url);
    if (system(cmd) != 0) { unlink(tmp_path); return -1; }
    int fd = open(tmp_path, O_RDONLY);
    if (fd < 0) { unlink(tmp_path); return -1; }
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); unlink(tmp_path); return -1; }
    if (out_size) *out_size = (size_t)st.st_size;
    if (out_data && out_size) {
        ssize_t n = read(fd, out_data, (size_t)st.st_size);
        close(fd);
        unlink(tmp_path);
        return n == st.st_size ? 0 : -1;
    }
    close(fd);
    unlink(tmp_path);
    return 0;
}

int oci_registry_put_blob(OciRegistryClient *client, const char *repo, const char *digest,
                          const void *data, size_t size) {
    if (!client || !repo || !digest || !data) return -1;
    /* Write blob data to temp file */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/oci_put_blob_%d.bin", getpid());
    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    ssize_t written = write(fd, data, size);
    close(fd);
    if (written != (ssize_t)size) { unlink(tmp_path); return -1; }
    /* Monolithic upload: PUT /v2/<name>/blobs/uploads/<digest> with data */
    char url[512];
    snprintf(url, sizeof(url), "https://%s/v2/%s/blobs/uploads/?digest=%s", client->registry, repo, digest);
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "curl -sS -L -X POST -H 'Content-Type: application/octet-stream' --data-binary @'%s' '%s' >/dev/null 2>&1",
             tmp_path, url);
    int ret = system(cmd);
    unlink(tmp_path);
    return ret == 0 ? 0 : -1;
}

int oci_registry_mount_blob(OciRegistryClient *client, const char *from_repo, const char *to_repo,
                            const char *digest) {
    if (!client || !from_repo || !to_repo || !digest) return -1;
    char url[512];
    snprintf(url, sizeof(url), "https://%s/v2/%s/blobs/uploads/?mount=%s&from=%s",
             client->registry, to_repo, digest, from_repo);
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "curl -sS -L -X POST '%s' >/dev/null 2>&1", url);
    return system(cmd) == 0 ? 0 : -1;
}

int oci_registry_delete_manifest(OciRegistryClient *client, const char *repo, const char *digest) {
    if (!client || !repo || !digest) return -1;
    char url[512];
    snprintf(url, sizeof(url), "https://%s/v2/%s/manifests/%s", client->registry, repo, digest);
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "curl -sS -L -X DELETE '%s' >/dev/null 2>&1", url);
    return system(cmd) == 0 ? 0 : -1;
}

int oci_registry_list_tags(OciRegistryClient *client, const char *repo, char tags[][128], int max) {
    if (!client || !repo || !tags || max <= 0) return -1;
    char url[512];
    snprintf(url, sizeof(url), "https://%s/v2/%s/tags/list", client->registry, repo);
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/oci_tags_%d.json", getpid());
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "curl -sS -L -o '%s' '%s' >/dev/null 2>&1", tmp_path, url);
    if (system(cmd) != 0) { unlink(tmp_path); return -1; }
    FILE *f = fopen(tmp_path, "r");
    if (!f) { unlink(tmp_path); return -1; }
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    fclose(f);
    unlink(tmp_path);
    /* Parse "tags":["tag1","tag2",...] from JSON */
    const char *tags_arr = strstr(buf, "\"tags\"");
    if (!tags_arr) return 0;
    const char *bracket = strchr(tags_arr, '[');
    if (!bracket) return 0;
    int count = 0;
    const char *scan = bracket + 1;
    while (count < max) {
        const char *q = strchr(scan, '\"');
        if (!q || q > strchr(scan, ']')) break;
        const char *end = strchr(q + 1, '\"');
        if (!end) break;
        size_t len = (size_t)(end - q - 1);
        if (len >= 128) len = 127;
        memcpy(tags[count], q + 1, len);
        tags[count][len] = 0;
        count++;
        scan = end + 1;
    }
    return count;
}

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

static const char *json_skip(const char *p, const char *key) {
    char s[256];
    snprintf(s, sizeof(s), "\"%s\"", key);
    const char *pos = strstr(p, s);
    if (!pos) return NULL;
    pos = strchr(pos + strlen(s), ':');
    if (!pos) return NULL;
    pos++;
    while (*pos && isspace((unsigned char)*pos)) pos++;
    return pos;
}

static void json_copy_str(const char *src, char *dst, size_t n) {
    if (!src) { *dst = 0; return; }
    while (*src && *src != '"' && n > 1) { *dst++ = *src++; n--; }
    *dst = 0;
}

int oci_runtime_spec_from_json(const char *json, OciRuntimeSpec *spec) {
    if (!json || !spec) return -1;
    memset(spec, 0, sizeof(*spec));
    json_copy_string_value(json, "ociVersion", spec->oci_version, sizeof(spec->oci_version));
    json_copy_string_value(json, "cwd", spec->process.cwd, sizeof(spec->process.cwd));
    const char *root = strstr(json, "\"root\"");
    if (root) {
        json_copy_string_value(root, "path", spec->root.path, sizeof(spec->root.path));
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

int oci_hook_create(OciHook *hook, const char *path, const char *args[], int argc,
                    const char *env[], int envc, int timeout) {
    if (!hook || !path) return -1;
    memset(hook, 0, sizeof(*hook));
    strncpy(hook->path, path, sizeof(hook->path) - 1);
    hook->argc = argc > 32 ? 32 : argc;
    for (int i = 0; i < hook->argc && args && args[i]; i++)
        strncpy(hook->args[i], args[i], sizeof(hook->args[i]) - 1);
    hook->envc = envc > 32 ? 32 : envc;
    for (int i = 0; i < hook->envc && env && env[i]; i++)
        strncpy(hook->env[i], env[i], sizeof(hook->env[i]) - 1);
    hook->timeout = timeout;
    return 0;
}


void oci_hook_free(OciHook *hook) {
    (void)hook;
}

/* -- Cleanup Stub ------------------------------------------------- */

int oci_cleanup_old_layers(const char *root_path, time_t max_age_days, bool dry_run) {
    if (!root_path || max_age_days <= 0) return -1;
    char blob_dir[1024];
    snprintf(blob_dir, sizeof(blob_dir), "%s/blobs/sha256", root_path);
    DIR *dir = opendir(blob_dir);
    if (!dir) {
        /* No blob directory means nothing to clean — not an error */
        if (errno == ENOENT) return 0;
        return -1;
    }
    time_t now = time(NULL);
    time_t max_age_secs = (time_t)max_age_days * 86400;
    int removed = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char fpath[1280];
        snprintf(fpath, sizeof(fpath), "%s/%s", blob_dir, entry->d_name);
        struct stat st;
        if (stat(fpath, &st) < 0) continue;
        if (!S_ISREG(st.st_mode)) continue;
        time_t age = now - st.st_mtime;
        if (age > max_age_secs) {
            if (!dry_run) {
                if (unlink(fpath) == 0) removed++;
            } else {
                removed++;
            }
        }
    }
    closedir(dir);
    return removed;
}

int oci_gc_unreferenced_blobs(const char *root_path, bool dry_run) {
    if (!root_path) return -1;
    char blob_dir[1024];
    snprintf(blob_dir, sizeof(blob_dir), "%s/blobs/sha256", root_path);
    /* Collect all blob digests */
    DIR *dir = opendir(blob_dir);
    if (!dir) {
        if (errno == ENOENT) return 0;
        return -1;
    }
    /* Collect all blob digests */
    typedef struct { char name[128]; time_t mtime; } blob_info_t;
    blob_info_t blobs[4096];
    int blob_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && blob_count < 4096) {
        if (entry->d_name[0] == '.') continue;
        char fpath[1280];
        snprintf(fpath, sizeof(fpath), "%s/%s", blob_dir, entry->d_name);
        struct stat st;
        if (stat(fpath, &st) < 0) continue;
        if (!S_ISREG(st.st_mode)) continue;
        strncpy(blobs[blob_count].name, entry->d_name, 127);
        blobs[blob_count].mtime = st.st_mtime;
        blob_count++;
    }
    closedir(dir);
    /* Mark referenced blobs: scan index.json and manifest files */
    char referenced[4096] = {0};
    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/index.json", root_path);
    FILE *f = fopen(index_path, "r");
    if (f) {
        char buf[65536];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = 0;
        fclose(f);
        /* Find all sha256: digests in index.json */
        const char *scan = buf;
        while ((scan = strstr(scan, "sha256:")) != NULL) {
            scan += 7;
            char digest[65];
            int i;
            for (i = 0; i < 64 && scan[i] && isxdigit((unsigned char)scan[i]); i++)
                digest[i] = scan[i];
            digest[i] = 0;
            if (i == 64) {
                for (int j = 0; j < blob_count; j++) {
                    if (strcmp(blobs[j].name, digest) == 0) {
                        referenced[j] = 1;
                        break;
                    }
                }
            }
        }
    }
    /* Also check manifest blobs in the blobs directory */
    for (int i = 0; i < blob_count; i++) {
        if (referenced[i]) continue;
        char mf_path[1280];
        snprintf(mf_path, sizeof(mf_path), "%s/%s", blob_dir, blobs[i].name);
        f = fopen(mf_path, "r");
        if (!f) continue;
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = 0;
        fclose(f);
        /* If this blob contains "sha256:" references, it's a manifest — mark its refs */
        if (strstr(buf, "\"sha256:") || strstr(buf, "\"digest\"")) {
            const char *scan = buf;
            while ((scan = strstr(scan, "sha256:")) != NULL) {
                scan += 7;
                char digest[65];
                int k;
                for (k = 0; k < 64 && scan[k] && isxdigit((unsigned char)scan[k]); k++)
                    digest[k] = scan[k];
                digest[k] = 0;
                if (k == 64) {
                    for (int j = 0; j < blob_count; j++) {
                        if (strcmp(blobs[j].name, digest) == 0) {
                            referenced[j] = 1;
                            break;
                        }
                    }
                }
            }
        }
    }
    /* Delete unreferenced blobs */
    int removed = 0;
    for (int i = 0; i < blob_count; i++) {
        if (!referenced[i]) {
            char fpath[1280];
            snprintf(fpath, sizeof(fpath), "%s/%s", blob_dir, blobs[i].name);
            if (!dry_run) {
                if (unlink(fpath) == 0) removed++;
            } else {
                removed++;
            }
        }
    }
    return removed;
}
