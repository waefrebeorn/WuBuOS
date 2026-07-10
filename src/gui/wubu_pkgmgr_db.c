/* wubu_pkgmgr_db.c -- Package manager database subsystem.
 *
 * Self-contained: progress reporting + SQLite exec/query wrappers + the
 * repo/installed list load callbacks. State via g_pkgmgr (extern in
 * wubu_pkgmgr_internal.h). Minimal includes.
 */

#include "wubu_pkgmgr_internal.h"

void pkgmgr_progress(const char *stage, const char *pkg_id, float progress, const char *msg) {
    if (g_pkgmgr.progress_cb)
        g_pkgmgr.progress_cb(g_pkgmgr.progress_userdata, stage, pkg_id, progress, msg);
}

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

int64_t db_exec(const char *sql) { return db_exec_raw(sql); }

int db_query(const char *sql, int (*callback)(void*, int, char**, char**), void *data) {
    return db_query_raw(sql, callback, data);
}

static int cb_list_installed_idx = 0;

void cb_list_installed_reset(void) { cb_list_installed_idx = 0; }
int cb_list_installed_count(void) { return cb_list_installed_idx; }

int cb_load_repos(void *data, int argc, char **argv, char **col) {
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

int cb_list_installed(void *data, int argc, char **argv, char **col) {
    (void)col;
    cb_list_installed_idx = 0;
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
