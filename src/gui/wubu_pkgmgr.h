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
#include <stddef.h>
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
    /* Filled by the install/remove paths during commit so rollback can undo
     * this item.  applied_id is the TRUE id touched (manifest id for install,
     * requested id for remove) -- the txn pkg_id may be a local .wubu path. */
    bool backup_taken;
    char applied_id[WUBU_PKG_MAX_NAME];
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

/* ============================================================
 * Trust-Anchor Revocation (Ed25519)
 * ============================================================
 * The authoritative revocation list lives in the pkgmgr SQLite DB
 * (revoked_keys table); wubu_pkgmgr_init() reloads a bounded in-process
 * cache from it, so a revocation persists across process restarts.  Keys
 * use the same canonical lowercase 64-char hex form as repo pubkeys and
 * the verifier; no secret material is ever stored here.
 *
 * wubu_pkgmgr_verify_against_repos() skips revoked anchors and fails closed
 * when no usable anchor remains, so both verification and install refuse a
 * package signed only by a revoked key. */
#define WUBU_PKGMGR_MAX_REVOKED_KEYS 64

/* Revoke an Ed25519 public key (persisted; idempotent). */
bool wubu_pkgmgr_key_revoke(const char* pubkey_hex);
/* Lift a revocation, e.g. after rotation to a new key (persisted). */
bool wubu_pkgmgr_key_unrevoke(const char* pubkey_hex);
/* True if the key is revoked in the reloaded cache. */
bool wubu_pkgmgr_key_is_revoked(const char* pubkey_hex);
/* Number of revoked keys currently cached. */
int wubu_pkgmgr_key_revoked_count(void);

/* --- Authorized operator surface (CLI: tools/wubu_pkg_key_ctl) ------
 * Revocation is a security decision: it must be signed by a one-time
 * bootstrapped admin Ed25519 key.  The exact signed command string is
 *   WUBU-PKG-REVOCATION-v1\n<op>\n<key>
 * where <op> is "revoke" or "unrevoke" and <key> is the canonical lowercase
 * 64-hex target key.  Verification happens BEFORE any DB write: a missing
 * admin key, a wrong key, an absent/tampered signature, or a tampered op/key
 * all refuse without touching the database. */
#define WUBU_PKG_REVOCATION_DOMAIN "WUBU-PKG-REVOCATION-v1"

/* Authenticated revocation-feed import.  A feed is a bounded newline-
 * delimited JSON text file whose entries are revoke/unrevoke operations;
 * see src/gui/wubu_pkgmgr_revoke_feed.c for the exact grammar.  The admin
 * signature is over a domain-separated SHA-256 of the EXACT feed file bytes
 * (distinct from the single-command domain below) and is verified before
 * any database mutation.  Entries are then applied atomically. */
#define WUBU_PKG_REVOCATION_FEED_DOMAIN "WUBU-PKG-REVOCATION-v1/feed"
#define WUBU_PKGMGR_FEED_MAX_BYTES  (64u * 1024u)
#define WUBU_PKGMGR_FEED_MAX_ENTRIES 64u

/* Import a signed revocation feed.  This is a single-signature convenience
 * wrapper, NOT a bypass: it feeds its one signature into the SAME global
 * M-of-N verifier used by wubu_pkgmgr_key_feed_import_quorum().  With an
 * explicit M-of-N policy configured (e.g. 2-of-2) a single signature is
 * refused; with no explicit policy the default is 1-of-1 of the bootstrapped
 * admin key.  There is no separate admin-only verification path.
 * sig_hex is 128 lowercase hex chars.  Returns false -- with no DB change --
 * for unsigned/wrong-key/tampered feeds, malformed lines, duplicate/conflicting
 * entries, bad key hex, oversized feeds, and any unmet quorum policy.
 * Re-applying an already-applied feed is a no-op.  On success *out_applied
 * receives the number of entries in the feed. */
