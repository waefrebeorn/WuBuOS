/*
 * wubu_pkgmgr.c  --  WuBuOS Package Manager (Facade)
 *
 * Submodules:
 *   wubu_pkgmgr_pkg.c     - package create/verify/extract/read-manifest
 *   wubu_pkgmgr_install.c - install/remove/upgrade/list
 *   wubu_pkgmgr_txn.c     - transactions, desktop registration
 */

#include "wubu_pkgmgr_internal.h"
#include "wubu_pkg_sign.h"
#include "wubu_pkgmgr_revoke_feed.h"

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
    "  download_url TEXT DEFAULT '',"
    "  sha256 TEXT DEFAULT '',"
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
    ");"
    /* Authoritative Ed25519 trust-anchor revocations.  Public keys only —
     * no private/secret material is ever stored in the DB or the repo. */
    "CREATE TABLE IF NOT EXISTS revoked_keys ("
    "  pubkey_hex TEXT PRIMARY KEY,"
    "  revoked_at INTEGER DEFAULT 0"
    ");"
    /* One-time operator/admin Ed25519 public key.  Single row (id = 1);
     * public key only -- the seed never enters the DB or the repository. */
    "CREATE TABLE IF NOT EXISTS pkg_admin_key ("
    "  id INTEGER PRIMARY KEY CHECK (id = 1),"
    "  pubkey_hex TEXT NOT NULL,"
    "  bootstrapped_at INTEGER DEFAULT 0"
    ");"
    /* Trust-anchor expiry metadata (public keys only, no secret material).
     * expires_at == 0 means the anchor never expires. */
    "CREATE TABLE IF NOT EXISTS anchor_expiry ("
    "  pubkey_hex TEXT PRIMARY KEY,"
    "  expires_at INTEGER NOT NULL DEFAULT 0"
    ");"
    /* Bounded M-of-N feed-apply policy: one config row (m, n) and its N
     * member public keys. All rows are public; seeds never enter the DB. */
    "CREATE TABLE IF NOT EXISTS pkg_quorum ("
    "  id INTEGER PRIMARY KEY CHECK (id = 1),"
    "  m INTEGER NOT NULL,"
    "  n INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS pkg_quorum_keys ("
    "  pubkey_hex TEXT PRIMARY KEY,"
    "  seq INTEGER NOT NULL"
    ");";

/* -- Initialize ---------------------------------------------------- */

/* Migrate pre-existing databases: CREATE TABLE IF NOT EXISTS leaves an old
 * repo_packages table untouched, so columns added later must be ALTERed in.
 * Returns false only on a real SQLite failure (a missing column that cannot
 * be added would break every index insert downstream). */
static bool migrate_repo_packages(void) {
    bool have_url = false, have_sha = false;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db, "PRAGMA table_info(repo_packages)",
                           -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *col = (const char *)sqlite3_column_text(stmt, 1);
        if (!col) continue;
        if (strcmp(col, "download_url") == 0) have_url = true;
        if (strcmp(col, "sha256") == 0) have_sha = true;
    }
    sqlite3_finalize(stmt);
    if (!have_url &&
        db_exec("ALTER TABLE repo_packages ADD COLUMN download_url TEXT DEFAULT ''") != 0)
        return false;
    if (!have_sha &&
        db_exec("ALTER TABLE repo_packages ADD COLUMN sha256 TEXT DEFAULT ''") != 0)
        return false;
    return true;
}

/* -- Trust-anchor revocation: ONE owner (this facade) --------------
 * Authoritative state: revoked_keys rows in the pkgmgr DB.  The bounded
 * cache below is rebuilt from those rows by keyrev_reload() on every init,
 * so a revocation survives a process restart. */

/* Canonical form matches hex_to_32() in wubu_pkg_ed25519.c: 64 lowercase
 * hex chars.  Never silently accept a form the verifier would reject. */
