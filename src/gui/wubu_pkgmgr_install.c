/*
 * wubu_pkgmgr_install.c  --  WuBuOS Package Manager: Install / Remove / Upgrade
 *
 * Depends on the facade (wubu_pkgmgr.c) for all DB and repo/search state.
 * Repo management, search and index queries live in the facade; this module
 * handles the install lifecycle and the installed-package queries it needs,
 * using its own private SQLite callbacks.
 */

#include "../egojar.h"
#include "wubu_pkgmgr_internal.h"
#include "../runtime/wubu_pkg.h"   /* pkg_version_cmp: the ONE version comparator */

#include <glob.h>

/* -- Private SQLite callbacks (file-local, no symbol clash) ------- */



static int cb_pkg_installed(void *data, int argc, char **argv, char **col) {
    (void)col;
    if (argc < 7) return 0;
    char *buffer = data;
    wubu_pkg_installed_t *pkg = (wubu_pkg_installed_t *)buffer;
    int *found = (int *)(buffer + sizeof(wubu_pkg_installed_t));
    *found = 1;
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

/* -- File Installation ------------------------------------------- */

/* A manifest path is untrusted package content.  Only relative paths with
 * no "." / ".." components may be copied, so a payload can never escape
 * its install root (or read outside the extracted payload). */
static bool safe_rel_path(const char *s) {
    if (!s || !s[0] || s[0] == '/') return false;
    const char *p = s;
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        if (len == 0) return false;                    /* empty component */
        if (len == 1 && p[0] == '.') return false;
        if (len == 2 && p[0] == '.' && p[1] == '.') return false;
        if (!slash) break;
        p = slash + 1;
    }
    return true;
}

/* Package ids become directory names (temp dir + install root).  Restrict
 * them to a conservative charset so manifest content cannot steer paths. */
bool pkgmgr_valid_id(const char *id) {
    if (!id || !id[0]) return false;
    if (!((id[0] >= 'a' && id[0] <= 'z') || (id[0] >= 'A' && id[0] <= 'Z') ||
          (id[0] >= '0' && id[0] <= '9'))) return false;
    for (const char *p = id; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
            return false;
    }
    if (strstr(id, "..")) return false;
    return true;
}

/* Record one installed file in the files table (bound parameters: paths
 * come from untrusted manifest content).  This is the bookkeeping that
 * makes wubu_pkgmgr_verify_installed() meaningful. */
