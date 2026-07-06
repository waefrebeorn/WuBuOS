/*
 * oci_internal.h  --  Internal header for OCI modules
 * 
 * Following EDR pattern: opaque structs in public header, full defs here.
 * All extracted modules include this header first.
 */

#ifndef WUBU_OCI_INTERNAL_H
#define WUBU_OCI_INTERNAL_H

#include "wubu_oci.h"
#include "wubu_image.h"
#include "wubu_container.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/sendfile.h>

/* -- mbedTLS TLS Support (optional) --------------------------------- */
#ifdef MBEDTLS_SSL_H
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/x509_crt.h>
#endif

/* -- SHA256 via wubu_crypto.h ---------------------------------------- */
#include "wubu_crypto.h"

/* -- sha256_digest wrapper (for backward compat with extracted code) --- */
static inline void sha256_digest(const void *data, size_t size, char *out_hex) {
    wubu_sha256_digest(data, size, out_hex, 65);
}

/* -- Safe String Macros ---------------------------------------------- */
#define WUBU_STRCPY(dst, src, sz) do { strncpy(dst, src, sz - 1); (dst)[sz - 1] = '\0'; } while(0)

/* -- Constants ------------------------------------------------------- */
#define OCI_BLOB_DIR    "/var/lib/wubu/oci/blobs/sha256"
#define OCI_LAYOUT_FILE "oci-layout"
#define OCI_HTTP_BUF_SIZE 8192

/* -- Forward Declarations (opaque in public header) ------------------ */

/* HTTP Client - full definition moved from wubu_oci.c */
typedef struct OciHttpClient {
    int sockfd;
    char host[256];
    int port;
    bool use_tls;
#ifdef MBEDTLS_SSL_H
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt cacert;
    bool tls_initialized;
#endif
} OciHttpClient;

/* Registry Client - full definition moved from wubu_oci.c */
struct OciRegistryClient {
    char registry[256];
    char auth_token[512];
    char username[128];
    char password[128];
};

/* -- Shared JSON Helpers (static inline for zero overhead) ----------- */

static inline const char *oci_json_find_string(const char *json, const char *key) {
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

static inline int oci_json_copy_string_value(const char *json, const char *key, char *dst, size_t dst_size) {
    if (!dst || dst_size == 0) return -1;
    const char *start = oci_json_find_string(json, key);
    if (!start) { dst[0] = '\0'; return -1; }
    const char *end = strchr(start, '"');
    if (!end) { dst[0] = '\0'; return -1; }
    size_t len = (size_t)(end - start);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
    return (int)len;
}

static inline int oci_json_find_int(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) return 0;
    const char *colon = strchr(pos + strlen(search), ':');
    if (!colon) return 0;
    while (*colon && !isdigit((unsigned char)*colon) && *colon != '-') colon++;
    return atoi(colon);
}