static bool revoked_key_valid(const char *hex) {
    if (!hex || strlen(hex) != 64) return false;
    for (size_t i = 0; i < 64; i++) {
        char c = hex[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

static int cb_load_revoked(void *data, int argc, char **argv, char **col) {
    (void)data; (void)col;
    if (argc < 1 || !argv[0] || !revoked_key_valid(argv[0])) return 0;
    if (g_pkgmgr.n_revoked_keys >= WUBU_PKGMGR_MAX_REVOKED_KEYS) return 0;
    snprintf(g_pkgmgr.revoked_keys[g_pkgmgr.n_revoked_keys], 65, "%s", argv[0]);
    g_pkgmgr.n_revoked_keys++;
    return 0;
}

static bool keyrev_reload(void) {
    g_pkgmgr.n_revoked_keys = 0;
    if (!g_pkgmgr.db) return false;
    return db_query("SELECT pubkey_hex FROM revoked_keys ORDER BY pubkey_hex",
                    cb_load_revoked, NULL) == 0;
}

bool wubu_pkgmgr_key_revoke(const char *pubkey_hex) {
    if (!revoked_key_valid(pubkey_hex) || !g_pkgmgr.db) return false;
    if (wubu_pkgmgr_key_is_revoked(pubkey_hex)) return true;   /* idempotent */
    if (g_pkgmgr.n_revoked_keys >= WUBU_PKGMGR_MAX_REVOKED_KEYS) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "INSERT OR IGNORE INTO revoked_keys (pubkey_hex, revoked_at) "
            "VALUES (?1, ?2)", -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_text(stmt, 1, pubkey_hex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(NULL));
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && keyrev_reload();
}

bool wubu_pkgmgr_key_unrevoke(const char *pubkey_hex) {
    if (!revoked_key_valid(pubkey_hex) || !g_pkgmgr.db) return false;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "DELETE FROM revoked_keys WHERE pubkey_hex = ?1", -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_text(stmt, 1, pubkey_hex, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && keyrev_reload();
}

bool wubu_pkgmgr_key_is_revoked(const char *pubkey_hex) {
    if (!revoked_key_valid(pubkey_hex)) return false;
    for (int i = 0; i < g_pkgmgr.n_revoked_keys; i++)
        if (strcmp(g_pkgmgr.revoked_keys[i], pubkey_hex) == 0) return true;
    return false;
}

int wubu_pkgmgr_key_revoked_count(void) { return g_pkgmgr.n_revoked_keys; }

/* -- Trust-anchor expiry: ONE owner (this facade) -------------------
 * Authoritative state is the anchor_expiry table; the bounded cache is
 * rebuilt on init, exactly like revocations. No crypto lives here. */
static int cb_load_expiry(void *data, int argc, char **argv, char **col) {
    (void)data; (void)col;
    if (argc < 2 || !argv[0] || !revoked_key_valid(argv[0])) return 0;
    if (g_pkgmgr.n_expiries >= WUBU_PKGMGR_MAX_ANCHOR_EXPIRIES) return 0;
    int64_t exp = strtoll(argv[1], NULL, 10);
    if (exp < 0) exp = 0;
    snprintf(g_pkgmgr.expiry_keys[g_pkgmgr.n_expiries], 65, "%s", argv[0]);
    g_pkgmgr.expiry_at[g_pkgmgr.n_expiries] = exp;
    g_pkgmgr.n_expiries++;
    return 0;
}

static bool expiry_reload(void) {
    g_pkgmgr.n_expiries = 0;
    if (!g_pkgmgr.db) return false;
    return db_query("SELECT pubkey_hex, expires_at FROM anchor_expiry "
                    "ORDER BY pubkey_hex", cb_load_expiry, NULL) == 0;
}

bool wubu_pkgmgr_anchor_set_expiry(const char *pubkey_hex, int64_t expires_at) {
    if (!revoked_key_valid(pubkey_hex) || !g_pkgmgr.db || expires_at < 0)
        return false;
    sqlite3_stmt *stmt = NULL;
    if (expires_at == 0) {
        if (sqlite3_prepare_v2(g_pkgmgr.db,
                "DELETE FROM anchor_expiry WHERE pubkey_hex = ?1",
                -1, &stmt, NULL) != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            return false;
        }
    } else {
        if (sqlite3_prepare_v2(g_pkgmgr.db,
                "INSERT OR REPLACE INTO anchor_expiry (pubkey_hex, expires_at) "
                "VALUES (?1, ?2)", -1, &stmt, NULL) != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)expires_at);
    }
    sqlite3_bind_text(stmt, 1, pubkey_hex, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && expiry_reload();
}

bool wubu_pkgmgr_anchor_expiry(const char *pubkey_hex, int64_t *out_expires_at) {
    if (!revoked_key_valid(pubkey_hex)) return false;
    for (int i = 0; i < g_pkgmgr.n_expiries; i++) {
        if (strcmp(g_pkgmgr.expiry_keys[i], pubkey_hex) == 0) {
            if (out_expires_at) *out_expires_at = g_pkgmgr.expiry_at[i];
            return true;
        }
    }
    return false;
}

bool wubu_pkgmgr_anchor_is_expired(const char *pubkey_hex) {
    int64_t exp = 0;
    if (!wubu_pkgmgr_anchor_expiry(pubkey_hex, &exp)) return false; /* no metadata */
    if (exp == 0) return false;                                    /* never expires */
    return (int64_t)time(NULL) >= exp;
}

/* -- Authorized operator surface ------------------------------------
 * Policy owner: this facade.  Crypto stays in wubu_pkg_ed25519.c; the CLI
 * (tools/wubu_pkg_key_ctl) is a thin argument parser over these calls. */

bool wubu_pkgmgr_key_command_string(const char *op, const char *pubkey_hex,
                                    char *out, size_t outsz) {
    if (!out || outsz < 16) return false;
    if (!revoked_key_valid(pubkey_hex)) return false;
    if (!op || (strcmp(op, "revoke") != 0 && strcmp(op, "unrevoke") != 0))
        return false;
    int n = snprintf(out, outsz, "%s\n%s\n%s",
                     WUBU_PKG_REVOCATION_DOMAIN, op, pubkey_hex);
    return n > 0 && (size_t)n < outsz;
}

static int cb_load_admin(void *data, int argc, char **argv, char **col) {
    (void)data; (void)col;
    if (argc < 1 || !argv[0] || !revoked_key_valid(argv[0])) return 0;
    snprintf(g_pkgmgr.admin_pubkey, sizeof(g_pkgmgr.admin_pubkey), "%s", argv[0]);
    return 0;
}

static bool admin_reload(void) {
    g_pkgmgr.admin_pubkey[0] = '\0';
    if (!g_pkgmgr.db) return false;
    return db_query("SELECT pubkey_hex FROM pkg_admin_key WHERE id = 1",
                    cb_load_admin, NULL) == 0;
}

bool wubu_pkgmgr_key_admin_bootstrap(const char *admin_pubkey_hex) {
    if (!revoked_key_valid(admin_pubkey_hex) || !g_pkgmgr.db) return false;
    if (g_pkgmgr.admin_pubkey[0]) {
        /* One-time: only the identical key may be re-submitted (no-op). */
        return strcmp(g_pkgmgr.admin_pubkey, admin_pubkey_hex) == 0;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "INSERT INTO pkg_admin_key (id, pubkey_hex, bootstrapped_at) "
            "VALUES (1, ?1, ?2)", -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_text(stmt, 1, admin_pubkey_hex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(NULL));
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && admin_reload();
}

bool wubu_pkgmgr_key_admin_pubkey(char *out, size_t outsz) {
    if (!out || outsz < 65 || !g_pkgmgr.admin_pubkey[0]) return false;
    snprintf(out, outsz, "%s", g_pkgmgr.admin_pubkey);
    return true;
}

/* Decode 128 lowercase hex chars into a 64-byte Ed25519 signature. */
static bool sig_hex_to_bytes(const char *hex, uint8_t out[64]) {
    if (!hex || !out || strlen(hex) != 128) return false;
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < 128; i += 2) {
        const char *p1 = strchr(digits, hex[i]);
        const char *p2 = strchr(digits, hex[i + 1]);
        if (!p1 || !p2) return false;
        out[i / 2] = (uint8_t)(((p1 - digits) << 4) | (p2 - digits));
    }
    return true;
}

/* Verify the operator signature over the exact domain-separated command.
 * No DB write occurs here; callers proceed only on true. */
static bool authorize_command(const char *op, const char *pubkey_hex,
                              const char *sig_hex) {
    char cmd[256];
    uint8_t sig[64];
    char err[256];
    if (!wubu_pkgmgr_key_command_string(op, pubkey_hex, cmd, sizeof(cmd)))
        return false;
    if (!g_pkgmgr.admin_pubkey[0]) return false;      /* no bootstrap: fail closed */
    if (!sig_hex_to_bytes(sig_hex, sig)) return false; /* unsigned/malformed */
    return wubu_pkg_verify_command(cmd, g_pkgmgr.admin_pubkey, sig,
                                   err, sizeof(err));
}

/* -- Supported admin-key rotation / recovery ------------------------
 * Signed by the CURRENT admin key; verification is BEFORE the single-row
 * UPDATE.  Reusing wubu_pkg_verify_command means no second crypto path. */
bool wubu_pkgmgr_key_rotate_command_string(const char *new_pubkey_hex,
                                            char *out, size_t outsz) {
    if (!out || outsz < 16 || !revoked_key_valid(new_pubkey_hex)) return false;
    int n = snprintf(out, outsz, "%s\n%s",
                     WUBU_PKG_ADMIN_ROTATION_DOMAIN, new_pubkey_hex);
    return n > 0 && (size_t)n < outsz;
}

bool wubu_pkgmgr_key_admin_rotate(const char *new_pubkey_hex, const char *sig_hex) {
    if (!revoked_key_valid(new_pubkey_hex) || !g_pkgmgr.db) return false;
    if (!g_pkgmgr.admin_pubkey[0]) return false;     /* no bootstrap: fail closed */
    char cmd[256];
    uint8_t sig[64];
    char err[256];
    if (!wubu_pkgmgr_key_rotate_command_string(new_pubkey_hex, cmd, sizeof(cmd)))
        return false;
    if (!sig_hex_to_bytes(sig_hex, sig)) return false;   /* unsigned/malformed */
    if (!wubu_pkg_verify_command(cmd, g_pkgmgr.admin_pubkey, sig,
                                 err, sizeof(err))) return false;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "UPDATE pkg_admin_key SET pubkey_hex = ?1, bootstrapped_at = ?2 "
            "WHERE id = 1", -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_text(stmt, 1, new_pubkey_hex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)time(NULL));
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    int changed = sqlite3_changes(g_pkgmgr.db);
    sqlite3_finalize(stmt);
    if (!ok || changed != 1) return false;   /* no row => no silent success */
    return admin_reload();
}

/* -- Bounded M-of-N quorum policy -----------------------------------
 * ONE owner. Config and member keys are authoritative in the DB; the cache
 * below is rebuilt on init. Configuration is admin-signed; feed apply uses
 * wubu_pkg_verify_bytes (the same Ed25519 leaf) for each member signature. */
static int cb_load_quorum(void *data, int argc, char **argv, char **col) {
    (void)data; (void)col;
    if (argc < 2 || !argv[0] || !argv[1]) return 0;
    int m = atoi(argv[0]), n = atoi(argv[1]);
    if (m < 1 || n < m || n > WUBU_PKGMGR_MAX_QUORUM_KEYS) return 0;
    g_pkgmgr.quorum_m = m;
    g_pkgmgr.quorum_n = n;
    return 0;
}

static int cb_load_quorum_key(void *data, int argc, char **argv, char **col) {
    (void)data; (void)col;
    if (argc < 1 || !argv[0] || !revoked_key_valid(argv[0])) return 0;
    if (g_pkgmgr.quorum_nkeys >= WUBU_PKGMGR_MAX_QUORUM_KEYS) return 0;
    snprintf(g_pkgmgr.quorum_keys[g_pkgmgr.quorum_nkeys], 65, "%s", argv[0]);
    g_pkgmgr.quorum_nkeys++;
    return 0;
}

static bool quorum_reload(void) {
    g_pkgmgr.quorum_m = 0;
    g_pkgmgr.quorum_n = 0;
    g_pkgmgr.quorum_nkeys = 0;
    if (!g_pkgmgr.db) return false;
    db_query("SELECT m, n FROM pkg_quorum WHERE id = 1", cb_load_quorum, NULL);
    db_query("SELECT pubkey_hex FROM pkg_quorum_keys ORDER BY seq",
             cb_load_quorum_key, NULL);
    return true;
}

bool wubu_pkgmgr_key_quorum_command_string(int m, int n, const char *const *keys,
                                            char *out, size_t outsz) {
    if (!out || outsz < 16 || !keys) return false;
    if (m < 1 || n < m || n > WUBU_PKGMGR_MAX_QUORUM_KEYS) return false;
    for (int i = 0; i < n; i++) {
        if (!revoked_key_valid(keys[i])) return false;
        for (int j = i + 1; j < n; j++)
            if (strcmp(keys[i], keys[j]) == 0) return false;  /* no dup members */
    }
    int off = snprintf(out, outsz, "%s\n%d\n%d",
                       WUBU_PKG_QUORUM_DOMAIN, m, n);
    if (off < 0 || (size_t)off >= outsz) return false;
    for (int i = 0; i < n; i++) {
        int k = snprintf(out + off, outsz - (size_t)off, "\n%s", keys[i]);
        if (k < 0 || (size_t)k >= outsz - (size_t)off) return false;
        off += k;
    }
    return true;
}

bool wubu_pkgmgr_key_quorum_configure(int m, int n, const char *const *keys,
                                      const char *sig_hex) {
    char cmd[1024];
    uint8_t sig[64];
    char err[256];
    if (!wubu_pkgmgr_key_quorum_command_string(m, n, keys, cmd, sizeof(cmd)))
        return false;
    if (!g_pkgmgr.admin_pubkey[0]) return false;
    if (!sig_hex_to_bytes(sig_hex, sig)) return false;
    if (!wubu_pkg_verify_command(cmd, g_pkgmgr.admin_pubkey, sig,
                                 err, sizeof(err))) return false;
    if (!g_pkgmgr.db) return false;

    /* All-or-nothing config replace: config row and member keys in ONE txn. */
    if (db_exec("BEGIN IMMEDIATE") != 0) return false;
    bool ok = (db_exec("DELETE FROM pkg_quorum") == 0 &&
               db_exec("DELETE FROM pkg_quorum_keys") == 0);
    sqlite3_stmt *stmt = NULL;
    if (ok && sqlite3_prepare_v2(g_pkgmgr.db,
            "INSERT INTO pkg_quorum (id, m, n) VALUES (1, ?1, ?2)",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, m);
        sqlite3_bind_int(stmt, 2, n);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        stmt = NULL;
    } else {
        if (stmt) sqlite3_finalize(stmt);
        ok = false;
    }
    for (int i = 0; i < n && ok; i++) {
        stmt = NULL;
        if (sqlite3_prepare_v2(g_pkgmgr.db,
                "INSERT INTO pkg_quorum_keys (pubkey_hex, seq) VALUES (?1, ?2)",
                -1, &stmt, NULL) != SQLITE_OK) { ok = false; break; }
        sqlite3_bind_text(stmt, 1, keys[i], -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, i);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
    }
    if (ok) ok = (db_exec("COMMIT") == 0);
    if (!ok) { db_exec("ROLLBACK"); return false; }
    return quorum_reload();
}

bool wubu_pkgmgr_key_quorum_status(int *out_m, int *out_n) {
    if (out_m) *out_m = g_pkgmgr.quorum_m;
    if (out_n) *out_n = g_pkgmgr.quorum_n;
    return g_pkgmgr.quorum_m > 0 && g_pkgmgr.quorum_n >= g_pkgmgr.quorum_m;
}

bool wubu_pkgmgr_key_revoke_authorized(const char *pubkey_hex, const char *sig_hex) {
    if (!revoked_key_valid(pubkey_hex)) return false;
    if (!authorize_command("revoke", pubkey_hex, sig_hex)) return false;
    return wubu_pkgmgr_key_revoke(pubkey_hex);
}

bool wubu_pkgmgr_key_unrevoke_authorized(const char *pubkey_hex, const char *sig_hex) {
    if (!revoked_key_valid(pubkey_hex)) return false;
    if (!authorize_command("unrevoke", pubkey_hex, sig_hex)) return false;
    return wubu_pkgmgr_key_unrevoke(pubkey_hex);
}

/* -- Authenticated revocation-feed import ---------------------------
 * Security order is deliberate:
 *   1. read the EXACT feed bytes (bounded);
 *   2. verify the admin signature over a domain-separated hash of those
 *      bytes -- NO database read or write has happened yet;
 *   3. parse the bytes strictly into entries;
 *   4. apply every entry inside ONE SQLite transaction, rolling all back
 *      on the first failure;
 *   5. rebuild the in-process cache exactly once after a successful commit.
 * Steps 2 and 3 are fail-closed, so an unsigned, wrong-key, tampered,
 * malformed, duplicate/conflicting or oversized feed never reaches step 4. */
static void feed_seterr(char *err, size_t errsz, const char *msg) {
    if (err && errsz) {
        strncpy(err, msg, errsz - 1);
        err[errsz - 1] = '\0';
    }
}

/* -- Authenticated revocation-feed import ---------------------------
 * Security order is deliberate:
 *   1. read the EXACT feed bytes (bounded);
 *   2. authenticate -- single admin signature OR M-of-N quorum -- over a
 *      domain-separated hash of those bytes; NO DB read/write yet;
 *   3. parse the bytes strictly into entries;
 *   4. apply every entry inside ONE SQLite transaction, rolling all back
 *      on the first failure;
 *   5. rebuild the in-process cache exactly once after a successful commit.
 * Steps 2 and 3 are fail closed. Both importers share steps 1/3/4/5; only
 * the authentication in step 2 differs -- no crypto is duplicated. */
static bool feed_read(const char *feed_path, uint8_t **out_data, size_t *out_sz,
                      char *err, size_t errsz) {
    *out_data = NULL; *out_sz = 0;
    if (!feed_path || !*feed_path) { feed_seterr(err, errsz, "no feed path"); return false; }
    FILE *f = fopen(feed_path, "rb");
    if (!f) { feed_seterr(err, errsz, "cannot open feed"); return false; }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f); feed_seterr(err, errsz, "cannot seek feed"); return false;
    }
    long szl = ftell(f);
    if (szl < 0 || (size_t)szl > WUBU_PKGMGR_FEED_MAX_BYTES) {
        fclose(f); feed_seterr(err, errsz, "feed exceeds byte limit"); return false;
    }
    size_t sz = (size_t)szl;
    rewind(f);
    uint8_t *data = (uint8_t *)malloc(sz ? sz : 1);
    if (!data) { fclose(f); feed_seterr(err, errsz, "out of memory"); return false; }
    if (sz && fread(data, 1, sz, f) != sz) {
        fclose(f); free(data); feed_seterr(err, errsz, "short feed read"); return false;
    }
    fclose(f);
    *out_data = data; *out_sz = sz;
    return true;
}

/* Steps 3/4/5: strict parse + all-or-nothing apply. Caller owns `data`. */
static bool feed_apply(const uint8_t *data, size_t sz, size_t *out_applied,
                       char *err, size_t errsz) {
    wubu_revoke_feed_entry_t entries[WUBU_PKGMGR_FEED_MAX_ENTRIES];
    size_t n = 0;
    if (!wubu_pkg_revoke_feed_parse(data, sz, entries,
                                    WUBU_PKGMGR_FEED_MAX_ENTRIES, &n, err, errsz))
        return false;

    /* Preflight the cache size so a valid feed cannot overflow it. */
    size_t final_count = (size_t)g_pkgmgr.n_revoked_keys;
    for (size_t i = 0; i < n; i++) {
        bool cur = wubu_pkgmgr_key_is_revoked(entries[i].key);
        if (entries[i].op == WUBU_REVOKE_FEED_REVOKE && !cur) final_count++;
        else if (entries[i].op == WUBU_REVOKE_FEED_UNREVOKE && cur) final_count--;
    }
    if (final_count > WUBU_PKGMGR_MAX_REVOKED_KEYS) {
        feed_seterr(err, errsz, "feed would exceed revocation cache capacity"); return false;
    }
    if (!g_pkgmgr.db) { feed_seterr(err, errsz, "no package DB"); return false; }

    if (db_exec("BEGIN IMMEDIATE") != 0) {
        feed_seterr(err, errsz, "cannot begin feed transaction"); return false;
    }
    bool ok = true;
    time_t now = time(NULL);
    for (size_t i = 0; i < n && ok; i++) {
        const char *sql = entries[i].op == WUBU_REVOKE_FEED_REVOKE
            ? "INSERT OR IGNORE INTO revoked_keys (pubkey_hex, revoked_at) VALUES (?1, ?2)"
            : "DELETE FROM revoked_keys WHERE pubkey_hex = ?1";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(g_pkgmgr.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            ok = false; break;
        }
        sqlite3_bind_text(stmt, 1, entries[i].key, -1, SQLITE_TRANSIENT);
        if (entries[i].op == WUBU_REVOKE_FEED_REVOKE)
            sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    if (ok) ok = (db_exec("COMMIT") == 0);
    if (!ok) {
        db_exec("ROLLBACK");
        feed_seterr(err, errsz, "feed apply failed; all entries rolled back");
        return false;
    }
    if (!keyrev_reload()) {
        feed_seterr(err, errsz, "committed but cache reload failed");
        return false;
    }
    if (out_applied) *out_applied = n;
    return true;
}

/* Original single-admin-signature importer (unchanged semantics). */
bool wubu_pkgmgr_key_feed_import(const char *feed_path, const char *sig_hex,
                                 size_t *out_applied, char *err, size_t errsz) {
    if (out_applied) *out_applied = 0;
    feed_seterr(err, errsz, "");
    uint8_t *data = NULL; size_t sz = 0;
    if (!feed_read(feed_path, &data, &sz, err, errsz)) return false;
    uint8_t sig[64];
    if (!g_pkgmgr.admin_pubkey[0]) {
        free(data); feed_seterr(err, errsz, "no admin key bootstrapped"); return false;
    }
    if (!sig_hex_to_bytes(sig_hex, sig)) {
        free(data); feed_seterr(err, errsz, "missing or malformed signature"); return false;
    }
    bool ok = wubu_pkg_verify_bytes(data, sz, WUBU_PKG_REVOCATION_FEED_DOMAIN,
                                    g_pkgmgr.admin_pubkey, sig, err, errsz);
    if (ok) ok = feed_apply(data, sz, out_applied, err, errsz);
    free(data);
    return ok;
}

/* Quorum authentication: count DISTINCT configured member keys that produce
 * a valid signature over the exact feed bytes. No DB read/write here. */
static bool feed_verify_quorum(const uint8_t *data, size_t sz,
                               const char *const *sigs, size_t n_sigs,
                               char *err, size_t errsz) {
    if (g_pkgmgr.quorum_m < 1 || g_pkgmgr.quorum_n < g_pkgmgr.quorum_m ||
        g_pkgmgr.quorum_nkeys < g_pkgmgr.quorum_n) {
        feed_seterr(err, errsz, "no usable quorum policy configured"); return false;
    }
    if (!sigs || n_sigs < (size_t)g_pkgmgr.quorum_m) {
        feed_seterr(err, errsz, "too few quorum signatures"); return false;
    }
    bool matched[WUBU_PKGMGR_MAX_QUORUM_KEYS] = {0};
    int distinct = 0;
    for (size_t s = 0; s < n_sigs; s++) {
        uint8_t sig[64];
        if (!sig_hex_to_bytes(sigs[s], sig)) continue;   /* malformed ignored */
        for (int k = 0; k < g_pkgmgr.quorum_nkeys; k++) {
            if (matched[k]) continue;
            char e[256];
            if (wubu_pkg_verify_bytes(data, sz, WUBU_PKG_REVOCATION_FEED_DOMAIN,
                                      g_pkgmgr.quorum_keys[k], sig, e, sizeof(e))) {
                matched[k] = true;
                if (++distinct >= g_pkgmgr.quorum_m) break;
            }
        }
        if (distinct >= g_pkgmgr.quorum_m) break;
    }
    if (distinct < g_pkgmgr.quorum_m) {
        feed_seterr(err, errsz, "M-of-N quorum not satisfied"); return false;
    }
    return true;
}

bool wubu_pkgmgr_key_feed_import_quorum(const char *feed_path,
                                        const char *const *sigs, size_t n_sigs,
                                        size_t *out_applied, char *err, size_t errsz) {
    if (out_applied) *out_applied = 0;
    feed_seterr(err, errsz, "");
    uint8_t *data = NULL; size_t sz = 0;
    if (!feed_read(feed_path, &data, &sz, err, errsz)) return false;
    bool ok = feed_verify_quorum(data, sz, sigs, n_sigs, err, errsz);
    if (ok) ok = feed_apply(data, sz, out_applied, err, errsz);
    free(data);
    return ok;
}

int wubu_pkgmgr_key_revoked_list(char out[][65], int max) {
    if (!out || max < 0) return 0;
    int n = g_pkgmgr.n_revoked_keys < max ? g_pkgmgr.n_revoked_keys : max;
    for (int i = 0; i < n; i++)
        snprintf(out[i], 65, "%s", g_pkgmgr.revoked_keys[i]);
    return n;
}

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
    if (!migrate_repo_packages()) { sqlite3_close(g_pkgmgr.db); g_pkgmgr.db = NULL; return false; }

    /* Load repos */
    g_pkgmgr.n_repos = 0;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT name, url, pubkey_hex, priority, enabled, last_update FROM repos");
    db_query(sql, cb_load_repos, NULL);

    /* Authoritative revocations must load before any verify/install path is
     * reachable: a failed reload fails initialization closed. */
    if (!keyrev_reload()) {
        sqlite3_close(g_pkgmgr.db);
        g_pkgmgr.db = NULL;
        return false;
    }

    /* The bootstrapped admin key must be authoritative before any authorized
     * path is reachable: fail initialization closed on a reload error. */
    if (!admin_reload()) {
        sqlite3_close(g_pkgmgr.db);
        g_pkgmgr.db = NULL;
        return false;
    }

    /* Trust-anchor expiry and the M-of-N quorum table are security inputs
     * too: a failed reload must fail init closed, never silently default
     * to "everything is valid / one signature is enough". */
    if (!expiry_reload() || !quorum_reload()) {
        sqlite3_close(g_pkgmgr.db);
        g_pkgmgr.db = NULL;
        return false;
    }

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
    /* name/url/pubkey are operator-supplied strings (and a repo name may
     * originate in a fetched repo config). Bind them: never interpolate. */
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db,
            "INSERT OR REPLACE INTO repos (name, url, pubkey_hex, priority, enabled) "
            "VALUES (?1, ?2, ?3, ?4, 1)", -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, url, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, pubkey_hex ? pubkey_hex : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, priority);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if (!ok) return false;

    /* Reload repos */
    wubu_pkgmgr_shutdown();
    wubu_pkgmgr_init(&g_pkgmgr.config);
    return true;
}

