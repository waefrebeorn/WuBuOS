/* wubu_pkgmgr_remote.c -- Remote repository index subsystem.
 *
 * Self-contained module: fetches a repo's index.json over HTTP (reusing
 * oci_http_client.c), parses it into the local repo_packages table, and
 * serves package-search / lookup queries against that table.
 *
 * Discipline:
 *   - includes ONLY wubu_pkgmgr_internal.h (which pulls in the public
 *     wubu_pkgmgr.h types, oci_internal.h for the HTTP client, sqlite3,
 *     shared helpers). No god headers, no redundant includes.
 *   - uses the shared g_pkgmgr state + db_exec/db_query helpers declared in
 *     the internal header; does not reach into wubu_pkgmgr.c internals.
 *   - public API (wubu_pkgmgr_repo_update / _search / _repo_get_info) is
 *     declared in wubu_pkgmgr.h; the JSON parser is private (static).
 */

#include "wubu_pkgmgr_internal.h"
/* Forward declaration (defined below repo_update). */
static int pkgmgr_parse_index(const char *json, const char *repo_name);

bool wubu_pkgmgr_repo_update(const char *name) {
    if (!g_pkgmgr.initialized) return false;

    /* Determine which repos to refresh. */
    wubu_pkg_repo_t targets[32];
    int n_targets = 0;
    for (int i = 0; i < g_pkgmgr.n_repos && n_targets < 32; i++) {
        if (name) {
            if (strcmp(g_pkgmgr.repos[i].name, name) == 0)
                targets[n_targets++] = g_pkgmgr.repos[i];
        } else if (g_pkgmgr.repos[i].enabled) {
            targets[n_targets++] = g_pkgmgr.repos[i];
        }
    }
    if (n_targets == 0) return false;

    int updated = 0;
    for (int t = 0; t < n_targets; t++) {
        const char *url = targets[t].url;
        if (!url || !url[0]) continue;

        /* Parse scheme://host[:port][/path] */
        const char *scheme_end = strstr(url, "://");
        const char *host_start = scheme_end ? scheme_end + 3 : url;
        bool use_tls = scheme_end && strncmp(url, "https", 5) == 0;
        const char *slash = strchr(host_start, '/');
        const char *host_end = slash ? slash : host_start + strlen(host_start);
        char host[256];
        size_t hlen = host_end - host_start;
        if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, host_start, hlen);
        host[hlen] = '\0';
        int port = use_tls ? 443 : 80;
        char *colon = strrchr(host, ':');
        if (colon) { port = atoi(colon + 1); *colon = '\0'; }
        const char *path = slash ? slash : "/index.json";

        OciHttpClient cli;
        memset(&cli, 0, sizeof(cli));
        cli.sockfd = -1;
        if (oci_http_connect(&cli, host, port, use_tls) != 0) continue;

        char resp[OCI_HTTP_BUF_SIZE * 4];
        int rc = oci_http_request(&cli, "GET", path, NULL, NULL, 0, resp, sizeof(resp));
        oci_http_close(&cli);
        if (rc != 0) continue;

        /* oci_http_request() strips the HTTP headers and returns only the
         * body (the JSON index) in resp, null-terminated. */
        const char *body = resp;

        /* Replace the repo's package index with freshly fetched entries. */
        char del[512];
        snprintf(del, sizeof(del), "DELETE FROM repo_packages WHERE repo_name='%s'",
                 targets[t].name);
        if (db_exec(del) != 0) continue;

        int inserted = pkgmgr_parse_index(body, targets[t].name);
        if (inserted < 0) continue;

        /* Stamp last_update only on a successful fetch+parse. */
        char sql[256];
        snprintf(sql, sizeof(sql),
                 "UPDATE repos SET last_update=strftime('%%s','now') WHERE name='%s'",
                 targets[t].name);
        if (db_exec(sql) != 0) continue;
        /* Keep the in-memory repo state consistent with the DB stamp so
         * callers reading via wubu_pkgmgr_repo_list() see it immediately.
         * targets[] is a copy; update the canonical g_pkgmgr.repos entry. */
        for (int k = 0; k < g_pkgmgr.n_repos; k++) {
            if (strcmp(g_pkgmgr.repos[k].name, targets[t].name) == 0) {
                g_pkgmgr.repos[k].last_update = (int64_t)time(NULL);
                break;
            }
        }
        updated++;
        pkgmgr_progress("repo_update", targets[t].name, 1.0f, "index refreshed");
    }

    return updated > 0;
}

/* Minimal, dependency-free parser for a JSON array of package index entries.
 * Schema (one object per package):
 *   { "id":"..", "name":"..", "version":"..", "arch":"x86_64",
 *     "description":"..", "download_url":"..", "sha256":"..", "size":N }
 * Returns the number of entries inserted into repo_packages, or -1 on error.
 * Robust enough for trusted repo output; does not validate signatures. */
