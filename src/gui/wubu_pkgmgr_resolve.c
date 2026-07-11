/* wubu_pkgmgr_resolve.c -- WuBuOS pkgmgr: dependency resolution + conflict check.
 * Extracted from wubu_pkgmgr.c (separable leaf). Self-contained: builds a sorted
 * package-id set, resolves transitive deps, checks install conflicts. Uses
 * g_pkgmgr (extern in wubu_pkgmgr_internal.h) + db_exec/db_query (declared there).
 * C11, minimal includes.
 */
#include "wubu_pkgmgr.h"
#include "wubu_pkgmgr_internal.h"

#include <stdlib.h>
#include <string.h>

static int pkgmgr_index_id(const char *id, char (*pkg_set)[128], int *n_total, int max) {
    for (int i = 0; i < *n_total; i++)
        if (strcmp(pkg_set[i], id) == 0) return i;
    if (*n_total >= max) return -1;
    strncpy(pkg_set[*n_total], id, 127);
    pkg_set[*n_total][127] = '\0';
    return (*n_total)++;
}

/* Kahn's algorithm for topological sort of package dependencies.
 * Accepts a list of package IDs and returns them ordered so that every
 * package appears after its dependencies (a valid installation order).
 * Uses SQLite's deps table to resolve the full transitive closure. */
int wubu_pkgmgr_resolve_deps(const char **pkg_ids, int n_pkgs,
                              char ***out_ordered, int *out_n) {
    if (!g_pkgmgr.initialized || !pkg_ids || n_pkgs <= 0 || !out_ordered || !out_n) {
        if (out_n) *out_n = 0;
        return -1;
    }

    enum { MAX_PKGS = 256 };
    char pkg_set[MAX_PKGS][128];
    int  in_degree[MAX_PKGS];
    int  adj[MAX_PKGS][MAX_PKGS];
    int  adj_counts[MAX_PKGS];
    int  n_total = 0;

    memset(in_degree, 0, sizeof(in_degree));
    memset(adj_counts, 0, sizeof(adj_counts));

    /* Seed the set with input packages. */
    for (int i = 0; i < n_pkgs; i++) {
        if (!pkg_ids[i] || !pkg_ids[i][0]) continue;
        if (pkgmgr_index_id(pkg_ids[i], pkg_set, &n_total, MAX_PKGS) < 0)
            return -1;
    }

    /* Phase 2: Build transitive dep graph by querying deps table directly.
     * We iterate until no new packages are discovered (closure stabilised). */
    bool changed;
    do {
        changed = false;
        for (int i = 0; i < n_total; i++) {
            sqlite3_stmt *stmt = NULL;
            char sql[512];
            snprintf(sql, sizeof(sql),
                "SELECT dep_id FROM deps WHERE pkg_id='%s'", pkg_set[i]);

            if (sqlite3_prepare_v2(g_pkgmgr.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
                if (stmt) sqlite3_finalize(stmt);
                continue;
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *dep_id = (const char *)sqlite3_column_text(stmt, 0);
                if (!dep_id) continue;

                int kidx = pkgmgr_index_id(dep_id, pkg_set, &n_total, MAX_PKGS);
                if (kidx < 0) { sqlite3_finalize(stmt); return -1; }

                /* Add edge: dep -> pkg (dep must be installed first) */
                bool found = false;
                for (int k = 0; k < adj_counts[kidx]; k++) {
                    if (adj[kidx][k] == i) { found = true; break; }
                }
                if (!found) {
                    adj[kidx][adj_counts[kidx]++] = i;
                    in_degree[i]++;
                    changed = true;
                }
            }
            sqlite3_finalize(stmt);
        }
    } while (changed);

    /* Phase 3: Kahn's algorithm — emit nodes with in-degree 0. */
    char *ordered[MAX_PKGS];
    int n_ordered = 0;
    int queue[MAX_PKGS];
    int q_head = 0, q_tail = 0;

    for (int i = 0; i < n_total; i++) {
        if (in_degree[i] == 0) queue[q_tail++] = i;
    }

    while (q_head < q_tail) {
        int v = queue[q_head++];
        ordered[n_ordered] = strdup(pkg_set[v]);
        if (!ordered[n_ordered]) {
            for (int j = 0; j < n_ordered; j++) free(ordered[j]);
            *out_n = 0;
            return -1;
        }
        n_ordered++;

        for (int k = 0; k < adj_counts[v]; k++) {
            int w = adj[v][k];
            if (--in_degree[w] == 0) queue[q_tail++] = w;
        }
    }

    if (n_ordered < n_total) {
        for (int i = 0; i < n_ordered; i++) free(ordered[i]);
        *out_n = 0;
        return -1;  /* cycle detected */
    }

    *out_ordered = (char **)malloc(sizeof(char *) * (size_t)n_ordered);
    if (!*out_ordered) {
        for (int i = 0; i < n_ordered; i++) free(ordered[i]);
        *out_n = 0;
        return -1;
    }
    memcpy(*out_ordered, ordered, sizeof(char *) * (size_t)n_ordered);
    *out_n = n_ordered;
    return 0;
}

bool wubu_pkgmgr_check_conflicts(const char *pkg_id, const wubu_pkg_installed_t *installed, int n_installed) {
    if (!pkg_id || !installed || n_installed < 0) return false;
    /* An already-installed package with the same id is a conflict — refuse to
     * clobber it (the installer should upgrade instead). */
    for (int i = 0; i < n_installed; i++) {
        if (strcmp(installed[i].manifest.id, pkg_id) == 0) {
            fprintf(stderr, "[pkgmgr] Package '%s' is already installed\n", pkg_id);
            return false;
        }
    }
    return true;
}

/* -- Cleanup ------------------------------------------------------- */

/* Remove cache files older than max_age_days. Scans g_pkgmgr.config.cache_dir
 * for cached .wubu packages and deletes any whose mtime exceeds the threshold.
 * Returns true if at least one file was cleaned. */
