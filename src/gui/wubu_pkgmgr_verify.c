/* wubu_pkgmgr_verify.c -- WuBuOS pkgmgr: installed-package integrity verification.
 * Extracted from wubu_pkgmgr.c (separable leaf). Self-contained: queries the
 * installed packages + files tables, checks file existence / sha256. Uses
 * g_pkgmgr (extern in wubu_pkgmgr_internal.h) + wubu_sha256_digest (there).
 * C11, minimal includes.
 */
#include "wubu_pkgmgr.h"
#include "wubu_pkgmgr_internal.h"

#include <stdlib.h>
#include <string.h>

int wubu_pkgmgr_verify_installed(char ***out_broken, int *out_n_broken) {
    if (!g_pkgmgr.initialized) { if (out_n_broken) *out_n_broken = 0; return -1; }

    /* Query all installed packages. */
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "SELECT id, install_path FROM packages", -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        if (out_n_broken) *out_n_broken = 0;
        return -1;
    }

    enum { MAX_BROKEN = 256 };
    char broken[MAX_BROKEN][128];
    int n_broken = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *pkg_id = (const char *)sqlite3_column_text(stmt, 0);
        const char *install_path = (const char *)sqlite3_column_text(stmt, 1);
        if (!pkg_id || !install_path) continue;

        /* Query the files table for this package to get expected checksums. */
        char fsql[512];
        snprintf(fsql, sizeof(fsql),
            "SELECT path, sha256 FROM files WHERE pkg_id='%s'", pkg_id);

        sqlite3_stmt *fstmt = NULL;
        if (sqlite3_prepare_v2(g_pkgmgr.db, fsql, -1, &fstmt, NULL) != SQLITE_OK) {
            if (fstmt) sqlite3_finalize(fstmt);
            continue;
        }

        bool pkg_broken = false;
        while (sqlite3_step(fstmt) == SQLITE_ROW) {
            const char *rel_path = (const char *)sqlite3_column_text(fstmt, 0);
            const char *expected_sha = (const char *)sqlite3_column_text(fstmt, 1);
            if (!rel_path || !expected_sha) continue;

            char full_path[640];
            snprintf(full_path, sizeof(full_path), "%s/%s", install_path, rel_path);

            /* Check file existence first. */
            struct stat st;
            if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
                pkg_broken = true;
                continue;
            }

            /* Read file and verify checksum (if a sha256 is recorded). */
            if (expected_sha[0]) {
                FILE *f = fopen(full_path, "rb");
                if (!f) { pkg_broken = true; continue; }
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                rewind(f);
                if (fsize < 0) { fclose(f); pkg_broken = true; continue; }
                char *data = (char *)malloc((size_t)fsize + 1);
                if (!data) { fclose(f); pkg_broken = true; continue; }
                size_t rd = fread(data, 1, (size_t)fsize, f);
                fclose(f);
                if ((long)rd != fsize) { free(data); pkg_broken = true; continue; }
                data[fsize] = '\0';

                char actual_hex[65];
                wubu_sha256_digest(data, (size_t)fsize, actual_hex, sizeof(actual_hex));
                free(data);

                if (strcmp(actual_hex, expected_sha) != 0) {
                    pkg_broken = true;
                }
            }
        }
        sqlite3_finalize(fstmt);

        if (pkg_broken && n_broken < MAX_BROKEN) {
            strncpy(broken[n_broken], pkg_id, sizeof(broken[n_broken]) - 1);
            broken[n_broken][sizeof(broken[n_broken]) - 1] = '\0';
            n_broken++;
        }
    }
    sqlite3_finalize(stmt);

    if (out_n_broken) *out_n_broken = n_broken;
    if (n_broken == 0) { if (out_broken) *out_broken = NULL; return 0; }

    if (out_broken) {
        *out_broken = (char **)malloc(sizeof(char *) * (size_t)n_broken);
        if (!*out_broken) return -1;
        for (int i = 0; i < n_broken; i++) {
            (*out_broken)[i] = strdup(broken[i]);
            if (!(*out_broken)[i]) { free(*out_broken); *out_broken = NULL; return -1; }
        }
    }
    return -1;  /* broken packages found */
}

