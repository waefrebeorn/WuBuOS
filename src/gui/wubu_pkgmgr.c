/*
 * wubu_pkgmgr.c  --  WuBuOS Package Manager (Facade)
 *
 * Submodules:
 *   wubu_pkgmgr_pkg.c     - package create/verify/extract/read-manifest
 *   wubu_pkgmgr_install.c - install/remove/upgrade/list
 *   wubu_pkgmgr_txn.c     - transactions, desktop registration
 */

#include "wubu_pkgmgr_internal.h"

/* -- Global State -------------------------------------------------- */

wubu_pkgmgr_state_t g_pkgmgr;

/* -- Progress ------------------------------------------------------ */

void pkgmgr_progress(const char *stage, const char *pkg_id, float progress, const char *msg) {
    if (g_pkgmgr.progress_cb)
        g_pkgmgr.progress_cb(g_pkgmgr.progress_userdata, stage, pkg_id, progress, msg);
}

/* -- DB Helpers ---------------------------------------------------- */

static int64_t db_exec_raw(const char *sql) {
    if (!g_pkgmgr.db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(g_pkgmgr.db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        if (err) { fprintf(stderr, "SQLite error: %s\n", err); sqlite3_free(err); }
        return -1;
    }
    return 0;
}

static int db_query_raw(const char *sql, int (*callback)(void*, int, char**, char**), void *data) {
    if (!g_pkgmgr.db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(g_pkgmgr.db, sql, callback, data, &err);
    if (rc != SQLITE_OK) {
        if (err) { fprintf(stderr, "SQLite error: %s\n", err); sqlite3_free(err); }
        return -1;
    }
    return 0;
}

/* Non-static wrappers for submodule use */
int64_t db_exec(const char *sql) { return db_exec_raw(sql); }
int db_query(const char *sql, int (*callback)(void*, int, char**, char**), void *data) {
    return db_query_raw(sql, callback, data);
}

/* -- SQLite Callbacks ---------------------------------------------- */

static int cb_list_installed_idx;

static int cb_load_repos(void *data, int argc, char **argv, char **col) {
    (void)data; (void)col;
    if (argc >= 5 && g_pkgmgr.n_repos < 32) {
        wubu_pkg_repo_t *r = &g_pkgmgr.repos[g_pkgmgr.n_repos++];
        strncpy(r->name, argv[0] ? argv[0] : "", sizeof(r->name) - 1);
        strncpy(r->url, argv[1] ? argv[1] : "", sizeof(r->url) - 1);
        strncpy(r->pubkey, argv[2] ? argv[2] : "", sizeof(r->pubkey) - 1);
        r->priority = argv[3] ? atoi(argv[3]) : 0;
        r->enabled = argv[4] ? atoi(argv[4]) : 1;
        r->last_update = argv[5] ? atoll(argv[5]) : 0;
        r->packages = NULL;
        r->n_packages = 0;
    }
    return 0;
}

static int cb_list_installed(void *data, int argc, char **argv, char **col) {
    (void)col;
    wubu_pkg_installed_t *buffer = data;
    if (argc < 7) return 0;
    wubu_pkg_installed_t *pkg = &buffer[cb_list_installed_idx++];
    memset(pkg, 0, sizeof(*pkg));
    strncpy(pkg->manifest.id, argv[0] ? argv[0] : "", sizeof(pkg->manifest.id) - 1);
    strncpy(pkg->manifest.name, argv[1] ? argv[1] : "", sizeof(pkg->manifest.name) - 1);
    strncpy(pkg->manifest.version, argv[2] ? argv[2] : "", sizeof(pkg->manifest.version) - 1);
    pkg->manifest.arch = (wubu_pkg_arch_t)(argv[3] ? atoi(argv[3]) : 0);
    strncpy(pkg->install_date, argv[4] ? argv[4] : "", sizeof(pkg->install_date) - 1);
    pkg->size_bytes = atoll(argv[5] ? argv[5] : "0");
    strncpy(pkg->install_path, argv[6] ? argv[6] : "", sizeof(pkg->install_path) - 1);
    return 0;
}

/* -- DB Schema ----------------------------------------------------- */

static const char SCHEMA_SQL[] =
    "CREATE TABLE IF NOT EXISTS packages ("
    "  id TEXT PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  version TEXT NOT NULL,"
    "  arch TEXT DEFAULT 'x86_64',"
    "  install_date TEXT NOT NULL,"
    "  size_bytes INTEGER DEFAULT 0,"
    "  install_path TEXT,"
    "  pkg_path TEXT,"
    "  signature TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS repos ("
    "  name TEXT PRIMARY KEY,"
    "  url TEXT NOT NULL,"
    "  pubkey_hex TEXT,"
    "  priority INTEGER DEFAULT 0,"
    "  enabled INTEGER DEFAULT 1,"
    "  last_update INTEGER DEFAULT 0"
    ");"
    "CREATE TABLE IF NOT EXISTS repo_packages ("
    "  id TEXT,"
    "  name TEXT,"
    "  version TEXT,"
    "  arch TEXT,"
    "  repo_name TEXT,"
    "  description TEXT,"
    "  deps TEXT,"
    "  size_bytes INTEGER,"
    "  PRIMARY KEY (id, repo_name)"
    ");"
    "CREATE TABLE IF NOT EXISTS deps ("
    "  pkg_id TEXT,"
    "  dep_id TEXT,"
    "  dep_version TEXT,"
    "  optional INTEGER DEFAULT 0,"
    "  PRIMARY KEY (pkg_id, dep_id)"
    ");"
    "CREATE TABLE IF NOT EXISTS files ("
    "  pkg_id TEXT,"
    "  path TEXT,"
    "  size INTEGER,"
    "  sha256 TEXT,"
    "  PRIMARY KEY (pkg_id, path)"
    ");"
    "CREATE TABLE IF NOT EXISTS desktop_entries ("
    "  pkg_id TEXT,"
    "  entry_idx INTEGER,"
    "  name TEXT,"
    "  exec_path TEXT,"
    "  icon_path TEXT,"
    "  categories TEXT,"
    "  PRIMARY KEY (pkg_id, entry_idx)"
    ");";

/* -- Initialize ---------------------------------------------------- */

bool wubu_pkgmgr_init(const wubu_pkgmgr_config_t *config) {
    if (g_pkgmgr.initialized) return true;
    memcpy(&g_pkgmgr.config, config, sizeof(*config));
    ensure_dir(g_pkgmgr.config.db_path);
    ensure_dir(g_pkgmgr.config.cache_dir);
    ensure_dir(g_pkgmgr.config.install_prefix);
    ensure_dir(g_pkgmgr.config.repo_config_dir);

    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/pkgmgr.db", g_pkgmgr.config.db_path);
    int rc = sqlite3_open(db_path, &g_pkgmgr.db);
    if (rc != SQLITE_OK) return false;
    if (db_exec(SCHEMA_SQL) != 0) { sqlite3_close(g_pkgmgr.db); return false; }

    /* Load repos */
    g_pkgmgr.n_repos = 0;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT name, url, pubkey_hex, priority, enabled, last_update FROM repos");
    db_query(sql, cb_load_repos, NULL);

    g_pkgmgr.initialized = true;
    return true;
}

void wubu_pkgmgr_shutdown(void) {
    if (g_pkgmgr.db) { sqlite3_close(g_pkgmgr.db); g_pkgmgr.db = NULL; }
    g_pkgmgr.initialized = false;
}

void wubu_pkgmgr_get_default_config(wubu_pkgmgr_config_t *config) {
    memset(config, 0, sizeof(*config));
    strncpy(config->db_path, "/var/wubu/pkgmgr", sizeof(config->db_path) - 1);
    strncpy(config->cache_dir, "/var/wubu/pkgmgr/cache", sizeof(config->cache_dir) - 1);
    strncpy(config->install_prefix, "/wubu/packages", sizeof(config->install_prefix) - 1);
    strncpy(config->repo_config_dir, "/var/wubu/pkgmgr/repos.d", sizeof(config->repo_config_dir) - 1);
    config->max_parallel_downloads = 4;
    config->verify_signatures = true;
    config->allow_untrusted = false;
    config->auto_update_index = true;
    config->index_ttl_hours = 24;
}

/* -- Repo Management ----------------------------------------------- */

bool wubu_pkgmgr_repo_add(const char *name, const char *url, const char *pubkey_hex, int priority) {
    if (!g_pkgmgr.initialized || !name || !url) return false;
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "INSERT OR REPLACE INTO repos (name, url, pubkey_hex, priority, enabled) "
        "VALUES ('%s', '%s', '%s', %d, 1)",
        name, url, pubkey_hex ? pubkey_hex : "", priority);
    if (db_exec(sql) != 0) return false;

    /* Reload repos */
    wubu_pkgmgr_shutdown();
    wubu_pkgmgr_init(&g_pkgmgr.config);
    return true;
}

bool wubu_pkgmgr_repo_remove(const char *name) {
    if (!g_pkgmgr.initialized || !name) return false;
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM repos WHERE name='%s'", name);
    if (db_exec(sql) != 0) return false;
    snprintf(sql, sizeof(sql), "DELETE FROM repo_packages WHERE repo_name='%s'", name);
    db_exec(sql);
    return true;
}

bool wubu_pkgmgr_repo_set_enabled(const char *name, bool enabled) {
    if (!g_pkgmgr.initialized || !name) return false;
    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE repos SET enabled=%d WHERE name='%s'", enabled ? 1 : 0, name);
    return db_exec(sql) == 0;
}

int wubu_pkgmgr_repo_list(wubu_pkg_repo_t *out, int max) {
    if (!g_pkgmgr.initialized || !out || max <= 0) return 0;
    int n = g_pkgmgr.n_repos < max ? g_pkgmgr.n_repos : max;
    for (int i = 0; i < n; i++) {
        out[i] = g_pkgmgr.repos[i];
    }
    return n;
}

bool wubu_pkgmgr_repo_update(const char *name) {
    if (!g_pkgmgr.initialized) return false;
    /* TODO: fetch index from remote */
    char sql[256];
    if (name) {
        snprintf(sql, sizeof(sql), "UPDATE repos SET last_update=strftime('%%s','now') WHERE name='%s'", name);
    } else {
        snprintf(sql, sizeof(sql), "UPDATE repos SET last_update=strftime('%%s','now') WHERE enabled=1");
    }
    db_exec(sql);
    return true;
}

/* -- Search -------------------------------------------------------- */

int wubu_pkgmgr_search(const char *query, wubu_pkg_repo_entry_t *out, int max) {
    if (!g_pkgmgr.initialized || !query || !out || max <= 0) return 0;
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM repo_packages WHERE name LIKE '%%%s%%' OR description LIKE '%%%s%%' LIMIT %d",
        query, query, max);
    return db_query(sql, NULL, out) == 0 ? max : 0;
}

