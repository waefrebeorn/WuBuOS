/*
 * wubu_pkgmgr_revoke_sync.c -- bounded signed cross-device revocation-feed sync.
 *
 * Transport + trust policy only.  The revocation DB is touched in exactly one
 * place: the call to wubu_pkgmgr_key_feed_import() at the end, which verifies
 * the admin/quorum signature over the exact fetched bytes BEFORE beginning its
 * single all-or-nothing transaction.  A refusal here never reaches that call.
 *
 * HTTP is done through the existing OCI HTTP client (oci_http_connect/send/
 * recv), so https:// inherits its REQUIRED TLS chain + hostname verification.
 */
#include "wubu_pkgmgr_revoke_sync.h"
#include "wubu_pkgmgr.h"
#include "wubu_settings.h"
#include "../runtime/oci/oci_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* Response scratch = feed cap + room for status/headers.  oci_http_recv()
 * takes the buffer size as a parameter, so a 64 KiB feed is not limited by
 * OCI_HTTP_BUF_SIZE (which only bounds the OCI request builder). */
#define SYNC_HEADROOM 16384u
#define SYNC_SIG_MAX  512u
#define SYNC_SIG_BUF  1024u

static void sync_seterr(char *err, size_t errsz, const char *msg) {
    if (err && errsz) {
        strncpy(err, msg, errsz - 1);
        err[errsz - 1] = '\0';
    }
}

void wubu_pkg_sync_config_init(wubu_pkg_sync_config_t *cfg) {
    if (cfg) memset(cfg, 0, sizeof(*cfg));
}

static bool config_add_url(wubu_pkg_sync_config_t *cfg, const char *url) {
    size_t len;
    if (!cfg || !url) return false;
    len = strlen(url);
    if (len == 0 || len >= WUBU_PKGMGR_SYNC_URL_MAX) return false;
    if (cfg->n >= (int)WUBU_PKGMGR_MAX_SYNC_URLS) return false;
    for (int i = 0; i < cfg->n; i++)
        if (strcmp(cfg->urls[i], url) == 0) return true;   /* idempotent */
    memcpy(cfg->urls[cfg->n], url, len + 1);
    cfg->n++;
    return true;
}

/* Add comma-separated URLs from a string (env var or CLI override). */
static bool config_add_csv(wubu_pkg_sync_config_t *cfg, const char *s) {
    char buf[WUBU_PKGMGR_MAX_SYNC_URLS * WUBU_PKGMGR_SYNC_URL_MAX + 16];
    char *tok;
    if (!s || !*s) return true;
    if (strlen(s) >= sizeof(buf)) return false;
    strcpy(buf, s);
    tok = strtok(buf, ",");
    while (tok) {
        char *e;
        while (*tok == ' ' || *tok == '\t') tok++;
        e = tok + strlen(tok);
        while (e > tok && (e[-1] == ' ' || e[-1] == '\t' ||
                           e[-1] == '\r' || e[-1] == '\n')) *(--e) = '\0';
        if (*tok && !config_add_url(cfg, tok)) return false;
        tok = strtok(NULL, ",");
    }
    return true;
}

int wubu_pkg_sync_config_load(const char *path, const char *extra,
                              wubu_pkg_sync_config_t *cfg) {
    FILE *f;
    if (!cfg) return -1;
    if (path && *path && (f = fopen(path, "r")) != NULL) {
        char line[WUBU_PKGMGR_SYNC_URL_MAX + 64];
        const char key[] = "trusted_sync_url";
        while (fgets(line, sizeof(line), f)) {
            char *p = line, *e;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#') continue;
            if (strncmp(p, key, sizeof(key) - 1) != 0) { fclose(f); return -1; }
            p += sizeof(key) - 1;
            while (*p == ' ' || *p == '\t') p++;
            if (*p != '=') { fclose(f); return -1; }
            p++;
            while (*p == ' ' || *p == '\t') p++;
            e = p + strlen(p);
            while (e > p && (e[-1] == '\n' || e[-1] == '\r' ||
                             e[-1] == ' ' || e[-1] == '\t')) *(--e) = '\0';
            if (!config_add_url(cfg, p)) { fclose(f); return -1; }
        }
        fclose(f);
    }
    /* A missing config file simply means no file-configured URLs. */
    if (!config_add_csv(cfg, getenv("WUBU_PKGMGR_SYNC_URLS"))) return -1;
    if (!config_add_csv(cfg, extra)) return -1;
    /* GUI settings store is a first-class read path, not just config/env/CLI.
     * Callers initialise settings before sync; an empty/uninitialised store
     * simply contributes no URLs. */
    if (!config_add_csv(cfg, wubu_settings_trusted_sync_url())) return -1;
    return cfg->n;
}

