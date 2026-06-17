/**
 * wubu_pkgmgr.h - WuBuOS Package Manager & .wubu App Store
 * 
 * .wubu container format:
 *   - Header: magic "WUBU", version, flags, arch, payload type
 *   - Manifest: JSON metadata (name, version, deps, description, entrypoints)
 *   - Payload: compressed archive (zstd) with app files
 *   - Signature: Ed25519 signature for verification
 * 
 * Features:
 *   - Local package database (SQLite)
 *   - Remote repository support (HTTPS + .wubu index)
 *   - Dependency resolution (topological sort)
 *   - Sandboxed installation (bubblewrap)
 *   - App manifests with desktop entries, MIME types, icons
 *   - Transactional install/remove/upgrade
 *   - Rollback support
 */

#ifndef WUBU_PKGMGR_H
#define WUBU_PKGMGR_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Constants
 * ============================================================ */
#define WUBU_PKG_MAGIC        0x55425557  /* "WUBU" */
#define WUBU_PKG_VERSION      1
#define WUBU_PKG_MAX_NAME     64
#define WUBU_PKG_MAX_VER      32
#define WUBU_PKG_MAX_DESC     512
#define WUBU_PKG_MAX_DEPS     32
#define WUBU_PKG_MAX_ENTRY    8
#define WUBU_PKG_MAX_FILES    1024
#define WUBU_PKG_SIG_SIZE     64          /* Ed25519 signature */

/* Payload types */
typedef enum {
    WUBU_PKG_PAYLOAD_NATIVE   = 0,  /* Native ELF executable */
    WUBU_PKG_PAYLOAD_LINUX    = 1,  /* Linux ELF (with deps) */
    WUBU_PKG_PAYLOAD_WIN32    = 2,  /* Windows PE (Proton) */
    WUBU_PKG_PAYLOAD_HOLYC    = 3,  /* HolyC source */
    WUBU_PKG_PAYLOAD_WASM     = 4,  /* WebAssembly module */
    WUBU_PKG_PAYLOAD_SCRIPT   = 5,  /* Shell/Python/JS script */
    WUBU_PKG_PAYLOAD_DATA     = 6,  /* Data-only (themes, fonts, etc) */
    WUBU_PKG_PAYLOAD_CONTAINER= 7,  /* Nested .wubu container */
} wubu_pkg_payload_t;

/* Architecture */
typedef enum {
    WUBU_PKG_ARCH_X86_64  = 0,
    WUBU_PKG_ARCH_AARCH64 = 1,
    WUBU_PKG_ARCH_RISCV64 = 2,
    WUBU_PKG_ARCH_ANY     = 255,  /* Architecture-independent */
} wubu_pkg_arch_t;

/* ============================================================
 * Package Manifest (in-container JSON)
 * ============================================================ */
typedef struct {
    char id[WUBU_PKG_MAX_NAME];           /* Unique package ID (lowercase, hyphens) */
    char name[WUBU_PKG_MAX_NAME];         /* Display name */
    char version[WUBU_PKG_MAX_VER];       /* SemVer */
    char description[WUBU_PKG_MAX_DESC];
    char maintainer[WUBU_PKG_MAX_NAME];
    char homepage[256];
    char license[64];
    
    /* Dependencies */
    char depends[WUBU_PKG_MAX_DEPS][WUBU_PKG_MAX_NAME];
    int n_depends;
    char recommends[WUBU_PKG_MAX_DEPS][WUBU_PKG_MAX_NAME];
    int n_recommends;
    char conflicts[WUBU_PKG_MAX_DEPS][WUBU_PKG_MAX_NAME];
    int n_conflicts;
    char provides[WUBU_PKG_MAX_DEPS][WUBU_PKG_MAX_NAME];
    int n_provides;
    
    /* Entrypoints (for desktop integration) */
    struct {
        char id[WUBU_PKG_MAX_NAME];       /* e.g., "main", "editor", "server" */
        char name[WUBU_PKG_MAX_NAME];     /* Display name */
        char exec[256];                   /* Command line with %f %u placeholders */
        char icon[256];                   /* Icon path in payload */
        char categories[256];             /* Desktop categories */
        char mime_types[256];             /* MIME types handled */
        bool terminal;                    /* Run in terminal */
        bool startup_notify;              /* Startup notification */
    } entrypoints[WUBU_PKG_MAX_ENTRY];
    int n_entrypoints;
    
    /* Files to install (relative paths in payload) */
    struct {
        char src[512];                    /* Source path in payload */
        char dst[512];                    /* Destination (relative to prefix) */
        uint32_t mode;                    /* File mode */
    } files[WUBU_PKG_MAX_FILES];
    int n_files;
    
    /* Runtime requirements */
    wubu_pkg_payload_t payload_type;
    wubu_pkg_arch_t arch;
    uint64_t min_ram_mb;                  /* Minimum RAM */
    uint64_t min_disk_mb;                 /* Minimum disk space */
    char required_libs[WUBU_PKG_MAX_DEPS][256];  /* System libraries */
    int n_required_libs;
    
    /* Sandbox profile */
    char sandbox_profile[64];             /* default, game, dos, proton, strict, dev */
    
    /* Build info */
    char build_host[256];
    char build_date[32];                  /* ISO 8601 */
    char builder_version[32];
} wubu_pkg_manifest_t;

