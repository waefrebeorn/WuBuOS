/*
 * wubu_pkgmgr_install.c  --  WuBuOS Package Manager: Install / Remove / Upgrade
 *
 * Depends on the facade (wubu_pkgmgr.c) for all DB and repo/search state.
 * Repo management, search and index queries live in the facade; this module
 * handles the install lifecycle and the installed-package queries it needs,
 * using its own private SQLite callbacks.
 */

#include "wubu_pkgmgr_internal.h"

#include <glob.h>
#include <sys/wait.h>
#include <unistd.h>

/* -- Private SQLite callbacks (file-local, no symbol clash) ------- */

static int g_list_idx;

static int cb_list_installed(void *data, int argc, char **argv, char **col) {
    (void)col;
    wubu_pkg_installed_t *buffer = data;
    if (g_list_idx < 0) return 0;
    if (argc < 7) return 0;
    wubu_pkg_installed_t *pkg = &buffer[g_list_idx++];
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

bool install_package_files(const wubu_pkg_manifest_t *manifest, const char *install_root) {
    char src[1024], dst[1024];
    for (int i = 0; i < manifest->n_files; i++) {
        snprintf(src, sizeof(src), "%s/%s", install_root, manifest->files[i].src);
        snprintf(dst, sizeof(dst), "%s/%s", g_pkgmgr.config.install_prefix, manifest->files[i].dst);

        char dir[1024];
        strncpy(dir, dst, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        char *last_slash = strrchr(dir, '/');
        if (last_slash) *last_slash = '\0';
        ensure_dir(dir);

        if (copy_file(src, dst)) {
            chmod(dst, manifest->files[i].mode);
        } else {
            return false;
        }
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

bool wubu_pkgmgr_install(const char *pkg_spec, bool dry_run) {
    pkgmgr_progress("install", pkg_spec, 0.1, "Resolving package");

    wubu_pkg_manifest_t *manifest = calloc(1, sizeof(wubu_pkg_manifest_t));
    if (!manifest) return false;
    bool is_local = (strstr(pkg_spec, ".wubu") != NULL);
    bool ret = false;

    if (is_local) {
        if (!extract_pkg_manifest(pkg_spec, manifest)) {
            free(manifest);
            return false;
        }
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

        /* In production: TLS download via libcurl (gated behind WUBU_HAVE_CURL).
         * For now assume the .wubu is already cached, matching the test harness. */
        if (!extract_pkg_manifest(cache_path, manifest)) {
            /* Fall back to treating pkg_spec as a path */
            if (!extract_pkg_manifest(pkg_spec, manifest)) {
                free(manifest);
                return false;
            }
        }

        pkg_spec = manifest->id;
    }

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
    {
        pid_t rm_pid = fork();
        if (rm_pid == 0) {
            execlp("rm", "rm", "-rf", temp_dir, (char *)NULL);
            _exit(1);
        }
        if (rm_pid > 0) waitpid(rm_pid, NULL, 0);
    }
    ensure_dir(temp_dir);

    char install_root[512];
    snprintf(install_root, sizeof(install_root), "%s/%s", g_pkgmgr.config.install_prefix, manifest->id);
    ensure_dir(install_root);

    if (!install_package_files(manifest, temp_dir)) {
        free(installed);
        free(manifest);
        return false;
    }

    pkgmgr_progress("install", pkg_spec, 0.85, "Registering desktop entries");
    for (int i = 0; i < manifest->n_entrypoints; i++) {
        generate_desktop_entry(manifest, i, install_root);
    }

    pkgmgr_progress("install", pkg_spec, 0.9, "Registering in database");

    char *manifest_json = manifest_to_json(manifest);
    time_t now = time(NULL);
    char date[32];
    strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    char sql[2048];
    snprintf(sql, sizeof(sql),
        "INSERT INTO packages (id, name, version, description, maintainer, homepage, license, "
        "manifest_json, install_path, install_date, auto_installed, size_bytes, payload_type, arch, sandbox_profile) "
        "VALUES ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', 0, 0, %d, %d, '%s')",
        manifest->id, manifest->name, manifest->version, manifest->description,
        manifest->maintainer, manifest->homepage, manifest->license,
        manifest_json ? manifest_json : "", install_root, date,
        (int)manifest->payload_type, (int)manifest->arch, manifest->sandbox_profile);
    free(manifest_json);

    db_exec(sql);

    for (int i = 0; i < manifest->n_depends; i++) {
        snprintf(sql, sizeof(sql),
            "INSERT INTO deps (pkg_id, dep_id, dep_version, optional) VALUES ('%s', '%s', '', 0)",
            manifest->id, manifest->depends[i]);
        db_exec(sql);
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
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT COUNT(*) FROM deps WHERE dep_id='%s'", pkg_id);
        int count = 0;
        db_query(sql, NULL, &count);
        if (count > 0) {
            fprintf(stderr, "[pkgmgr] Package is required by other packages\n");
            return false;
        }
    }

    pkgmgr_progress("remove", pkg_id, 0.5, "Removing files");

    pid_t rm_pid = fork();
    if (rm_pid == 0) {
        execlp("rm", "rm", "-rf", pkg.install_path, (char *)NULL);
        _exit(1);
    }
    if (rm_pid > 0) waitpid(rm_pid, NULL, 0);

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

    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM packages WHERE id='%s'", pkg_id);
    db_exec(sql);
    snprintf(sql, sizeof(sql), "DELETE FROM deps WHERE pkg_id='%s'", pkg_id);
    db_exec(sql);

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

    if (strcmp(installed.manifest.version, repo.version) >= 0) {
        return true; /* Already up to date */
    }

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

    g_list_idx = 0;
    db_query(sql, cb_list_installed, pkgs);
    *count = g_list_idx;

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

    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT id, name, version, arch, install_date, size_bytes, install_path "
        "FROM packages WHERE id='%s'", pkg_id);

    char buffer[sizeof(wubu_pkg_installed_t) + sizeof(int)];
    memset(buffer, 0, sizeof(buffer));
    wubu_pkg_installed_t *pkg = (wubu_pkg_installed_t *)buffer;
    int *found = (int *)(buffer + sizeof(wubu_pkg_installed_t));
    *found = 0;

    if (db_query(sql, cb_pkg_installed, buffer) != 0) {
        return false;
    }

    if (!*found) return false;

    *out = *pkg;
    return true;
}

bool wubu_pkgmgr_is_installed(const char *pkg_id) {
    wubu_pkg_installed_t pkg;
    return wubu_pkgmgr_get_installed(pkg_id, &pkg);
}