static bool record_installed_file(const char *pkg_id, const char *rel_path,
                                  uint64_t size, const char *sha_hex) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "INSERT OR REPLACE INTO files (pkg_id, path, size, sha256) "
            "VALUES (?1, ?2, ?3, ?4)", -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_text(stmt, 1, pkg_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rel_path, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)size);
    sqlite3_bind_text(stmt, 4, sha_hex, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

/* Copy every manifest file from the extracted payload (src_root) into the
 * package's TRUE install root (install_root = <prefix>/<id>), recording
 * each copied file's path/size/sha256 in the files table.  dst is relative
 * to install_root, so the DB install_path, the on-disk layout and
 * verify_installed()'s install_path+rel_path join all agree. */
bool install_package_files(const wubu_pkg_manifest_t *manifest, const char *src_root,
                           const char *install_root, uint64_t *out_total_size) {
    if (out_total_size) *out_total_size = 0;
    char src[1024], dst[1024];
    for (int i = 0; i < manifest->n_files; i++) {
        if (!safe_rel_path(manifest->files[i].src) ||
            !safe_rel_path(manifest->files[i].dst)) {
            fprintf(stderr, "[pkgmgr] refusing unsafe payload path '%s' -> '%s'\n",
                    manifest->files[i].src, manifest->files[i].dst);
            return false;
        }
        snprintf(src, sizeof(src), "%s/%s", src_root, manifest->files[i].src);
        snprintf(dst, sizeof(dst), "%s/%s", install_root, manifest->files[i].dst);

        char dir[1024];
        strncpy(dir, dst, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        char *last_slash = strrchr(dir, '/');
        if (last_slash) *last_slash = '\0';
        ensure_dir(dir);

        if (!copy_file(src, dst)) return false;
        chmod(dst, manifest->files[i].mode);

        /* Bookkeeping: hash the file AS INSTALLED and record it. */
        struct stat st;
        if (stat(dst, &st) != 0) return false;
        char hex[65];
        if (!wubu_pkgmgr_hash_file(dst, hex, sizeof(hex))) return false;
        if (!record_installed_file(manifest->id, manifest->files[i].dst,
                                   (uint64_t)st.st_size, hex))
            return false;
        if (out_total_size) *out_total_size += (uint64_t)st.st_size;
    }
    return true;
}

void generate_desktop_entry(const wubu_pkg_manifest_t *manifest, int entry_idx, const char *install_path) {
    const struct {
        char id[WUBU_PKG_MAX_NAME];
        char name[WUBU_PKG_MAX_NAME];
        char exec[256];
        char icon[256];
        char categories[256];
        char mime_types[256];
        bool terminal;
        bool startup_notify;
    } *e = &manifest->entrypoints[entry_idx];
    char path[512];
    char dir[512];
    snprintf(path, sizeof(path), "%s/share/applications/%s-%s.desktop",
             g_pkgmgr.config.install_prefix, manifest->id, e->id);

    strncpy(dir, path, sizeof(dir) - 1);
    char *last_slash = strrchr(dir, '/');
    if (last_slash) *last_slash = '\0';
    ensure_dir(dir);

    char content[2048];
    snprintf(content, sizeof(content),
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=%s\n"
        "Comment=%s\n"
        "Exec=%s\n"
        "Icon=%s\n"
        "Categories=%s\n"
        "MimeType=%s\n"
        "Terminal=%s\n"
        "StartupNotify=%s\n"
        "X-Wubu-Package=%s\n"
        "X-Wubu-Entrypoint=%s\n",
        e->name, manifest->description, e->exec, e->icon,
        e->categories, e->mime_types,
        e->terminal ? "true" : "false",
        e->startup_notify ? "true" : "false",
        manifest->id, e->id);

    write_file(path, content);
    (void)install_path;
}

/* -- Install lifecycle ------------------------------------------- */

/* rm -rf a directory tree via fork+exec (no system()). */
static void remove_tree(const char *path) {
    if (!path || !path[0]) return;
    pid_t rm_pid = fork();
    if (rm_pid == 0) {
        execlp("rm", "rm", "-rf", path, (char *)NULL);
        _exit(1);
    }
    if (rm_pid > 0) waitpid(rm_pid, NULL, 0);
}

#ifdef WUBU_HAVE_MBEDTLS
bool wubu_pkgmgr_verify_against_repos(const char *pkg_path) {
    /* A package carries no repo-origin field, so no single key can be
     * assumed "the" owner. Every ENABLED, keyed, NON-REVOKED repository is
     * a configured trust anchor; accept if any of them verifies, reject if
     * none does.  Revocations are the authoritative, DB-backed policy: a
     * revoked key is skipped even when it is still configured as a repo
     * anchor, and zero usable anchors fail closed. */
    int usable_anchors = 0;
    for (int i = 0; i < g_pkgmgr.n_repos; i++) {
        const char *key = g_pkgmgr.repos[i].pubkey;
        if (!g_pkgmgr.repos[i].enabled || !key[0]) continue;
        if (wubu_pkgmgr_key_is_revoked(key)) {
            fprintf(stderr, "[pkgmgr] skipping REVOKED trust anchor '%s' for '%s'\n",
                    key, pkg_path ? pkg_path : "(null)");
            continue;
        }
        /* Expiry is fail-closed anchor metadata: an EXPIRED anchor is
         * skipped just like a revoked one, so a package signed only by an
         * expired key can no longer be admitted. */
        if (wubu_pkgmgr_anchor_is_expired(key)) {
            fprintf(stderr, "[pkgmgr] skipping EXPIRED trust anchor '%s' for '%s'\n",
                    key, pkg_path ? pkg_path : "(null)");
            continue;
        }
        usable_anchors++;
        if (wubu_pkgmgr_verify_package(pkg_path, key)) return true;
    }
    if (usable_anchors == 0)
        fprintf(stderr, "[pkgmgr] no usable (non-revoked) Ed25519 trust anchor "
                        "configured: FAIL CLOSED\n");
    return false;
}
#endif

bool wubu_pkgmgr_install(const char *pkg_spec, bool dry_run) {
    pkgmgr_progress("install", pkg_spec, 0.1, "Resolving package");

    wubu_pkg_manifest_t *manifest = calloc(1, sizeof(wubu_pkg_manifest_t));
    if (!manifest) return false;
    bool is_local = (strstr(pkg_spec, ".wubu") != NULL);
    /* The actual .wubu file to extract: the local path, or the verified
     * cache artifact for a remote install.  Never the package id. */
    char pkg_path[512];
    pkg_path[0] = '\0';

    if (is_local) {
        if (!extract_pkg_manifest(pkg_spec, manifest)) {
            free(manifest);
            return false;
        }
        snprintf(pkg_path, sizeof(pkg_path), "%s", pkg_spec);
    } else {
        /* Search repos via facade */
        wubu_pkg_repo_entry_t repo_pkg;
        if (!wubu_pkgmgr_repo_get_info(pkg_spec, &repo_pkg)) {
            free(manifest);
            return false;
        }

        pkgmgr_progress("install", pkg_spec, 0.3, "Downloading package");
        char cache_path[512];
        snprintf(cache_path, sizeof(cache_path), "%s/%s-%s.wubu",
                 g_pkgmgr.config.cache_dir, repo_pkg.id, repo_pkg.version);

        /* Local-cache fast path: reuse the cached .wubu ONLY when it still
         * matches the index's sha256.  A stale or tampered cache entry is
         * discarded and re-fetched; a missing index hash fails closed in
         * the fetch leaf. */
        bool cached_ok = false;
        if (access(cache_path, R_OK) == 0) {
            char hex[65], want[65];
            snprintf(want, sizeof(want), "%s", repo_pkg.sha256);
            for (char *q = want; *q; q++)
                if (*q >= 'A' && *q <= 'F') *q = (char)(*q - 'A' + 'a');
            if (want[0] &&
                wubu_pkgmgr_hash_file(cache_path, hex, sizeof(hex)) &&
                strcmp(hex, want) == 0) {
                cached_ok = true;
            } else {
                remove_tree(cache_path);
            }
        }
        if (!cached_ok) {
            /* REAL download over HTTP with a sha256 gate BEFORE extract.
             * Fails closed on missing hash, fetch error, or mismatch. */
            if (!wubu_pkgmgr_fetch_package(&repo_pkg, cache_path)) {
                fprintf(stderr, "[pkgmgr] install refused: payload for '%s' could not be "
                                "downloaded and verified\n", pkg_spec);
                free(manifest);
                return false;
            }
        }

        if (!extract_pkg_manifest(cache_path, manifest)) {
            free(manifest);
            return false;
        }
        snprintf(pkg_path, sizeof(pkg_path), "%s", cache_path);
        pkg_spec = manifest->id;
    }

    if (!pkgmgr_valid_id(manifest->id)) {
        fprintf(stderr, "[pkgmgr] install refused: unsafe package id '%s'\n", manifest->id);
        free(manifest);
        return false;
    }
    /* Dependency ids are read back into SQL and resolver queries later;
     * validate at ingest with the SAME owner, before any install side effects. */
    for (int i = 0; i < manifest->n_depends; i++) {
        if (!pkgmgr_valid_id(manifest->depends[i])) {
            fprintf(stderr, "[pkgmgr] install refused: unsafe dependency id '%s'\n",
                    manifest->depends[i]);
            free(manifest);
            return false;
        }
    }

#ifdef WUBU_HAVE_MBEDTLS
    /* Fail-closed Ed25519 gate BEFORE any extraction: with the crypto build
     * a package is never installed unless its signature verifies against a
     * configured repository trust anchor.  The legacy no-mbedTLS build does
     * not pretend to verify. */
    if (g_pkgmgr.config.verify_signatures) {
        if (g_pkgmgr.n_repos == 0 || !wubu_pkgmgr_verify_against_repos(pkg_path)) {
            fprintf(stderr,
                    "[pkgmgr] install refused: no configured Ed25519 trust anchor "
                    "verified this package\n");
            free(manifest);
            return false;
        }
    }
#endif

    pkgmgr_progress("install", pkg_spec, 0.5, "Checking dependencies");

    wubu_pkg_installed_t *installed = calloc(256, sizeof(wubu_pkg_installed_t));
    if (!installed) {
        free(manifest);
        return false;
    }
    int n_installed = wubu_pkgmgr_list_installed(installed, 256);
    if (!wubu_pkgmgr_check_conflicts(pkg_spec, installed, n_installed)) {
        fprintf(stderr, "[pkgmgr] Package conflicts with installed packages\n");
        free(installed);
        free(manifest);
        return false;
    }

    if (dry_run) {
        pkgmgr_progress("install", pkg_spec, 1.0, "Dry run complete");
        free(installed);
        free(manifest);
        return true;
    }

    pkgmgr_progress("install", pkg_spec, 0.7, "Installing files");

    char temp_dir[512];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/wubu_install_%s", manifest->id);
    remove_tree(temp_dir);
    ensure_dir(temp_dir);

    /* install_root is the TRUE root of the copied layout: every manifest
     * dst lands under it, and the DB install_path records exactly it, so
     * remove and verify_installed operate on the real payload. */
    char install_root[512];
    snprintf(install_root, sizeof(install_root), "%s/%s", g_pkgmgr.config.install_prefix, manifest->id);

    /* Transaction backup: snapshot the PRE-install state before any payload
     * file is copied in -- and before ensure_dir creates a fresh root, so a
     * first-time install rolls back to NO root at all.  No-op outside commit. */
    pkgmgr_backup_capture(manifest->id, install_root);
    ensure_dir(install_root);

    /* Unpack the .wubu payload into temp_dir so install_package_files() has
     * real files to copy into install_root. We extract relative to temp_dir,
     * which mirrors the package's payload layout (tar archived ".") */
    if (!wubu_pkgmgr_extract_package(pkg_path, temp_dir)) {
        fprintf(stderr, "[pkgmgr] Failed to extract payload\n");
        remove_tree(temp_dir);
        remove_tree(install_root);
        free(installed);
        free(manifest);
        return false;
    }

    uint64_t total_size = 0;
    if (!install_package_files(manifest, temp_dir, install_root, &total_size)) {
        fprintf(stderr, "[pkgmgr] Failed to install payload files\n");
        remove_tree(temp_dir);
        remove_tree(install_root);
        free(installed);
        free(manifest);
        return false;
    }
    remove_tree(temp_dir);

    pkgmgr_progress("install", pkg_spec, 0.85, "Registering desktop entries");
    for (int i = 0; i < manifest->n_entrypoints; i++) {
        generate_desktop_entry(manifest, i, install_root);
    }

    pkgmgr_progress("install", pkg_spec, 0.9, "Registering in database");

    char *manifest_json = manifest_to_json(manifest);
    time_t now = time(NULL);
    char date[32];
    strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    /* Bound parameters: every value here originates in untrusted package
     * content.  Check the result -- a failed registration (e.g. the id is
     * already installed) must fail the install, not report success. */
    bool db_ok = false;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "INSERT INTO packages (id, name, version, description, maintainer, homepage, "
            "license, manifest_json, install_path, install_date, auto_installed, size_bytes, "
            "payload_type, arch, sandbox_profile) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 0, ?11, ?12, ?13, ?14)",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, manifest->id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, manifest->name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, manifest->version, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, manifest->description, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, manifest->maintainer, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, manifest->homepage, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, manifest->license, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, manifest_json ? manifest_json : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, install_root, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 10, date, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 11, (sqlite3_int64)total_size);
        sqlite3_bind_int(stmt, 12, (int)manifest->payload_type);
        sqlite3_bind_int(stmt, 13, (int)manifest->arch);
        sqlite3_bind_text(stmt, 14, manifest->sandbox_profile, -1, SQLITE_TRANSIENT);
        db_ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
    }
    free(manifest_json);

    if (!db_ok) {
        fprintf(stderr, "[pkgmgr] Failed to register package '%s' in database\n", manifest->id);
        /* Fail closed: undo the copied payload and its file records. */
        remove_tree(install_root);
        sqlite3_stmt *del = NULL;
        if (sqlite3_prepare_v2(g_pkgmgr.db, "DELETE FROM files WHERE pkg_id=?1",
                               -1, &del, NULL) == SQLITE_OK) {
            sqlite3_bind_text(del, 1, manifest->id, -1, SQLITE_TRANSIENT);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
        free(installed);
        free(manifest);
        return false;
    }

    for (int i = 0; i < manifest->n_depends; i++) {
        sqlite3_stmt *dstmt = NULL;
        if (sqlite3_prepare_v2(g_pkgmgr.db,
                "INSERT OR REPLACE INTO deps (pkg_id, dep_id, dep_version, optional) "
                "VALUES (?1, ?2, '', 0)", -1, &dstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(dstmt, 1, manifest->id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(dstmt, 2, manifest->depends[i], -1, SQLITE_TRANSIENT);
            sqlite3_step(dstmt);
            sqlite3_finalize(dstmt);
        }
    }

    pkgmgr_progress("install", pkg_spec, 1.0, "Install complete");
    free(installed);
    free(manifest);
    return true;
}

bool wubu_pkgmgr_remove(const char *pkg_id, bool auto_remove_deps) {
    pkgmgr_progress("remove", pkg_id, 0.2, "Removing package");

    wubu_pkg_installed_t pkg;
    if (!wubu_pkgmgr_get_installed(pkg_id, &pkg)) {
        return false;
    }

    if (!auto_remove_deps) {
        /* pkg_id may be caller/DB supplied; bind it and read the real count. */
        int count = 0;
        sqlite3_stmt *ref = NULL;
        if (sqlite3_prepare_v2(g_pkgmgr.db,
                "SELECT COUNT(*) FROM deps WHERE dep_id=?1", -1, &ref, NULL) == SQLITE_OK) {
            sqlite3_bind_text(ref, 1, pkg_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(ref) == SQLITE_ROW) count = sqlite3_column_int(ref, 0);
            sqlite3_finalize(ref);
        }
        if (count > 0) {
            fprintf(stderr, "[pkgmgr] Package is required by other packages\n");
            return false;
        }
    }

    pkgmgr_progress("remove", pkg_id, 0.5, "Removing files");

    /* install_path is the TRUE payload root (install copies under it), so
     * this removes the actual installed files, not an empty directory.
     * Snapshot it FIRST so a rollback can put it back. */
    pkgmgr_backup_capture(pkg_id, pkg.install_path);
    remove_tree(pkg.install_path);

    char desktop_pattern[512];
    snprintf(desktop_pattern, sizeof(desktop_pattern), "%s/share/applications/%s-*.desktop",
             g_pkgmgr.config.install_prefix, pkg_id);

    glob_t glob_result;
    if (glob(desktop_pattern, 0, NULL, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            unlink(glob_result.gl_pathv[i]);
        }
        globfree(&glob_result);
    }

    static const char *const deletes[] = {
        "DELETE FROM packages WHERE id=?1",
        "DELETE FROM deps WHERE pkg_id=?1"
    };
    for (int i = 0; i < 2; i++) {
        sqlite3_stmt *dstmt = NULL;
        if (sqlite3_prepare_v2(g_pkgmgr.db, deletes[i], -1, &dstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(dstmt, 1, pkg_id, -1, SQLITE_TRANSIENT);
            sqlite3_step(dstmt);
            sqlite3_finalize(dstmt);
        }
    }
    /* Drop the file bookkeeping too, or verify_installed would keep
     * reporting a removed package as broken forever. */
    {
        sqlite3_stmt *del = NULL;
        if (sqlite3_prepare_v2(g_pkgmgr.db, "DELETE FROM files WHERE pkg_id=?1",
                               -1, &del, NULL) == SQLITE_OK) {
            sqlite3_bind_text(del, 1, pkg_id, -1, SQLITE_TRANSIENT);
            sqlite3_step(del);
            sqlite3_finalize(del);
        }
    }

    pkgmgr_progress("remove", pkg_id, 1.0, "Remove complete");
    return true;
}

bool wubu_pkgmgr_upgrade(const char *pkg_spec, bool dry_run) {
    wubu_pkg_installed_t installed;
    if (!wubu_pkgmgr_get_installed(pkg_spec, &installed)) {
        return wubu_pkgmgr_install(pkg_spec, dry_run); /* Not installed, install it */
    }

    wubu_pkg_repo_entry_t repo;
    if (!wubu_pkgmgr_repo_get_info(pkg_spec, &repo)) {
        return false; /* Not in repo */
    }

    /* Numeric version comparison (1.10 > 1.9).  strcmp got this wrong;
     * pkg_version_cmp in src/runtime/wubu_pkg.c is the single owner. */
    if (pkg_version_cmp(installed.manifest.version, repo.version) >= 0) {
        return true; /* Already up to date */
    }

    /* One txn item covers the whole upgrade.  Snapshot the OLD install root
     * before remove+install mutate it; the inner calls' capture hook is a
     * no-op for this item (first snapshot wins). */
    pkgmgr_backup_capture(pkg_spec, installed.install_path);
    wubu_pkgmgr_remove(pkg_spec, false);
    return wubu_pkgmgr_install(pkg_spec, dry_run);
}

bool wubu_pkgmgr_upgrade_all(bool dry_run) {
    wubu_pkg_installed_t pkgs[256];
    int n = wubu_pkgmgr_list_installed(pkgs, 256);
    for (int i = 0; i < n; i++) {
        wubu_pkgmgr_upgrade(pkgs[i].manifest.id, dry_run);
    }
    return true;
}

/* -- Installed query (private callback) -------------------------- */

int wubu_pkgmgr_list_installed(wubu_pkg_installed_t *out, int max) {
    if (!g_pkgmgr.initialized) return 0;

    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT id, name, version, arch, install_date, size_bytes, install_path "
        "FROM packages ORDER BY name");

    size_t buffer_size = sizeof(int) + 256 * sizeof(wubu_pkg_installed_t);
    char *buffer = calloc(1, buffer_size);
    if (!buffer) return 0;

    int *count = (int *)buffer;
    wubu_pkg_installed_t *pkgs = (wubu_pkg_installed_t *)(buffer + sizeof(int));

    cb_list_installed_reset();
    db_query(sql, cb_list_installed, pkgs);
    *count = cb_list_installed_count();

    int n = *count < max ? *count : max;
    if (out && n > 0) {
        memcpy(out, pkgs, n * sizeof(wubu_pkg_installed_t));
    }
    int result = *count;
    free(buffer);
    return result;
}

bool wubu_pkgmgr_get_installed(const char *pkg_id, wubu_pkg_installed_t *out) {
    if (!g_pkgmgr.initialized) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "SELECT id, name, version, arch, install_date, size_bytes, install_path "
            "FROM packages WHERE id=?1", -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_text(stmt, 1, pkg_id, -1, SQLITE_TRANSIENT);

    char buffer[sizeof(wubu_pkg_installed_t) + sizeof(int)];
    memset(buffer, 0, sizeof(buffer));
    wubu_pkg_installed_t *pkg = (wubu_pkg_installed_t *)buffer;
    int *found = (int *)(buffer + sizeof(wubu_pkg_installed_t));
    *found = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *vals[7];
        for (int i = 0; i < 7; i++) vals[i] = (const char *)sqlite3_column_text(stmt, i);
        cb_pkg_installed(buffer, 7, (char **)vals, NULL);
    }
    sqlite3_finalize(stmt);

    if (!*found) return false;

    *out = *pkg;
    return true;
}

bool wubu_pkgmgr_is_installed(const char *pkg_id) {
    wubu_pkg_installed_t pkg;
    return wubu_pkgmgr_get_installed(pkg_id, &pkg);
}