/* ============================================================
 * Installed Package Database Entry
 * ============================================================ */
typedef struct {
    wubu_pkg_manifest_t manifest;
    char install_path[512];               /* Where installed (e.g., /usr/lib/wubu/pkg-name) */
    char install_date[32];                /* ISO 8601 */
    bool auto_installed;                  /* Installed as dependency */
    uint64_t size_bytes;                  /* Installed size */
    char file_hashes[WUBU_PKG_MAX_FILES][65];  /* SHA256 of each file */
    int n_file_hashes;
} wubu_pkg_installed_t;

/* ============================================================
 * Repository Index Entry
 * ============================================================ */
typedef struct {
    char id[WUBU_PKG_MAX_NAME];
    char name[WUBU_PKG_MAX_NAME];
    char version[WUBU_PKG_MAX_VER];
    char description[WUBU_PKG_MAX_DESC];
    char download_url[512];
    char sha256[65];
    uint64_t download_size;
    uint64_t installed_size;
    wubu_pkg_arch_t arch;
    char depends[WUBU_PKG_MAX_DEPS][WUBU_PKG_MAX_NAME];
    int n_depends;
    char provides[WUBU_PKG_MAX_DEPS][WUBU_PKG_MAX_NAME];
    int n_provides;
} wubu_pkg_repo_entry_t;

/* ============================================================
 * Repository
 * ============================================================ */
typedef struct {
    char name[64];
    char url[512];                        /* Base URL (e.g., https://repo.wubuos.org/stable) */
    char index_url[512];                  /* Index JSON URL */
    char pubkey[128];                     /* Ed25519 public key (hex) */
    bool enabled;
    int priority;                         /* Higher = preferred */
    wubu_pkg_repo_entry_t* packages;
    int n_packages;
    time_t last_update;
} wubu_pkg_repo_t;

/* ============================================================
 * Package Manager Configuration
 * ============================================================ */
typedef struct {
    char db_path[512];                    /* SQLite database path */
    char cache_dir[512];                  /* Download cache */
    char install_prefix[512];             /* Default install prefix */
    char repo_config_dir[512];            /* Repo configs */
    int max_parallel_downloads;
    bool verify_signatures;               /* Require valid signatures */
    bool allow_untrusted;                 /* Allow unsigned packages */
    bool auto_update_index;               /* Auto-refresh repo index */
    int index_ttl_hours;                  /* Index cache TTL */
} wubu_pkgmgr_config_t;

/* ============================================================
 * Transaction (for atomic operations)
 * ============================================================ */
typedef enum {
    WUBU_PKG_TXN_INSTALL = 0,
    WUBU_PKG_TXN_REMOVE  = 1,
    WUBU_PKG_TXN_UPGRADE = 2,
    WUBU_PKG_TXN_DOWNGRADE = 3,
} wubu_pkg_txn_type_t;

typedef struct {
    wubu_pkg_txn_type_t type;
    char pkg_id[WUBU_PKG_MAX_NAME];
    char old_version[WUBU_PKG_MAX_VER];
    char new_version[WUBU_PKG_MAX_VER];
    bool is_dependency;
} wubu_pkg_txn_item_t;

typedef struct {
    wubu_pkg_txn_item_t items[128];
    int n_items;
    bool dry_run;
    char rollback_dir[512];               /* For rollback on failure */
} wubu_pkg_transaction_t;

/* ============================================================
 * Public API
 * ============================================================ */

/* Initialize package manager */
bool wubu_pkgmgr_init(const wubu_pkgmgr_config_t* config);
void wubu_pkgmgr_shutdown(void);

/* Get default configuration */
void wubu_pkgmgr_get_default_config(wubu_pkgmgr_config_t* config);

/* ============================================================
 * Repository Management
 * ============================================================ */

/* Add repository */
bool wubu_pkgmgr_repo_add(const char* name, const char* url, const char* pubkey_hex, int priority);
/* Remove repository */
bool wubu_pkgmgr_repo_remove(const char* name);
/* Enable/disable repository */
bool wubu_pkgmgr_repo_set_enabled(const char* name, bool enabled);
/* List repositories */
int wubu_pkgmgr_repo_list(wubu_pkg_repo_t* out, int max);
/* Refresh repository index */
bool wubu_pkgmgr_repo_update(const char* name);  /* NULL = all enabled */