bool wubu_pkgmgr_key_feed_import(const char *feed_path, const char *sig_hex,
                                 size_t *out_applied, char *err, size_t errsz);

/* Build the exact command string used for both signing and verification. */
bool wubu_pkgmgr_key_command_string(const char *op, const char *pubkey_hex,
                                    char *out, size_t outsz);
/* One-time bootstrap of the admin public key.  Bootstrapping a DIFFERENT key
 * afterwards is refused; the same key is an idempotent no-op. */
bool wubu_pkgmgr_key_admin_bootstrap(const char *admin_pubkey_hex);
/* Copy the bootstrapped admin public key (hex). False if none recorded. */
bool wubu_pkgmgr_key_admin_pubkey(char *out, size_t outsz);
/* Authorized mutations: sig_hex is 128 lowercase hex chars (a 64-byte
 * Ed25519 signature) over the command string above. */
bool wubu_pkgmgr_key_revoke_authorized(const char *pubkey_hex, const char *sig_hex);
bool wubu_pkgmgr_key_unrevoke_authorized(const char *pubkey_hex, const char *sig_hex);
/* Enumerate revoked keys (canonical lowercase hex) into out[i][65]. */
int wubu_pkgmgr_key_revoked_list(char out[][65], int max);

/* ============================================================
 * Trust-anchor expiry metadata
 * ============================================================
 * An Ed25519 repo trust anchor may carry an absolute expiry (Unix epoch
 * seconds).  expires_at == 0 means "no expiry" so legacy anchors stay valid.
 * wubu_pkgmgr_verify_against_repos() SKIPS expired anchors and fails closed
 * when no usable anchor remains.  Authoritative rows live in the pkgmgr DB;
 * a bounded in-process cache is reloaded by wubu_pkgmgr_init(). */
#define WUBU_PKGMGR_MAX_ANCHOR_EXPIRIES 64

/* Install/clear expiry for a canonical 64-hex anchor key (0 clears).
 *
 * SECURITY: setting expiry is an operator decision.  The ONLY public mutator
 * is authenticated: wubu_pkgmgr_anchor_set_expiry_authorized() verifies an
 * admin Ed25519 signature over the domain-separated command below BEFORE
 * any DB write.  The raw writer is internal-only (see wubu_pkgmgr_internal.h)
 * and exists solely so differential gates can show the pre-change, unsigned
 * behavior; it is deliberately absent from this public surface. */
#define WUBU_PKG_EXPIRY_DOMAIN "WUBU-PKG-EXPIRY-v1"

/* Build the exact signed command: WUBU-PKG-EXPIRY-v1\n<key>\n<epoch>. */
bool wubu_pkgmgr_anchor_expiry_command_string(const char *pubkey_hex,
                                               int64_t expires_at,
                                               char *out, size_t outsz);
/* Authenticated expiry mutation. sig_hex is 128 lowercase hex chars; a
 * missing/wrong-key/tampered signature refuses with NO database change. */
bool wubu_pkgmgr_anchor_set_expiry_authorized(const char *pubkey_hex,
                                               int64_t expires_at,
                                               const char *sig_hex);
/* True if the key has expiry metadata; *out_expires_at receives it. */
bool wubu_pkgmgr_anchor_expiry(const char *pubkey_hex, int64_t *out_expires_at);
/* True only for a key whose expiry is set and already in the past. */
bool wubu_pkgmgr_anchor_is_expired(const char *pubkey_hex);

/* ============================================================
 * Supported admin-key rotation / recovery
 * ============================================================
 * TOFU bootstrap is one-time and has no re-bootstrap path.  Rotation is
 * signed by the CURRENT admin key over exactly:
 *     WUBU-PKG-ADMIN-ROTATION-v1\n<new_admin_pubkey_hex>
 * Verification happens BEFORE the single-row update; the new key replaces
 * the old atomically, and the old key loses all authority immediately. */