bool wubu_pkg_sync_url_trusted(const wubu_pkg_sync_config_t *cfg, const char *url) {
    if (!cfg || !url || !*url) return false;
    for (int i = 0; i < cfg->n; i++)
        if (strcmp(cfg->urls[i], url) == 0) return true;
    return false;
}

typedef struct {
    bool  tls;
    char  host[256];
    int   port;
    char  path[512];
} sync_url_t;

/* Strict URL grammar: scheme + host[:port] + absolute path only.  Rejects
 * userinfo, fragments, queries, dot-dot, non-ASCII/control bytes. */
static bool url_parse(const char *url, sync_url_t *u, char *err, size_t errsz) {
    const char *rest, *path, *ps;
    char hp[256];
    size_t len, hplen, hl, pl;
    char *colon;
    long port;

    if (!url || !*url) { sync_seterr(err, errsz, "no sync url"); return false; }
    len = strlen(url);
    if (len >= WUBU_PKGMGR_SYNC_URL_MAX) {
        sync_seterr(err, errsz, "sync url too long"); return false;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)url[i];
        if (c <= 0x20 || c >= 0x7f) {
            sync_seterr(err, errsz, "illegal byte in sync url"); return false;
        }
    }
    if (strncmp(url, "https://", 8) == 0) {
        u->tls = true; u->port = 443; rest = url + 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        u->tls = false; u->port = 80; rest = url + 7;
    } else {
        sync_seterr(err, errsz, "unsupported url scheme"); return false;
    }

    hplen = strcspn(rest, "/");
    if (hplen == 0 || hplen >= sizeof(hp)) {
        sync_seterr(err, errsz, "bad sync url host"); return false;
    }
    memcpy(hp, rest, hplen); hp[hplen] = '\0';
    if (strchr(hp, '@')) {
        sync_seterr(err, errsz, "userinfo not allowed in sync url"); return false;
    }
    colon = strchr(hp, ':');
    if (colon) {
        if (strchr(colon + 1, ':')) {
            sync_seterr(err, errsz, "bad sync url port"); return false;
        }
        *colon = '\0';
        ps = colon + 1;
        if (!*ps) { sync_seterr(err, errsz, "bad sync url port"); return false; }
        port = 0;
        for (const char *q = ps; *q; q++) {
            if (*q < '0' || *q > '9') {
                sync_seterr(err, errsz, "bad sync url port"); return false;
            }
            port = port * 10 + (*q - '0');
            if (port > 65535) {
                sync_seterr(err, errsz, "bad sync url port"); return false;
            }
        }
        if (port == 0) { sync_seterr(err, errsz, "bad sync url port"); return false; }
        u->port = (int)port;
    }
    hl = strlen(hp);
    if (hl == 0 || hl >= sizeof(u->host)) {
        sync_seterr(err, errsz, "bad sync url host"); return false;
    }
    for (size_t i = 0; i < hl; i++) {
        unsigned char c = (unsigned char)hp[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-')) {
            sync_seterr(err, errsz, "bad sync url host"); return false;
        }
    }
    memcpy(u->host, hp, hl + 1);

    path = rest + hplen;
    if (*path == '\0') {
        strcpy(u->path, "/");
    } else {
        pl = strlen(path);
        if (path[0] != '/' || pl >= sizeof(u->path) ||
            strchr(path, '?') || strchr(path, '#') || strstr(path, "..")) {
            sync_seterr(err, errsz, "bad sync url path"); return false;
        }
        memcpy(u->path, path, pl + 1);
    }
    return true;
}

