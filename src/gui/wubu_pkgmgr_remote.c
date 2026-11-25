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

        /* Parse scheme://host[:port][/path] (shared splitter, fetch leaf). */
        char host[256], path[512];
        int port = 80;
        bool use_tls = false;
        if (!pkgmgr_split_url(url, host, sizeof(host), &port, path, sizeof(path), &use_tls))
            continue;
        if (!path[0]) snprintf(path, sizeof(path), "/index.json");

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

        /* Replace the repo's package index with freshly fetched entries.
         * repo_name is an operator/config string -- bind it. */
        sqlite3_stmt *del = NULL;
        bool del_ok = false;
        if (sqlite3_prepare_v2(g_pkgmgr.db,
                "DELETE FROM repo_packages WHERE repo_name=?1", -1, &del, NULL) == SQLITE_OK) {
            sqlite3_bind_text(del, 1, targets[t].name, -1, SQLITE_TRANSIENT);
            del_ok = sqlite3_step(del) == SQLITE_DONE;
            sqlite3_finalize(del);
        }
        if (!del_ok) continue;

        int inserted = pkgmgr_parse_index(body, targets[t].name);
        if (inserted < 0) continue;

        /* Stamp last_update only on a successful fetch+parse. */
        sqlite3_stmt *ustmt = NULL;
        bool upd_ok = false;
        if (sqlite3_prepare_v2(g_pkgmgr.db,
                "UPDATE repos SET last_update=strftime('%s','now') WHERE name=?1",
                -1, &ustmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(ustmt, 1, targets[t].name, -1, SQLITE_TRANSIENT);
            upd_ok = sqlite3_step(ustmt) == SQLITE_DONE;
            sqlite3_finalize(ustmt);
        }
        if (!upd_ok) continue;
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
            if (MATCH_KEY("id")) wubu_strlcpy(id,  buf, sizeof(id));
            else if (MATCH_KEY("name")) wubu_strlcpy(name,  buf, sizeof(name));
            else if (MATCH_KEY("version")) wubu_strlcpy(version,  buf, sizeof(version));
            else if (MATCH_KEY("arch")) wubu_strlcpy(arch,  buf, sizeof(arch));
            else if (MATCH_KEY("description")) wubu_strlcpy(desc,  buf, sizeof(desc));
            else if (MATCH_KEY("download_url")) wubu_strlcpy(durl,  buf, sizeof(durl));
            else if (MATCH_KEY("sha256")) wubu_strlcpy(sha,  buf, sizeof(sha));
            else if (MATCH_KEY("size")) size = (uint64_t)atoll(buf);
            #undef MATCH_KEY

            /* Advance to the next key or end of object. */
            while (*p && *p != ',' && *p != '}') p++;
            if (*p == ',') p++;
        }
        if (*p == '}') p++;

        /* Insert the parsed entry.  Bound parameters: index fields are
         * untrusted repo output and must never be interpolated into SQL.
         * download_url + sha256 are kept so install can fetch and verify. */
        if (id[0]) {
            sqlite3_stmt *ins = NULL;
            if (sqlite3_prepare_v2(g_pkgmgr.db,
                    "INSERT OR REPLACE INTO repo_packages "
                    "(id, name, version, arch, repo_name, description, deps, size_bytes, "
                    " download_url, sha256) "
                    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, '', ?7, ?8, ?9)",
                    -1, &ins, NULL) == SQLITE_OK) {
                sqlite3_bind_text(ins, 1, id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 2, name[0] ? name : id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 3, version, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 4, arch, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 5, repo_name, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 6, desc, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(ins, 7, (sqlite3_int64)size);
                sqlite3_bind_text(ins, 8, durl, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 9, sha, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(ins) == SQLITE_DONE) count++;
                sqlite3_finalize(ins);
            }
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

/* Callback: copy each repo_packages row into the caller's out array.
 * Column order: id,name,version,description,repo_name,size_bytes,
 *               download_url,sha256 */
static int cb_search(void *data, int argc, char **argv, char **col) {
    (void)argc; (void)col;
    pkgmgr_search_ctx_t *ctx = data;
    if (ctx->filled >= ctx->max) return 0; /* buffer full */
    wubu_pkg_repo_entry_t *e = &ctx->out[ctx->filled++];
    memset(e, 0, sizeof(*e));
    wubu_strlcpy(e->id,           argv[0] ? argv[0] : "", sizeof(e->id));
    wubu_strlcpy(e->name,         argv[1] ? argv[1] : "", sizeof(e->name));
    wubu_strlcpy(e->version,      argv[2] ? argv[2] : "", sizeof(e->version));
    wubu_strlcpy(e->description,  argv[3] ? argv[3] : "", sizeof(e->description));
    e->download_size = argv[5] ? (uint64_t)atoll(argv[5]) : 0;
    e->installed_size = e->download_size;
    wubu_strlcpy(e->download_url,  argv[6] ? argv[6] : "", sizeof(e->download_url));
    wubu_strlcpy(e->sha256,        argv[7] ? argv[7] : "", sizeof(e->sha256));
    e->arch = WUBU_PKG_ARCH_X86_64; /* repo index stores arch as TEXT; default */
    return 0;
}

int wubu_pkgmgr_search(const char *query, wubu_pkg_repo_entry_t *out, int max) {
    if (!g_pkgmgr.initialized || !query || !out || max <= 0) return 0;
    memset(out, 0, (size_t)max * sizeof(*out));
    pkgmgr_search_ctx_t ctx = { out, max, 0 };
    /* Search text is external UI input: bind the LIKE patterns, never
     * interpolate them into the SQL string. */
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "SELECT id,name,version,description,repo_name,size_bytes,download_url,sha256 "
            "FROM repo_packages WHERE id LIKE ?1 OR name LIKE ?2 "
            "OR description LIKE ?3 LIMIT ?4", -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return 0;
    }
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%%%s%%", query);
    for (int i = 1; i <= 3; i++) sqlite3_bind_text(stmt, i, pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, max);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *vals[8];
        for (int i = 0; i < 8; i++) vals[i] = (const char *)sqlite3_column_text(stmt, i);
        cb_search(&ctx, 8, (char **)vals, NULL);
    }
    sqlite3_finalize(stmt);
    return ctx.filled;
}