bool wubu_pkgmgr_repo_get_info(const char *pkg_id, wubu_pkg_repo_entry_t *out) {
    if (!g_pkgmgr.initialized || !pkg_id || !out) return false;
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT * FROM repo_packages WHERE id='%s' LIMIT 1", pkg_id);
    return db_query(sql, NULL, out) == 0;
}

/* -- Manifest ------------------------------------------------------ */

char *manifest_to_json(const wubu_pkg_manifest_t *m) {
    if (!m) return NULL;
    /* Estimate size */
    size_t cap = 4096;
    char *json = malloc(cap);
    if (!json) return NULL;
    int pos = 0;
    pos += snprintf(json + pos, cap - pos, "{\"id\":\"%s\",", m->id);
    pos += snprintf(json + pos, cap - pos, "\"name\":\"%s\",", m->name);
    pos += snprintf(json + pos, cap - pos, "\"version\":\"%s\",", m->version);
    pos += snprintf(json + pos, cap - pos, "\"arch\":\"%s\",", m->arch);
    pos += snprintf(json + pos, cap - pos, "\"description\":\"%s\",", m->description);
    pos += snprintf(json + pos, cap - pos, "\"deps\":[");
    for (int i = 0; i < m->n_depends; i++) {
        pos += snprintf(json + pos, cap - pos, "%s\"%s\"", i > 0 ? "," : "", m->depends[i]);
    }
    pos += snprintf(json + pos, cap - pos, "],");
    pos += snprintf(json + pos, cap - pos, "\"entries\":[");
    for (int i = 0; i < m->n_entrypoints; i++) {
        pos += snprintf(json + pos, cap - pos, "%s{\"name\":\"%s\",\"exec\":\"%s\",\"icon\":\"%s\",\"categories\":\"%s\"}",
            i > 0 ? "," : "", m->entrypoints[i].name, m->entrypoints[i].exec,
            m->entrypoints[i].icon, m->entrypoints[i].categories);
    }
    pos += snprintf(json + pos, cap - pos, "]}");
    return json;
}