/* One bounded GET through the OCI HTTP client.  `resp`/`resp_cap` scratch,
 * `out`/`out_cap` receive the exact body bytes with *out_len set.  Rejects
 * redirects, non-200, missing/oversize Content-Length, and empty bodies. */
static bool http_get(const char *host, int port, bool tls, const char *path,
                     size_t max_body, uint8_t *resp, size_t resp_cap,
                     uint8_t *out, size_t out_cap, size_t *out_len,
                     char *err, size_t errsz) {
    OciHttpClient c;
    char req[2048], hosthdr[300];
    char *status, *sp, *hd, *line;
    int n, code = -1;
    size_t recv_len = 0, body_len, clen = 0;
    bool have_cl = false;
    uint8_t *body;

    *out_len = 0;
    memset(&c, 0, sizeof(c));
    if (oci_http_connect(&c, host, port, tls) != 0) {
        oci_http_close(&c);
        sync_seterr(err, errsz,
                    tls ? "TLS connect/verify failed or endpoint unreachable"
                        : "endpoint unreachable");
        return false;
    }
    if (port == 80 || port == 443)
        snprintf(hosthdr, sizeof(hosthdr), "%s", host);
    else
        snprintf(hosthdr, sizeof(hosthdr), "%s:%d", host, port);
    n = snprintf(req, sizeof(req),
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "User-Agent: WuBuOS-RevSync/1.0\r\n"
                 "Accept: application/octet-stream\r\n"
                 "Connection: close\r\n\r\n", path, hosthdr);
    if (n < 0 || (size_t)n >= sizeof(req)) {
        oci_http_close(&c); sync_seterr(err, errsz, "sync request too large");
        return false;
    }
    if (oci_http_send(&c, req, (size_t)n) != 0) {
        oci_http_close(&c); sync_seterr(err, errsz, "sync send failed");
        return false;
    }
    if (oci_http_recv(&c, resp, resp_cap, &recv_len) != 0 || recv_len == 0) {
        oci_http_close(&c); sync_seterr(err, errsz, "no sync response");
        return false;
    }
    oci_http_close(&c);

    status = strstr((char *)resp, "HTTP/");
    if (status && (sp = strchr(status, ' ')) != NULL &&
        sp[1] >= '0' && sp[1] <= '9')
        code = atoi(sp + 1);
    if (code < 0) { sync_seterr(err, errsz, "bad HTTP response"); return false; }
    if (code >= 300 && code < 400) {
        sync_seterr(err, errsz, "redirect refused"); return false;
    }
    if (code != 200) { sync_seterr(err, errsz, "HTTP error"); return false; }

    hd = strstr((char *)resp, "\r\n\r\n");
    if (!hd) { sync_seterr(err, errsz, "malformed HTTP response"); return false; }
    for (line = (char *)resp; line < hd; ) {
        char *e = strstr(line, "\r\n");
        if (!e || e > hd) break;
        if (strncasecmp(line, "Content-Length:", 15) == 0) {
            const char *v = line + 15;
            while (*v == ' ') v++;
            if (!isdigit((unsigned char)*v)) {
                sync_seterr(err, errsz, "bad Content-Length"); return false;
            }
            clen = (size_t)strtoull(v, NULL, 10);
            have_cl = true;
        }
        line = e + 2;
    }
    if (!have_cl) {
        sync_seterr(err, errsz, "missing Content-Length"); return false;
    }
    if (clen > max_body || clen > out_cap) {
        sync_seterr(err, errsz, "oversize sync body"); return false;
    }
    body = (uint8_t *)hd + 4;
    body_len = recv_len - (size_t)(body - resp);
    if (body_len < clen) { sync_seterr(err, errsz, "short sync body"); return false; }
    if (clen == 0) { sync_seterr(err, errsz, "empty sync body"); return false; }
    memcpy(out, body, clen);
    *out_len = clen;
    return true;
}