#define WUBU_PKG_ADMIN_ROTATION_DOMAIN "WUBU-PKG-ADMIN-ROTATION-v1"
bool wubu_pkgmgr_key_rotate_command_string(const char *new_pubkey_hex,
                                            char *out, size_t outsz);
bool wubu_pkgmgr_key_admin_rotate(const char *new_pubkey_hex, const char *sig_hex);

/* ============================================================
 * M-of-N quorum policy for feed apply
 * ============================================================
 * Bounded policy table: M required valid signatures from N distinct member
 * keys (1 <= M <= N <= WUBU_PKGMGR_MAX_QUORUM_KEYS).  Configuration is itself
 * signed by the current admin key.  Feed import verifies M distinct member
 * signatures over the EXACT feed bytes BEFORE any DB read/write, then applies
 * every entry in one transaction (all-or-nothing).  This is the ONE global
 * feed-authentication policy: the legacy single-signature importer above is
 * a wrapper over this verifier and cannot bypass it. */
#define WUBU_PKGMGR_MAX_QUORUM_KEYS 8
#define WUBU_PKG_QUORUM_DOMAIN "WUBU-PKG-QUORUM-v1"

bool wubu_pkgmgr_key_quorum_command_string(int m, int n, const char *const *keys,
                                            char *out, size_t outsz);
bool wubu_pkgmgr_key_quorum_configure(int m, int n, const char *const *keys,
                                      const char *sig_hex);
bool wubu_pkgmgr_key_quorum_status(int *out_m, int *out_n);
bool wubu_pkgmgr_key_feed_import_quorum(const char *feed_path,
                                        const char *const *sigs, size_t n_sigs,
                                        size_t *out_applied, char *err, size_t errsz);

/* ============================================================
 * Supported lost-admin recovery via bounded escrow set
 * ============================================================
 * The current admin key can authorize normal rotation, but if it is LOST
 * there must be a recovery path that does not need it.  An admin-signed
 * configuration installs a bounded escrow set of M-of-N public keys
 * (2 <= M <= N <= WUBU_PKGMGR_MAX_ESCROW_KEYS).  Recovery verifies M DISTINCT
 * escrow signatures over the domain-separated command below and, only on
 * success, atomically replaces the admin key with the new one.  The old admin
 * key loses all authority immediately; no re-bootstrap path exists.
 * All keys are public hex; no secret material is stored in the DB. */
#define WUBU_PKGMGR_MAX_ESCROW_KEYS 8
#define WUBU_PKG_ESCROW_DOMAIN "WUBU-PKG-ESCROW-v1"
#define WUBU_PKG_ESCROW_RECOVERY_DOMAIN "WUBU-PKG-ESCROW-RECOVERY-v1"

/* Escrow configuration command (signed by CURRENT admin):
 *   WUBU-PKG-ESCROW-v1\n<M>\n<N>\n<key1>\n...\n<keyN> */
bool wubu_pkgmgr_escrow_command_string(int m, int n, const char *const *keys,
                                        char *out, size_t outsz);
/* Install/replace the escrow set. M must be >= 2 (multi-party recovery). */
bool wubu_pkgmgr_escrow_configure(int m, int n, const char *const *keys,
                                  const char *sig_hex);
bool wubu_pkgmgr_escrow_status(int *out_m, int *out_n);

/* Recovery command (signed by each escrow member):
 *   WUBU-PKG-ESCROW-RECOVERY-v1\n<new_admin_pubkey_hex> */
bool wubu_pkgmgr_escrow_recovery_command_string(const char *new_pubkey_hex,
                                                char *out, size_t outsz);
/* Install a new admin key from M distinct escrow signatures. Verification is
 * BEFORE any DB write; the admin-key row is replaced in one transaction and
 * reloaded before this call returns true. */
bool wubu_pkgmgr_key_admin_recover(const char *new_pubkey_hex,
                                   const char *const *sigs, size_t n_sigs);

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