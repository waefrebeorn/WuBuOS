/*
 * wubu_image_manifest.c  --  WuBuOS Image Manifest JSON Operations
 *
 * Extracted from wubu_image.c (2026-07-06): manifest serialization,
 * deserialization, save/load, and ID computation.
 *
 * C11 only. No globals. Self-contained: depends only on wubu_image.h
 * for types and wubu_image_internal.h for sha256_digest wrapper.
 */

#include "wubu_image_internal.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

/* ================================================================ */
/* ID Computation                                                    */
/* ================================================================ */

int wubu_manifest_compute_id(WubuImageManifest *manifest) {
    if (!manifest) return -1;

    char combined[WUBU_MAX_LAYERS * 65];
    combined[0] = '\0';
    size_t combined_len = 0;
    for (int i = 0; i < manifest->layer_count; i++) {
        size_t dlen = strlen(manifest->layers[i].digest);
        if (combined_len + dlen >= sizeof(combined)) break;
        memcpy(combined + combined_len, manifest->layers[i].digest, dlen);
        combined_len += dlen;
        combined[combined_len] = '\0';
    }
    sha256_digest(combined, strlen(combined), manifest->image_id, WUBU_IMAGE_ID_LEN + 1);
    return 0;
}

/* ================================================================ */
/* JSON Serialization                                                */
/* ================================================================ */

int wubu_manifest_to_json(const WubuImageManifest *manifest, char *out_json, size_t out_size) {
    if (!manifest || !out_json || out_size < 1024) return -1;

    char *p = out_json;
    size_t remaining = out_size;
    int n;

#define APPEND(...) \
    do { \
        n = snprintf(p, remaining, __VA_ARGS__); \
        if (n < 0) return -1; \
        if ((size_t)n >= remaining) return -1; \
        p += n; remaining -= n; \
    } while(0)

    APPEND("{");
    APPEND("\"image_id\":\"%s\",", manifest->image_id);
    APPEND("\"name\":\"%s\",", manifest->name);
    APPEND("\"tag\":\"%s\",", manifest->tag);
    APPEND("\"arch\":%d,", manifest->arch);
    APPEND("\"os\":%d,", manifest->os);
    APPEND("\"created\":%ld,", manifest->created);
    APPEND("\"layer_count\":%d,", manifest->layer_count);
    APPEND("\"entrypoint\":\"%s\",", manifest->entrypoint);
    APPEND("\"cmd\":\"%s\",", manifest->cmd);
    APPEND("\"workdir\":\"%s\",", manifest->workdir);
    APPEND("\"user\":\"%s\"", manifest->user);
    APPEND("}");

#undef APPEND
    return 0;
}

/* ================================================================ */
/* JSON Deserialization                                              */
/* ================================================================ */

static const char *json_str_val(const char *json, const char *key,
                                char *out, size_t out_size) {
    char key_q[64];
    snprintf(key_q, sizeof(key_q), "\"%s\"", key);
    const char *p = strstr(json, key_q);
    if (!p) return NULL;

    const char *start = strchr(p, '"');
    if (!start) return NULL;
    start = strchr(start + 1, '"');  /* skip key's own quotes */
    if (!start) return NULL;
    start++;                         /* first char of value */
    const char *end = strchr(start, '"');
    if (!end) return NULL;

    size_t len = (size_t)(end - start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return end + 1;
}

static int json_int_val(const char *json, const char *key) {
    char key_q[64];
    snprintf(key_q, sizeof(key_q), "\"%s\"", key);
    const char *p = strstr(json, key_q);
    if (!p) return 0;
    while (*p && !isdigit((unsigned char)*p) && *p != '-') p++;
    if (*p) return atoi(p);
    return 0;
}

int wubu_manifest_from_json(const char *json, WubuImageManifest *manifest) {
    if (!json || !manifest) return -1;
    memset(manifest, 0, sizeof(*manifest));

    json_str_val(json, "image_id", manifest->image_id, sizeof(manifest->image_id));
    json_str_val(json, "name", manifest->name, sizeof(manifest->name));
    json_str_val(json, "tag", manifest->tag, sizeof(manifest->tag));
    manifest->arch = (WubuArch)json_int_val(json, "arch");
    manifest->os   = (WubuOS)json_int_val(json, "os");
    manifest->created = (uint64_t)json_int_val(json, "created");
    manifest->layer_count = json_int_val(json, "layer_count");
    json_str_val(json, "entrypoint", manifest->entrypoint, sizeof(manifest->entrypoint));
    json_str_val(json, "cmd", manifest->cmd, sizeof(manifest->cmd));
    json_str_val(json, "workdir", manifest->workdir, sizeof(manifest->workdir));
    json_str_val(json, "user", manifest->user, sizeof(manifest->user));

    return 0;
}

/* ================================================================ */
/* File I/O                                                          */
/* ================================================================ */

int wubu_manifest_save(const WubuImageManifest *manifest, const char *path) {
    if (!manifest || !path) return -1;

    char json[65536];
    if (wubu_manifest_to_json(manifest, json, sizeof(json)) < 0) return -1;

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    write(fd, json, strlen(json));
    close(fd);
    return 0;
}

int wubu_manifest_load(const char *path, WubuImageManifest *manifest) {
    if (!path || !manifest) return -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    struct stat st;
    fstat(fd, &st);
    char *buf = malloc(st.st_size + 1);
    if (!buf) { close(fd); return -1; }

    ssize_t n = read(fd, buf, st.st_size);
    close(fd);
    if (n != st.st_size) { free(buf); return -1; }
    buf[n] = '\0';

    int ret = wubu_manifest_from_json(buf, manifest);
    free(buf);
    return ret;
}