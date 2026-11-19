/*
 * wubu_pkgmgr_fetch.c  --  WuBuOS Package Manager: payload download + hash gate.
 *
 * One concern: getting a remote .wubu payload into the download cache, and
 * proving it is the exact artifact the repository index promised BEFORE any
 * extract/install happens.  Fail closed: a missing/invalid index hash, a
 * failed fetch, or a sha256 mismatch all refuse the download (no install).
 *
 * Reuses the existing owners:
 *   - HTTP transport: oci_http_client.c (oci_http_connect / oci_http_request_body)
 *   - hashing:        wubu_sha256_digest (src/runtime/wubu_crypto.h)
 * No new socket code, no second crypto implementation.
 */

#include "../egojar.h"
#include "wubu_pkgmgr_internal.h"

/* -- URL splitting (shared with the repo-index fetcher) -------------- */

/* Split scheme://host[:port][/path] into parts.  `path` receives the path
 * INCLUDING the leading '/', or "" when the URL has none (caller picks its
 * own default, e.g. "/index.json" for a repo or "/" for a payload).
 * Returns false on an unusable URL. */
bool pkgmgr_split_url(const char *url, char *host, size_t hostsz,
                      int *port, char *path, size_t pathsz, bool *use_tls) {
    if (!url || !url[0] || !host || hostsz == 0 || !port || !path || pathsz == 0)
        return false;
    const char *scheme_end = strstr(url, "://");
    const char *host_start = scheme_end ? scheme_end + 3 : url;
    bool tls = scheme_end && strncmp(url, "https", 5) == 0;
    const char *slash = strchr(host_start, '/');
    const char *host_end = slash ? slash : host_start + strlen(host_start);
    size_t hlen = (size_t)(host_end - host_start);
    if (hlen == 0 || hlen >= hostsz) return false;
    memcpy(host, host_start, hlen);
    host[hlen] = '\0';
    int p = tls ? 443 : 80;
    char *colon = strrchr(host, ':');
    if (colon) {
        p = atoi(colon + 1);
        *colon = '\0';
        if (p <= 0 || p > 65535) return false;
    }
    if (host[0] == '\0') return false;
    if (slash) {
        size_t plen = strlen(slash);
        if (plen >= pathsz) return false;
        memcpy(path, slash, plen + 1);
    } else {
        path[0] = '\0';
    }
    *port = p;
    if (use_tls) *use_tls = tls;
    return true;
}

/* -- File hashing (one owner for "hash a file on disk") -------------- */

bool wubu_pkgmgr_hash_file(const char *path, char *out_hex, size_t out_size) {
    if (!path || !out_hex || out_size < 65) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    /* Read the whole file and hash it with the one-shot digest.  Payloads
     * are bounded by the fetch buffer cap (256 MB), so this stays bounded. */
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz < 0) { fclose(f); return false; }
    char *data = malloc((size_t)sz + 1);
    if (!data) { fclose(f); return false; }
    size_t rd = fread(data, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(data); return false; }
    wubu_sha256_digest(data, (size_t)sz, out_hex, out_size);
    free(data);
    return true;
}

/* -- Hash validation -------------------------------------------------- */

/* An index hash must be exactly 64 hex characters.  Anything else (empty,
 * truncated, "deadbeef") is not a trust anchor: refuse it. */
