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

/* -- DB Helpers ---------------------------------------------------- */

/* Non-static wrappers for submodule use */

/* -- SQLite Callbacks ---------------------------------------------- */

/* -- DB Schema ----------------------------------------------------- */

static const char SCHEMA_SQL[] =
    "CREATE TABLE IF NOT EXISTS packages ("
    "  id TEXT PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  version TEXT NOT NULL,"
    "  description TEXT DEFAULT '',"
    "  maintainer TEXT DEFAULT '',"
    "  homepage TEXT DEFAULT '',"
    "  license TEXT DEFAULT '',"
    "  manifest_json TEXT,"
    "  install_path TEXT,"
    "  install_date TEXT,"
    "  auto_installed INTEGER DEFAULT 0,"
    "  size_bytes INTEGER DEFAULT 0,"
    "  payload_type INTEGER DEFAULT 0,"
    "  arch TEXT DEFAULT 'x86_64',"
    "  sandbox_profile TEXT DEFAULT ''"
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

/* -- Manifest ------------------------------------------------------ */

/* -- Dep Resolution ------------------------------------------------ */

/* Helper: look up or insert a package id into the sorted set.
 * Returns index in [0, n_total), or -1 on overflow. */
bool wubu_pkgmgr_clean_cache(int max_age_days) {
    if (!g_pkgmgr.initialized) return false;
    if (max_age_days <= 0) max_age_days = 7;
    time_t cutoff = time(NULL) - (time_t)max_age_days * 86400;
    bool any = false;
    int rm_count = 0;

    DIR *d = opendir(g_pkgmgr.config.cache_dir);
    if (!d) return false;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char path[640];
        snprintf(path, sizeof(path), "%s/%s", g_pkgmgr.config.cache_dir, e->d_name);

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        if (st.st_mtime < cutoff) {
            if (unlink(path) == 0) {
                any = true;
                rm_count++;
            }
        }
    }
    closedir(d);

    if (any) {
        fprintf(stderr, "[pkgmgr] Cleaned %d cache files older than %d days\n",
                rm_count, max_age_days);
    }
    return any;
}

/* Find packages with auto_installed=1 that no other package depends on
 * (orphans). If dry_run is true, returns count without removing.
 * If dry_run is false, removes them and returns the count removed. */
int wubu_pkgmgr_autoremove(bool dry_run) {
    if (!g_pkgmgr.initialized) return 0;

    /* Query: packages where auto_installed=1 AND no other package depends on them.
     *   SELECT id FROM packages WHERE auto_installed=1
     *   AND id NOT IN (SELECT dep_id FROM deps) */
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT id FROM packages WHERE auto_installed=1 "
        "AND id NOT IN (SELECT DISTINCT dep_id FROM deps)");

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return 0;
    }

    /* Collect orphan package ids. */
    enum { MAX_ORPHANS = 256 };
    char orphans[MAX_ORPHANS][128];
    int n_orphans = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        if (!id || !id[0]) continue;
        if (n_orphans >= MAX_ORPHANS) break;
        strncpy(orphans[n_orphans], id, sizeof(orphans[n_orphans]) - 1);
        orphans[n_orphans][sizeof(orphans[n_orphans]) - 1] = '\0';
        n_orphans++;
    }
    sqlite3_finalize(stmt);

    if (dry_run) {
        if (n_orphans > 0) {
            fprintf(stderr, "[pkgmgr] Dry-run: would remove %d orphaned packages:\n", n_orphans);
            for (int i = 0; i < n_orphans && i < 10; i++)
                fprintf(stderr, "  - %s\n", orphans[i]);
            if (n_orphans > 10)
                fprintf(stderr, "  ... and %d more\n", n_orphans - 10);
        }
        return n_orphans;
    }

    /* Actually remove. */
    int removed = 0;
    for (int i = 0; i < n_orphans; i++) {
        char delsql[256];
        snprintf(delsql, sizeof(delsql), "DELETE FROM packages WHERE id='%s'", orphans[i]);
        if (db_exec(delsql) == 0) removed++;
    }

    if (removed > 0)
        fprintf(stderr, "[pkgmgr] Auto-removed %d orphaned packages\n", removed);
    return removed;
}

/* Verify installed packages by checking file checksums against the files table.
 * Returns 0 if all are intact, or -1 if any are broken. Populates out_broken
 * with the list of package ids whose files fail verification (caller must free). */
bool wubu_pkgmgr_get_stats(wubu_pkgmgr_stats_t *out) {
    if (!g_pkgmgr.initialized || !out) return false;
    memset(out, 0, sizeof(*out));
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