static int pkgmgr_parse_index(const char *json, const char *repo_name) {
    if (!json) return -1;
    const char *p = json;
    /* Find the opening '[' */
    while (*p && *p != '[') p++;
    if (*p != '[') return -1;
    p++;

    int count = 0;
    while (*p) {
        /* Skip to next object start. */
        while (*p && *p != '{' && *p != ']') p++;
        if (*p == ']') break;
        if (*p != '{') break;
        p++;

        char id[128] = {0}, name[128] = {0}, version[64] = {0}, arch[32] = {0};
        char desc[256] = {0}, durl[512] = {0}, sha[65] = {0};
        uint64_t size = 0;

        /* Walk key/value pairs until the matching '}'. */
        while (*p && *p != '}') {
            while (*p && *p != '"' && *p != '}') p++;
            if (*p == '}') break;
            p++; /* skip opening quote of key */
            const char *key = p;
            while (*p && *p != '"') p++;
            if (!*p) break;
            size_t klen = p - key;
            p++; /* skip closing quote */
            while (*p && *p != ':') p++;
            if (!*p) break;
            p++;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

            char buf[512];
            bool is_str = (*p == '"');
            const char *val_start = NULL;
            size_t vlen = 0;
            if (is_str) {
                p++;
                val_start = p;
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p + 1)) p++; /* skip escape */
                    p++;
                }
                vlen = p - val_start;
                if (*p == '"') p++;
            } else {
                val_start = p;
                while (*p && *p != ',' && *p != '}' && *p != '\n') p++;
                vlen = p - val_start;
            }
            if (vlen >= sizeof(buf)) vlen = sizeof(buf) - 1;
            memcpy(buf, val_start, vlen);
            buf[vlen] = '\0';

            /* Strip a trailing comma/space from string values. */
            if (is_str) {
                while (vlen > 0 && (buf[vlen - 1] == ',' || buf[vlen - 1] == ' '))
                    buf[--vlen] = '\0';
            }

            #define MATCH_KEY(lit) (klen == strlen(lit) && strncmp(key, lit, klen) == 0)
            if (MATCH_KEY("id")) strncpy(id, buf, sizeof(id) - 1);
            else if (MATCH_KEY("name")) strncpy(name, buf, sizeof(name) - 1);
            else if (MATCH_KEY("version")) strncpy(version, buf, sizeof(version) - 1);
            else if (MATCH_KEY("arch")) strncpy(arch, buf, sizeof(arch) - 1);
            else if (MATCH_KEY("description")) strncpy(desc, buf, sizeof(desc) - 1);
            else if (MATCH_KEY("download_url")) strncpy(durl, buf, sizeof(durl) - 1);
            else if (MATCH_KEY("sha256")) strncpy(sha, buf, sizeof(sha) - 1);
            else if (MATCH_KEY("size")) size = (uint64_t)atoll(buf);
            #undef MATCH_KEY

            /* Advance to the next key or end of object. */
            while (*p && *p != ',' && *p != '}') p++;
            if (*p == ',') p++;
        }
        if (*p == '}') p++;

        /* Insert the parsed entry. */
        if (id[0]) {
            char ins[1024];
            snprintf(ins, sizeof(ins),
                "INSERT OR REPLACE INTO repo_packages "
                "(id, name, version, arch, repo_name, description, deps, size_bytes) "
                "VALUES ('%s','%s','%s','%s','%s','%s','',%llu)",
                id, name[0] ? name : id, version, arch, repo_name, desc,
                (unsigned long long)size);
            if (db_exec(ins) == 0) count++;
        }
    }
    return count;
}

/* -- Search -------------------------------------------------------- */

/* Context passed to the search callback so it knows the buffer size and can
 * track how many entries it has filled. */
typedef struct {
    wubu_pkg_repo_entry_t *out;
    int max;
    int filled;
} pkgmgr_search_ctx_t;

/* Callback: copy each repo_packages row into the caller's out array. */
static int cb_search(void *data, int argc, char **argv, char **col) {
    (void)argc; (void)col;
    pkgmgr_search_ctx_t *ctx = data;
    if (ctx->filled >= ctx->max) return 0; /* buffer full */
    wubu_pkg_repo_entry_t *e = &ctx->out[ctx->filled++];
    memset(e, 0, sizeof(*e));
    strncpy(e->id,          argv[0] ? argv[0] : "", sizeof(e->id) - 1);
    strncpy(e->name,        argv[1] ? argv[1] : "", sizeof(e->name) - 1);
    strncpy(e->version,     argv[2] ? argv[2] : "", sizeof(e->version) - 1);
    strncpy(e->description, argv[3] ? argv[3] : "", sizeof(e->description) - 1);
    e->download_size = argv[5] ? (uint64_t)atoll(argv[5]) : 0;
    e->installed_size = e->download_size;
    e->arch = WUBU_PKG_ARCH_X86_64; /* repo index stores arch as TEXT; default */
    return 0;
}

int wubu_pkgmgr_search(const char *query, wubu_pkg_repo_entry_t *out, int max) {
    if (!g_pkgmgr.initialized || !query || !out || max <= 0) return 0;
    memset(out, 0, (size_t)max * sizeof(*out));
    pkgmgr_search_ctx_t ctx = { out, max, 0 };
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT id,name,version,description,repo_name,size_bytes "
        "FROM repo_packages WHERE id LIKE '%%%s%%' OR name LIKE '%%%s%%' "
        "OR description LIKE '%%%s%%' LIMIT %d",
        query, query, query, max);
    if (db_query(sql, cb_search, &ctx) != 0) return 0;
    return ctx.filled;
}

bool wubu_pkgmgr_repo_get_info(const char *pkg_id, wubu_pkg_repo_entry_t *out) {
    if (!g_pkgmgr.initialized || !pkg_id || !out) return false;
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT * FROM repo_packages WHERE id='%s' LIMIT 1", pkg_id);
    return db_query(sql, NULL, out) == 0;
}
