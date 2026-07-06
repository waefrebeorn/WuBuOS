/*
 * oci_registry.c  --  OCI Registry Client Operations
 * 
 * Extracted from wubu_oci.c (lines 1309-1516).
 */

#include "oci_internal.h"

/* -- Registry Client ------------------------------------------------- */

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

/* -- Registry Operations using HTTP Client -------------------------- */

static int oci_registry_do_request(OciRegistryClient *client, const char *method, const char *path,
                                    const char *extra_headers, const void *body, size_t body_len,
                                    char *response, size_t resp_size, int *out_status) {
    if (!client || !client->registry[0]) return -1;

    /* Parse host:port from registry */
    char host[256];
    int port = 443;
    bool use_tls = true;
    const char *registry = client->registry;

    if (strncmp(registry, "https://", 8) == 0) {
        registry += 8;
        use_tls = true;
        port = 443;
    } else if (strncmp(registry, "http://", 7) == 0) {
        registry += 7;
        use_tls = false;
        port = 80;
    }

    const char *colon = strchr(registry, ':');
    if (colon) {
        size_t host_len = colon - registry;
        if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
        memcpy(host, registry, host_len);
        host[host_len] = '\0';
        port = atoi(colon + 1);
    } else {
        strncpy(host, registry, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
    }

    OciHttpClient http;
    if (oci_http_connect(&http, host, port, use_tls) < 0) {
        /* Fallback: try without TLS for local registries */
        if (use_tls) {
            use_tls = false;
            port = 80;
            if (oci_http_connect(&http, host, port, use_tls) < 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    /* Build auth header if needed */
    char auth_header[512] = {0};
    if (client->username[0] && client->password[0]) {
        char credentials[256];
        snprintf(credentials, sizeof(credentials), "%s:%s", client->username, client->password);
        char encoded[384];
        oci_base64_encode(credentials, encoded, sizeof(encoded));
        snprintf(auth_header, sizeof(auth_header), "Authorization: Basic %s", encoded);
    } else if (client->auth_token[0]) {
            snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", client->auth_token);
        }

    char combined_headers[1024] = {0};
    if (extra_headers && extra_headers[0]) {
        strncpy(combined_headers, extra_headers, sizeof(combined_headers) - 1);
    }
    if (auth_header[0]) {
        if (combined_headers[0]) {
            strncat(combined_headers, "\r\n", sizeof(combined_headers) - strlen(combined_headers) - 1);
        }
        strncat(combined_headers, auth_header, sizeof(combined_headers) - strlen(combined_headers) - 1);
    }

    int rc = oci_http_request(&http, method, path, combined_headers[0] ? combined_headers : NULL,
                              body, body_len, response, resp_size);
    oci_http_close(&http);
    return rc;
}

int oci_registry_ping(OciRegistryClient *client) {
    if (!client || !client->registry[0]) return -1;
    char response[1024];
    return oci_registry_do_request(client, "GET", "/v2/", NULL, NULL, 0, response, sizeof(response), NULL);
}

int oci_registry_get_manifest(OciRegistryClient *client, const char *repo, const char *tag_or_digest,
                              OciImageManifest *out_manifest, char *out_raw_json, size_t raw_size) {
    if (!client || !repo || !tag_or_digest || !out_manifest) return -1;
    char path[512];
    snprintf(path, sizeof(path), "/v2/%s/manifests/%s", repo, tag_or_digest);
    const char *accept = "Accept: application/vnd.oci.image.manifest.v2+json, application/json";
    char response[65536];
    if (oci_registry_do_request(client, "GET", path, accept, NULL, 0, response, sizeof(response), NULL) < 0) {
        return -1;
    }
    if (out_raw_json && raw_size) {
        size_t n = strlen(response);
        if (n >= raw_size) n = raw_size - 1;
        memcpy(out_raw_json, response, n);
        out_raw_json[n] = 0;
    }
    return oci_manifest_from_json(response, out_manifest);
}

int oci_registry_put_manifest(OciRegistryClient *client, const char *repo, const char *tag,
                              const OciImageManifest *manifest, const char *media_type) {
    if (!client || !repo || !manifest) return -1;
    char json[65536];
    if (oci_manifest_to_json(manifest, json, sizeof(json)) < 0) return -1;
    char path[512];
    snprintf(path, sizeof(path), "/v2/%s/manifests/%s", repo, tag ? tag : "latest");
    const char *content_type = media_type ? media_type : "application/vnd.oci.image.manifest.v2+json";
    char headers[512];
    snprintf(headers, sizeof(headers), "Content-Type: %s", content_type);
    char response[4096];
    return oci_registry_do_request(client, "PUT", path, headers, json, strlen(json), response, sizeof(response), NULL);
}

int oci_registry_get_blob(OciRegistryClient *client, const char *repo, const char *digest,
                          void *out_data, size_t *out_size) {
    if (!client || !repo || !digest) return -1;
    char path[512];
    snprintf(path, sizeof(path), "/v2/%s/blobs/%s", repo, digest);
    char response[65536];
    int rc = oci_registry_do_request(client, "GET", path, NULL, NULL, 0, response, sizeof(response), NULL);
    if (rc < 0) return -1;
    if (out_size) *out_size = strlen(response);
    if (out_data && out_size) {
        size_t copy_size = *out_size < strlen(response) ? *out_size : strlen(response);
        memcpy(out_data, response, copy_size);
        return copy_size == strlen(response) ? 0 : -1;
    }
    return 0;
}

int oci_registry_put_blob(OciRegistryClient *client, const char *repo, const char *digest,
                          const void *data, size_t size) {
    if (!client || !repo || !digest || !data) return -1;
    char path[512];
    snprintf(path, sizeof(path), "/v2/%s/blobs/uploads/?digest=%s", repo, digest);
    char response[4096];
    return oci_registry_do_request(client, "POST", path, "Content-Type: application/octet-stream", data, size, response, sizeof(response), NULL);
}

int oci_registry_mount_blob(OciRegistryClient *client, const char *from_repo, const char *to_repo,
                            const char *digest) {
    if (!client || !from_repo || !to_repo || !digest) return -1;
    char path[512];
    snprintf(path, sizeof(path), "/v2/%s/blobs/uploads/?mount=%s&from=%s", to_repo, digest, from_repo);
    char response[4096];
    return oci_registry_do_request(client, "POST", path, NULL, NULL, 0, response, sizeof(response), NULL);
}

int oci_registry_delete_manifest(OciRegistryClient *client, const char *repo, const char *digest) {
    if (!client || !repo || !digest) return -1;
    char path[512];
    snprintf(path, sizeof(path), "/v2/%s/manifests/%s", repo, digest);
    char response[4096];
    return oci_registry_do_request(client, "DELETE", path, NULL, NULL, 0, response, sizeof(response), NULL);
}

int oci_registry_list_tags(OciRegistryClient *client, const char *repo, char tags[][128], int max) {
    if (!client || !repo || !tags || max <= 0) return -1;
    char path[512];
    snprintf(path, sizeof(path), "/v2/%s/tags/list", repo);
    char response[65536];
    if (oci_registry_do_request(client, "GET", path, NULL, NULL, 0, response, sizeof(response), NULL) < 0) {
        return -1;
    }
    /* Parse "tags":["tag1","tag2",...] from JSON */
    const char *tags_arr = strstr(response, "\"tags\"");
    if (!tags_arr) return 0;
    const char *bracket = strchr(tags_arr, '[');
    if (!bracket) return 0;
    int count = 0;
    const char *scan = bracket + 1;
    while (count < max) {
        const char *q = strchr(scan, '"');
        if (!q || q > strchr(scan, ']')) break;
        const char *end = strchr(q + 1, '"');
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