static inline const char *oci_json_skip(const char *p, const char *key) {
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

static inline void oci_json_copy_str(const char *src, char *dst, size_t n) {
    if (!src) { *dst = 0; return; }
    while (*src && *src != '"' && n > 1) { *dst++ = *src++; n--; }
    *dst = 0;
}

/* -- Directory Helper ------------------------------------------------ */
static inline void oci_ensure_oci_dirs(void) {
    mkdir(OCI_BLOB_DIR, 0755);
}

/* -- HTTP Client API (implemented in oci_http_client.c) -------------- */
int  oci_http_connect(OciHttpClient *client, const char *host, int port, bool use_tls);
void oci_http_close(OciHttpClient *client);
int  oci_http_send(OciHttpClient *client, const void *data, size_t len);
int  oci_http_recv(OciHttpClient *client, void *buf, size_t buf_size, size_t *out_recv);
int  oci_http_request(OciHttpClient *client, const char *method, const char *path,
                       const char *extra_headers, const void *body, size_t body_len,
                       char *response, size_t resp_size);

/* Auth helpers */
void oci_parse_auth_header(const char *response_headers, char *token, size_t token_size,
                            char *realm, size_t realm_size, char *service, size_t service_size,
                            char *scope, size_t scope_size);
void oci_base64_encode(const char *input, char *output, size_t out_size);

/* -- Config API (implemented in oci_image_config.c) ------------------ */
int  oci_config_create(OciImageConfig *config, const void *wubu_manifest_ptr);
int  oci_config_to_json(const OciImageConfig *config, char *out_json, size_t out_size);
int  oci_config_from_json(const char *json, OciImageConfig *config);
int  oci_config_compute_digest(const OciImageConfig *config, char *out_digest, size_t out_size);

/* -- Manifest API (implemented in oci_image_manifest.c) -------------- */
int  oci_manifest_create(OciImageManifest *manifest, const void *wubu_manifest_ptr);
int  oci_manifest_to_json(const OciImageManifest *manifest, char *out_json, size_t out_size);
int  oci_manifest_from_json(const char *json, OciImageManifest *manifest);
int  oci_manifest_compute_digest(const OciImageManifest *manifest, char *out_digest, size_t out_size);

/* -- Index API (implemented in oci_image_index.c) -------------------- */
int  oci_index_create(OciImageIndex *index);
int  oci_index_add_manifest(OciImageIndex *index, const OciDescriptor *desc, const OciPlatform *platform);
int  oci_index_to_json(const OciImageIndex *index, char *out_json, size_t out_size);
int  oci_index_from_json(const char *json, OciImageIndex *index);

/* -- Blob Store API (implemented in oci_blob_store.c) ---------------- */
int  oci_blob_store_init(const char *root_path);
int  oci_blob_put(const char *root_path, const char *digest, const void *data, size_t size);
int  oci_blob_get(const char *root_path, const char *digest, void *out_data, size_t *out_size);
bool oci_blob_exists(const char *root_path, const char *digest);

/* -- Conversion API (implemented in oci_convert.c) ------------------- */
int  oci_image_to_wubu(const char *oci_dir, const char *wubu_output);
int  oci_manifest_to_wubu(const OciImageManifest *oci_manifest, const char *oci_dir,
                           WubuImageManifest *wubu_manifest);
int  oci_image_from_wubu(const char *wubu_path, const char *output_dir);
int  oci_image_from_manifest(const void *wubu_manifest_ptr, const char *output_dir);

/* -- Registry API (implemented in oci_registry.c) -------------------- */
OciRegistryClient *oci_registry_client_new(const char *registry, const char *username, const char *password);
void oci_registry_client_free(OciRegistryClient *client);
int  oci_registry_ping(OciRegistryClient *client);
int  oci_registry_get_manifest(OciRegistryClient *client, const char *repo, const char *tag_or_digest,
                                OciImageManifest *out_manifest, char *out_raw_json, size_t raw_size);
int  oci_registry_put_manifest(OciRegistryClient *client, const char *repo, const char *tag,
                                const OciImageManifest *manifest, const char *media_type);
int  oci_registry_get_blob(OciRegistryClient *client, const char *repo, const char *digest,
                            void *out_data, size_t *out_size);
int  oci_registry_put_blob(OciRegistryClient *client, const char *repo, const char *digest,
                            const void *data, size_t size);
int  oci_registry_mount_blob(OciRegistryClient *client, const char *from_repo, const char *to_repo,
                              const char *digest);
int  oci_registry_list_tags(OciRegistryClient *client, const char *repo, char tags[][128], int max);
int  oci_registry_delete_manifest(OciRegistryClient *client, const char *repo, const char *digest);

/* -- Runtime Spec API (implemented in oci_runtime_spec.c) ------------ */
int  oci_runtime_spec_create(OciRuntimeSpec *spec, const void *manifest);
void oci_runtime_spec_free(OciRuntimeSpec *spec);
int  oci_runtime_spec_to_json(const OciRuntimeSpec *spec, char *out_json, size_t out_size);
int  oci_runtime_spec_from_json(const char *json, OciRuntimeSpec *spec);
int  oci_runtime_spec_validate(const OciRuntimeSpec *spec);

/* -- Hooks API (implemented in oci_hooks.c) -------------------------- */
int  oci_hook_create(OciHook *hook, const char *path, const char *args[], int argc,
                      const char *env[], int envc, int timeout);
void oci_hook_free(OciHook *hook);

/* -- Cleanup API (implemented in oci_cleanup.c) ---------------------- */
int  oci_cleanup_old_layers(const char *root_path, time_t max_age_days, bool dry_run);
int  oci_gc_unreferenced_blobs(const char *root_path, bool dry_run);

/* -- Media Type Helpers (implemented in oci_media_types.c) ----------- */
const char *oci_media_type_image_manifest_v1(void);
const char *oci_media_type_image_manifest_v2(void);
const char *oci_media_type_image_index_v1(void);
const char *oci_media_type_image_config_v1(void);
const char *oci_media_type_layer_v1(void);
const char *oci_media_type_layer_v1_gzip(void);
const char *oci_media_type_layer_v1_zstd(void);
const char *oci_media_type_empty_json(void);

/* -- Descriptor Operations (implemented in oci_descriptor.c) --------- */
int oci_create_descriptor(OciDescriptor *desc, const char *media_type, uint64_t size, const char *sha256_digest);

#endif /* WUBU_OCI_INTERNAL_H */