bool wubu_pkgmgr_repo_remove(const char *name) {
    if (!g_pkgmgr.initialized || !name) return false;
    /* Both statements run through bound parameters: a quote-bearing repo
     * name is deleted as a literal row, never executed as extra SQL. */
    static const char *const deletes[] = {
        "DELETE FROM repos WHERE name=?1",
        "DELETE FROM repo_packages WHERE repo_name=?1"
    };
    for (int i = 0; i < 2; i++) {
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(g_pkgmgr.db, deletes[i], -1, &stmt, NULL) != SQLITE_OK) {
            if (stmt) sqlite3_finalize(stmt);
            if (i == 0) return false;
            continue;
        }
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
        bool done = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (i == 0 && !done) return false;
    }
    return true;
}

bool wubu_pkgmgr_repo_set_enabled(const char *name, bool enabled) {
    if (!g_pkgmgr.initialized || !name) return false;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_pkgmgr.db, "UPDATE repos SET enabled=?1 WHERE name=?2",
                           -1, &stmt, NULL) != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_int(stmt, 1, enabled ? 1 : 0);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
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
        /* Orphan ids were read from the packages table (untrusted manifest
         * content originally) -- bind them instead of interpolating. */
        sqlite3_stmt *del = NULL;
        if (sqlite3_prepare_v2(g_pkgmgr.db, "DELETE FROM packages WHERE id=?1",
                               -1, &del, NULL) != SQLITE_OK) {
            if (del) sqlite3_finalize(del);
            continue;
        }
        sqlite3_bind_text(del, 1, orphans[i], -1, SQLITE_TRANSIENT);
        if (sqlite3_step(del) == SQLITE_DONE) removed++;
        sqlite3_finalize(del);
    }

    if (removed > 0)
        fprintf(stderr, "[pkgmgr] Auto-removed %d orphaned packages\n", removed);
    return removed;
}

/* Report real package/repository counts from the live tables. */
static int cb_stats_count(void *data, int argc, char **argv, char **col) {
    (void)argc; (void)col;
    if (argv && argv[0]) *(int *)data = atoi(argv[0]);
    return 0;
}

bool wubu_pkgmgr_get_stats(wubu_pkgmgr_stats_t *out) {
    if (!g_pkgmgr.initialized || !out) return false;
    memset(out, 0, sizeof(*out));
    int count = 0;
    /* db_query invokes the callback once per row; passing NULL left
     * installed_count permanently at 0 even with packages installed. */
    db_query("SELECT COUNT(*) FROM packages", cb_stats_count, &count);
    out->installed_count = count;
    out->total_repos = g_pkgmgr.n_repos;
    return true;
}

void wubu_pkgmgr_set_progress_callback(wubu_pkgmgr_progress_cb cb, void *userdata) {
    g_pkgmgr.progress_cb = cb;
    g_pkgmgr.progress_userdata = userdata;
}