/* Hand the exact bytes + detached signature to the existing importer. */
static bool import_temp(const uint8_t *data, size_t len, const char *sig_hex,
                        size_t *out_applied, char *err, size_t errsz) {
    const char *tmp = getenv("TMPDIR");
    char tpl[512];
    int fd;
    size_t off = 0;
    bool ok;

    if (!tmp || !*tmp) tmp = "/tmp";
    snprintf(tpl, sizeof(tpl), "%s/wubu-revsync-XXXXXX", tmp);
    fd = mkstemp(tpl);
    if (fd < 0) { sync_seterr(err, errsz, "cannot create temp feed"); return false; }
    while (off < len) {
        ssize_t w = write(fd, data + off, len - off);
        if (w <= 0) {
            close(fd); unlink(tpl);
            sync_seterr(err, errsz, "cannot write temp feed"); return false;
        }
        off += (size_t)w;
    }
    close(fd);
    ok = wubu_pkgmgr_key_feed_import(tpl, sig_hex, out_applied, err, errsz);
    unlink(tpl);
    return ok;
}

bool wubu_pkgmgr_key_feed_sync(const char *url,
                               const wubu_pkg_sync_config_t *cfg,
                               size_t *out_applied, char *err, size_t errsz) {
    sync_url_t u;
    uint8_t *resp = NULL, *feed = NULL, *sigbuf = NULL;
    size_t resp_cap, feed_len = 0, sig_len = 0;
    char sigpath[sizeof(u.path)], sig_hex[129];
    bool ok = false;

    if (out_applied) *out_applied = 0;
    sync_seterr(err, errsz, "");
    if (!url || !*url) { sync_seterr(err, errsz, "no sync url"); return false; }
    if (!wubu_pkg_sync_url_trusted(cfg, url)) {
        sync_seterr(err, errsz, "sync url not in trusted sync config"); return false;
    }
    if (!url_parse(url, &u, err, errsz)) return false;

    resp_cap = WUBU_PKGMGR_FEED_MAX_BYTES + SYNC_HEADROOM;
    resp = (uint8_t *)malloc(resp_cap);
    feed = (uint8_t *)malloc(WUBU_PKGMGR_FEED_MAX_BYTES + 1);
    sigbuf = (uint8_t *)malloc(SYNC_SIG_BUF);
    if (!resp || !feed || !sigbuf) {
        sync_seterr(err, errsz, "out of memory"); goto cleanup;
    }

    /* Fetch the exact feed body (bounded to the importer's byte cap). */
    if (!http_get(u.host, u.port, u.tls, u.path, WUBU_PKGMGR_FEED_MAX_BYTES,
                  resp, resp_cap, feed, WUBU_PKGMGR_FEED_MAX_BYTES + 1,
                  &feed_len, err, errsz))
        goto cleanup;

    /* Detached signature sidecar, derived from the SAME trusted host/path. */
    if (strlen(u.path) + 4 >= sizeof(u.path)) {
        sync_seterr(err, errsz, "sync sig path too long"); goto cleanup;
    }
    snprintf(sigpath, sizeof(sigpath), "%s.sig", u.path);
    if (!http_get(u.host, u.port, u.tls, sigpath, SYNC_SIG_MAX,
                  resp, resp_cap, sigbuf, SYNC_SIG_BUF, &sig_len, err, errsz))
        goto cleanup;

    while (sig_len > 0 && (sigbuf[sig_len - 1] == '\n' ||
                           sigbuf[sig_len - 1] == '\r' ||
                           sigbuf[sig_len - 1] == ' ' ||
                           sigbuf[sig_len - 1] == '\t')) sig_len--;
    if (sig_len != 2 * WUBU_PKG_SIG_SIZE) {
        sync_seterr(err, errsz, "bad detached signature length"); goto cleanup;
    }
    for (size_t i = 0; i < 2 * WUBU_PKG_SIG_SIZE; i++) {
        unsigned char c = sigbuf[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            sync_seterr(err, errsz, "bad detached signature hex"); goto cleanup;
        }
        sig_hex[i] = (char)c;
    }
    sig_hex[2 * WUBU_PKG_SIG_SIZE] = '\0';

    /* The importer verifies this signature before its single transaction. */
    ok = import_temp(feed, feed_len, sig_hex, out_applied, err, errsz);

cleanup:
    free(sigbuf);
    free(feed);
    free(resp);
    return ok;
}