/* Search packages */
int wubu_pkgmgr_search(const char* query, wubu_pkg_repo_entry_t* out, int max);
/* Get package info from repo */
bool wubu_pkgmgr_repo_get_info(const char* pkg_id, wubu_pkg_repo_entry_t* out);

/* ============================================================
 * Package Operations
 * ============================================================ */

/* Install package (from repo or local .wubu file) */
bool wubu_pkgmgr_install(const char* pkg_spec, bool dry_run);
/* Remove package */
bool wubu_pkgmgr_remove(const char* pkg_id, bool auto_remove_deps);
/* Upgrade package */
bool wubu_pkgmgr_upgrade(const char* pkg_spec, bool dry_run);
/* Upgrade all */
bool wubu_pkgmgr_upgrade_all(bool dry_run);

/* List installed packages */
int wubu_pkgmgr_list_installed(wubu_pkg_installed_t* out, int max);
/* Get installed package info */
bool wubu_pkgmgr_get_installed(const char* pkg_id, wubu_pkg_installed_t* out);
/* Check if installed */
bool wubu_pkgmgr_is_installed(const char* pkg_id);

/* ============================================================
 * .wubu Container Operations
 * ============================================================ */

/* Create .wubu package from directory */
bool wubu_pkgmgr_create_package(const char* src_dir, const char* output_path, 
                                 const wubu_pkg_manifest_t* manifest,
                                 const char* sign_key_hex);  /* NULL = no signature */

/* Verify .wubu package */
bool wubu_pkgmgr_verify_package(const char* pkg_path, const char* pubkey_hex);

/* Extract .wubu package (for inspection) */
bool wubu_pkgmgr_extract_package(const char* pkg_path, const char* dest_dir);

/* Read manifest from .wubu */
bool wubu_pkgmgr_read_manifest(const char* pkg_path, wubu_pkg_manifest_t* out);

/* ============================================================
 * Dependency Resolution
 * ============================================================ */

/* Resolve dependencies for install */
int wubu_pkgmgr_resolve_deps(const char** pkg_ids, int n_pkgs, 
                              char*** out_pkg_ids, int* out_n_pkgs);

/* Check for conflicts */
bool wubu_pkgmgr_check_conflicts(const char* pkg_id, const wubu_pkg_installed_t* installed, int n_installed);

/* ============================================================
 * Transaction Support
 * ============================================================ */

/* Begin transaction */
bool wubu_pkgmgr_txn_begin(wubu_pkg_transaction_t* txn, bool dry_run);
/* Add item to transaction */
bool wubu_pkgmgr_txn_add(wubu_pkg_transaction_t* txn, wubu_pkg_txn_type_t type,
                          const char* pkg_id, const char* old_ver, const char* new_ver,
                          bool is_dep);
/* Commit transaction */
bool wubu_pkgmgr_txn_commit(wubu_pkg_transaction_t* txn);
/* Rollback transaction */
bool wubu_pkgmgr_txn_rollback(wubu_pkg_transaction_t* txn);

/* ============================================================
 * App Integration (Desktop/MIME/Startup)
 * ============================================================ */

/* Register installed app with desktop system */
bool wubu_pkgmgr_register_desktop(const wubu_pkg_installed_t* pkg);
/* Unregister app */
bool wubu_pkgmgr_unregister_desktop(const char* pkg_id);

/* Generate .desktop files for entrypoints */
bool wubu_pkgmgr_generate_desktop_files(const wubu_pkg_installed_t* pkg);

/* ============================================================
 * Cleanup/Maintenance
 * ============================================================ */

/* Clean download cache */
bool wubu_pkgmgr_clean_cache(int max_age_days);
/* Remove orphaned packages (not required by any installed) */
int wubu_pkgmgr_autoremove(bool dry_run);
/* Verify installed packages (check hashes) */
int wubu_pkgmgr_verify_installed(char*** out_broken, int* out_n_broken);

/* ============================================================
 * Statistics/Info
 * ============================================================ */

typedef struct {
    int total_packages;
    int total_repos;
    int installed_count;
    uint64_t total_size_bytes;
    uint64_t cache_size_bytes;
    int available_upgrades;
} wubu_pkgmgr_stats_t;

bool wubu_pkgmgr_get_stats(wubu_pkgmgr_stats_t* out);

/* ============================================================
 * Callback Types (for progress reporting)
 * ============================================================ */

typedef void (*wubu_pkgmgr_progress_cb)(void* userdata, const char* stage, 
                                         const char* pkg_id, float progress, const char* message);

/* Set progress callback */
void wubu_pkgmgr_set_progress_callback(wubu_pkgmgr_progress_cb cb, void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_PKGMGR_H */