/*
 * wubu_pkgmgr_internal.h  --  Internal header for wubu_pkgmgr submodules
 */

#ifndef WUBU_PKGMGR_INTERNAL_H
#define WUBU_PKGMGR_INTERNAL_H

#include "wubu_pkgmgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <zstd.h>
#include <sqlite3.h>
/* Use WuBuOS's self-contained SHA256 (no external crypto dep). Any future
 * TLS download path should gate libcurl/openssl behind WUBU_HAVE_CURL. */
#include "../runtime/wubu_crypto.h"

/* -- Global state (extern) --------------------------------------- */
typedef struct {
    wubu_pkgmgr_config_t config;
    bool initialized;
    sqlite3 *db;
    wubu_pkgmgr_progress_cb progress_cb;
    void *progress_userdata;
    wubu_pkg_repo_t repos[32];
    int n_repos;
} wubu_pkgmgr_state_t;

extern wubu_pkgmgr_state_t g_pkgmgr;

/* -- .wubu container format header (private, shared by pkg module) -- */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;              /* "WUBU" */
    uint16_t version;            /* 1 */
    uint16_t flags;             /* Reserved */
    uint8_t arch;               /* wubu_pkg_arch_t */
    uint8_t payload_type;       /* wubu_pkg_payload_t */
    uint32_t manifest_size;     /* Compressed manifest size */
    uint64_t payload_size;      /* Compressed payload size */
    uint64_t uncompressed_size; /* Total uncompressed */
    uint8_t signature[WUBU_PKG_SIG_SIZE]; /* Ed25519 */
    char build_date[32];        /* ISO 8601 */
    char builder_version[32];
    uint32_t crc32;             /* CRC32 of header */
} wubu_pkg_header_t;
#pragma pack(pop)

/* -- Utility helpers (static inline, no symbol export) ----------- */

static inline bool ensure_dir(const char *path) {
    if (!path || !*path) return false;
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    /* Create parent directories recursively, then the leaf. */
    char tmp[512];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static inline bool write_file(const char *path, const char *content) {
    if (!path || !content) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return written == len;
}

static inline bool read_file(const char *path, char **out_content, size_t *out_size) {
    if (!path || !out_content || !out_size) return false;
    FILE *f = fopen(path, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz < 0) { fclose(f); return false; }
    if (sz == 0) { fclose(f); *out_content = NULL; *out_size = 0; return true; }
    *out_size = (size_t)sz;
    *out_content = malloc(*out_size + 1);
    if (!*out_content) { fclose(f); return false; }
    size_t rd = fread(*out_content, 1, *out_size, f);
    fclose(f);
    if (rd != *out_size) { free(*out_content); *out_content = NULL; return false; }
    (*out_content)[*out_size] = '\0';
    return true;
}

static inline bool copy_file(const char *src, const char *dst) {
    if (!src || !dst) return false;
    char *data = NULL;
    size_t size = 0;
    if (!read_file(src, &data, &size)) return false;
    bool ok = write_file(dst, data);
    free(data);
    return ok;
}

static inline char *sha256_hex(const char *data, size_t len) {
    if (!data) return NULL;
    char *hex = malloc(65);
    if (!hex) return NULL;
    wubu_sha256_digest(data, len, hex, 65);
    return hex;
}

/* -- DB helpers (defined in facade, non-static) ------------------ */

int64_t db_exec(const char *sql);
int db_query(const char *sql, int (*callback)(void*, int, char**, char**), void *data);

/* -- Progress callback (defined in facade, non-static) ------------ */

void pkgmgr_progress(const char *stage, const char *pkg_id, float progress, const char *msg);

/* -- Manifest helpers (defined in facade) ------------------------ */

char *manifest_to_json(const wubu_pkg_manifest_t *m);

/* -- Package I/O helpers (defined in pkg module) ----------------- */

bool write_pkg(const char *output_path, const wubu_pkg_manifest_t *manifest,
               const char *payload_dir, const char *sign_key_hex);
bool read_pkg_header(const char *pkg_path, wubu_pkg_header_t *header);
bool extract_pkg_manifest(const char *pkg_path, wubu_pkg_manifest_t *out);

/* -- Install helpers (defined in install module) ----------------- */

bool install_package_files(const wubu_pkg_manifest_t *manifest, const char *install_root);
void generate_desktop_entry(const wubu_pkg_manifest_t *manifest, int entry_idx, const char *install_path);

/* -- Package I/O public API (wubu_pkgmgr_pkg.c) ------------------ */

bool wubu_pkgmgr_create_package(const char *src_dir, const char *output_path,
                                 const wubu_pkg_manifest_t *manifest,
                                 const char *privkey_path);
bool wubu_pkgmgr_verify_package(const char *pkg_path, const char *pubkey_hex);
bool wubu_pkgmgr_extract_package(const char *pkg_path, const char *dest_dir);
bool wubu_pkgmgr_read_manifest(const char *pkg_path, wubu_pkg_manifest_t *out);

/* -- Install/remove/upgrade (wubu_pkgmgr_install.c) ------------- */

bool wubu_pkgmgr_install(const char *pkg_spec, bool dry_run);
bool wubu_pkgmgr_remove(const char *pkg_id, bool auto_remove_deps);
bool wubu_pkgmgr_upgrade(const char *pkg_spec, bool dry_run);
bool wubu_pkgmgr_upgrade_all(bool dry_run);
int  wubu_pkgmgr_list_installed(wubu_pkg_installed_t *out, int max);
bool wubu_pkgmgr_get_installed(const char *pkg_id, wubu_pkg_installed_t *out);
bool wubu_pkgmgr_is_installed(const char *pkg_id);

/* -- Transaction (wubu_pkgmgr_txn.c) ---------------------------- */

bool wubu_pkgmgr_txn_begin(wubu_pkg_transaction_t *txn, bool dry_run);
bool wubu_pkgmgr_txn_add(wubu_pkg_transaction_t *txn, wubu_pkg_txn_type_t type,
                          const char *pkg_id, const char *old_ver, const char *new_ver,
                          bool is_dep);
bool wubu_pkgmgr_txn_commit(wubu_pkg_transaction_t *txn);
bool wubu_pkgmgr_txn_rollback(wubu_pkg_transaction_t *txn);
bool wubu_pkgmgr_register_desktop(const wubu_pkg_installed_t *pkg);
bool wubu_pkgmgr_unregister_desktop(const char *pkg_id);
bool wubu_pkgmgr_generate_desktop_files(const wubu_pkg_installed_t *pkg);

#endif /* WUBU_PKGMGR_INTERNAL_H */