/* -- Dep Resolution ------------------------------------------------ */

int wubu_pkgmgr_resolve_deps(const char **pkg_ids, int n_pkgs,
                              char ***out_ordered, int *out_n) {
    (void)pkg_ids; (void)n_pkgs; (void)out_ordered; (void)out_n;
    /* TODO: topological sort implementation */
    return 0;
}

bool wubu_pkgmgr_check_conflicts(const char *pkg_id, const wubu_pkg_installed_t *installed, int n_installed) {
    (void)pkg_id; (void)installed; (void)n_installed;
    /* TODO: conflict detection */
    return false;
}

/* -- Cleanup ------------------------------------------------------- */

bool wubu_pkgmgr_clean_cache(int max_age_days) {
    if (!g_pkgmgr.initialized) return false;
    /* TODO: iterate cache dir, remove old files */
    (void)max_age_days;
    return true;
}

int wubu_pkgmgr_autoremove(bool dry_run) {
    if (!g_pkgmgr.initialized) return 0;
    (void)dry_run;
    /* TODO: find orphaned deps */
    return 0;
}

int wubu_pkgmgr_verify_installed(char ***out_broken, int *out_n_broken) {
    (void)out_broken; (void)out_n_broken;
    /* TODO: verify file checksums */
    return 0;
}

bool wubu_pkgmgr_get_stats(wubu_pkgmgr_stats_t *out) {
    if (!g_pkgmgr.initialized || !out) return false;
    memset(out, 0, sizeof(*out));
    char sql[64];
    int count = 0;
    db_query("SELECT COUNT(*) FROM packages", NULL, &count);
    out->installed_count = count;
    out->total_repos = 0;
    return true;
}

void wubu_pkgmgr_set_progress_callback(wubu_pkgmgr_progress_cb cb, void *userdata) {
    g_pkgmgr.progress_cb = cb;
    g_pkgmgr.progress_userdata = userdata;
}