static bool valid_sha256_hex(const char *s) {
    if (!s) return false;
    size_t n = strlen(s);
    if (n != 64) return false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

/* Lowercase a 64-char hex hash in place so index casing never matters. */
static void lower_hex(char *s) {
    for (size_t i = 0; s[i]; i++)
        if (s[i] >= 'A' && s[i] <= 'F') s[i] = (char)(s[i] - 'A' + 'a');
}

/* -- Download + verify ------------------------------------------------ */

/* Fetch entry->download_url into cache_path, verifying the body's sha256
 * against entry->sha256 BEFORE the file is admitted to the cache.  The
 * download lands in cache_path.part and is renamed into place only on a
 * hash match; on any failure nothing is left behind.  Returns true only
 * when cache_path holds the exact promised artifact. */
bool wubu_pkgmgr_fetch_package(const wubu_pkg_repo_entry_t *entry, const char *cache_path) {
    if (!entry || !cache_path || !cache_path[0]) return false;

    /* Fail closed on a missing trust anchor: no URL or no valid index hash
     * means we cannot prove what we would install. */
    if (!entry->download_url[0]) {
        fprintf(stderr, "[pkgmgr] fetch refused: index entry '%s' has no download_url\n",
                entry->id);
        return false;
    }
    if (!valid_sha256_hex(entry->sha256)) {
        fprintf(stderr, "[pkgmgr] fetch refused: index entry '%s' has no valid sha256\n",
                entry->id);
        return false;
    }
    char expected[65];
    snprintf(expected, sizeof(expected), "%s", entry->sha256);
    lower_hex(expected);

    char host[256], path[512];
    int port = 80;
    bool use_tls = false;
    if (!pkgmgr_split_url(entry->download_url, host, sizeof(host), &port,
                          path, sizeof(path), &use_tls)) {
        fprintf(stderr, "[pkgmgr] fetch refused: bad download_url '%s'\n", entry->download_url);
        return false;
    }
    if (!path[0]) { path[0] = '/'; path[1] = '\0'; }

    OciHttpClient cli;
    memset(&cli, 0, sizeof(cli));
    cli.sockfd = -1;
    if (oci_http_connect(&cli, host, port, use_tls) != 0) {
        fprintf(stderr, "[pkgmgr] fetch failed: cannot connect to %s:%d\n", host, port);
        return false;
    }

    /* Size the response buffer from the index's size hint (headers need
     * headroom).  A truncated read cannot match the promised hash, so the
     * sha256 gate below also catches undersized buffers fail-closed. */
    size_t resp_size = 8u * 1024u * 1024u;
    if (entry->download_size > 0) {
        uint64_t want = entry->download_size + 64u * 1024u;
        if (want < 1024u * 1024u) want = 1024u * 1024u;
        if (want > 256u * 1024u * 1024u) want = 256u * 1024u * 1024u;
        resp_size = (size_t)want;
    }
    char *resp = malloc(resp_size);
    if (!resp) { oci_http_close(&cli); return false; }

    size_t body_len = 0;
    int rc = oci_http_request_body(&cli, "GET", path, NULL, NULL, 0,
                                   resp, resp_size, &body_len);
    oci_http_close(&cli);
    if (rc != 0 || body_len == 0) {
        fprintf(stderr, "[pkgmgr] fetch failed: GET %s (rc=%d, body=%zu)\n",
                entry->download_url, rc, body_len);
        free(resp);
        return false;
    }

    /* THE GATE: hash the downloaded bytes before anything touches disk as a
     * trusted artifact. */
    char actual[65];
    wubu_sha256_digest(resp, body_len, actual, sizeof(actual));
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "[pkgmgr] fetch rejected: sha256 mismatch for '%s'\n"
                        "         index:    %s\n         received: %s\n",
                entry->id, expected, actual);
        free(resp);
        return false;
    }

    /* Hash matched: write to a .part file, then rename atomically so a
     * crash mid-write can never leave a half payload in the cache. */
    char part[640];
    snprintf(part, sizeof(part), "%s.part", cache_path);
    FILE *f = fopen(part, "wb");
    if (!f) { free(resp); return false; }
    size_t wr = fwrite(resp, 1, body_len, f);
    int frc = fclose(f);
    free(resp);
    if (wr != body_len || frc != 0) { unlink(part); return false; }
    if (rename(part, cache_path) != 0) { unlink(part); return false; }

    pkgmgr_progress("download", entry->id, 1.0f, "payload downloaded and verified");
    return true;
}