/* -- Single-entry lookup --------------------------------------------- */

typedef struct {
    wubu_pkg_repo_entry_t *out;
    bool found;
} pkgmgr_info_ctx_t;

/* Column order: id,name,version,description,download_url,sha256,
 *               size_bytes,arch */
static int cb_repo_info(void *data, int argc, char **argv, char **col) {
    (void)argc; (void)col;
    pkgmgr_info_ctx_t *ctx = data;
    if (ctx->found) return 0;
    wubu_pkg_repo_entry_t *e = ctx->out;
    memset(e, 0, sizeof(*e));
    wubu_strlcpy(e->id,            argv[0] ? argv[0] : "", sizeof(e->id));
    wubu_strlcpy(e->name,          argv[1] ? argv[1] : "", sizeof(e->name));
    wubu_strlcpy(e->version,       argv[2] ? argv[2] : "", sizeof(e->version));
    wubu_strlcpy(e->description,   argv[3] ? argv[3] : "", sizeof(e->description));
    wubu_strlcpy(e->download_url,  argv[4] ? argv[4] : "", sizeof(e->download_url));
    wubu_strlcpy(e->sha256,        argv[5] ? argv[5] : "", sizeof(e->sha256));
    e->download_size = argv[6] ? (uint64_t)atoll(argv[6]) : 0;
    e->installed_size = e->download_size;
    const char *arch = argv[7] ? argv[7] : "";
    if (strcmp(arch, "aarch64") == 0) e->arch = WUBU_PKG_ARCH_AARCH64;
    else if (strcmp(arch, "riscv64") == 0) e->arch = WUBU_PKG_ARCH_RISCV64;
    else if (strcmp(arch, "any") == 0) e->arch = WUBU_PKG_ARCH_ANY;
    else e->arch = WUBU_PKG_ARCH_X86_64;
    ctx->found = true;
    return 0;
}

bool wubu_pkgmgr_repo_get_info(const char *pkg_id, wubu_pkg_repo_entry_t *out) {
    if (!g_pkgmgr.initialized || !pkg_id || !out) return false;
    memset(out, 0, sizeof(*out));
    /* Bound parameter + a REAL callback: the old code passed NULL to
     * db_query, so it returned true while leaving *out untouched. */
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "SELECT id,name,version,description,download_url,sha256,size_bytes,arch "
            "FROM repo_packages WHERE id=?1 LIMIT 1", -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_text(stmt, 1, pkg_id, -1, SQLITE_TRANSIENT);
    pkgmgr_info_ctx_t ctx = { out, false };
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *vals[8];
        for (int i = 0; i < 8; i++)
            vals[i] = (const char *)sqlite3_column_text(stmt, i);
        cb_repo_info(&ctx, 8, (char **)vals, NULL);
    }
    sqlite3_finalize(stmt);
    return ctx.found;
}
