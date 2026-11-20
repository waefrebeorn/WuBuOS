/*
 * wubu_pkg_key_ctl.c -- thin operator CLI for Ed25519 trust-anchor rotation.
 *
 * This file owns NO crypto and NO revocation policy.  It parses arguments and
 * calls the facade API in wubu_pkgmgr.c; signatures are produced/verified by
 * wubu_pkg_ed25519.c.  Every command is a fresh process, so a signed revoke
 * that survives into the next `list` invocation is proof of DB persistence.
 *
 * Commands:
 *   bootstrap <admin_pub_hex>            one-time admin-key recording
 *   revoke <key_hex> [--sig SIG_HEX]     authorized revoke (needs --sig)
 *   unrevoke <key_hex> [--sig SIG_HEX]   authorized unrevoke (needs --sig)
 *   list                                 enumerate revoked keys (no auth)
 *   sign <revoke|unrevoke> <key_hex> --seed <seed_hex>
 *                                        print the command signature hex
 *
 * --db DIR selects the pkgmgr DB directory (or set WUBU_PKGMGR_DB).
 */
#include "wubu_pkgmgr.h"
#include "wubu_pkg_sign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_POS 8

static void usage(const char *name) {
    fprintf(stderr,
        "usage: %s [--db DIR] bootstrap <admin_pub_hex>\n"
        "       %s [--db DIR] revoke <key_hex> --sig <sig_hex>\n"
        "       %s [--db DIR] unrevoke <key_hex> --sig <sig_hex>\n"
        "       %s [--db DIR] list\n"
        "       %s sign <revoke|unrevoke> <key_hex> --seed <seed_hex>\n",
        name, name, name, name, name);
}

static const char *opt_value(int argc, char **argv, const char *flag) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return argv[i + 1];
    return NULL;
}

/* Collect non-option tokens into pos[]. --db/--sig/--seed consume their value. */
static int collect_pos(int argc, char **argv, char *pos[], int max) {
    int n = 0;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--db") == 0 || strcmp(argv[i], "--sig") == 0 ||
             strcmp(argv[i], "--seed") == 0) && i + 1 < argc) {
            i++;
            continue;
        }
        if (n < max) pos[n++] = argv[i];
    }
    return n;
}

static void bytes_to_hex(const uint8_t *b, size_t n, char *out) {
    static const char d[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[2 * i]     = d[b[i] >> 4];
        out[2 * i + 1] = d[b[i] & 0x0f];
    }
    out[2 * n] = '\0';
}

static bool init_db(const char *dbdir) {
    wubu_pkgmgr_config_t cfg;
    wubu_pkgmgr_get_default_config(&cfg);
    snprintf(cfg.db_path, sizeof(cfg.db_path), "%s", dbdir);
    char p[600];
    snprintf(p, sizeof(p), "%s/cache", dbdir);
    snprintf(cfg.cache_dir, sizeof(cfg.cache_dir), "%s", p);
    snprintf(p, sizeof(p), "%s/prefix", dbdir);
    snprintf(cfg.install_prefix, sizeof(cfg.install_prefix), "%s", p);
    snprintf(p, sizeof(p), "%s/repos", dbdir);
    snprintf(cfg.repo_config_dir, sizeof(cfg.repo_config_dir), "%s", p);
    return wubu_pkgmgr_init(&cfg);
}

int main(int argc, char **argv) {
    char *pos[MAX_POS];
    int np = collect_pos(argc, argv, pos, MAX_POS);
    if (np < 1) { usage(argv[0]); return 2; }
    const char *cmd = pos[0];

    const char *db = opt_value(argc, argv, "--db");
    if (!db) db = getenv("WUBU_PKGMGR_DB");
    if (!db || !*db) db = "pkgdb";
    const char *sig = opt_value(argc, argv, "--sig");
    const char *seed = opt_value(argc, argv, "--seed");

    /* sign needs no DB: it only hashes the command and calls the crypto leaf. */
    if (strcmp(cmd, "sign") == 0) {
        if (np < 3 || !seed) { usage(argv[0]); return 2; }
        char command[256];
        if (!wubu_pkgmgr_key_command_string(pos[1], pos[2],
                                            command, sizeof(command))) {
            fprintf(stderr, "ctl: cannot build command (bad op or key)\n");
            return 2;
        }
        uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN];
        char err[256], hex[2 * WUBU_PKG_SIGNATURE_LEN + 1];
        if (!wubu_pkg_sign_command(command, seed, out_sig, err, sizeof(err))) {
            fprintf(stderr, "ctl: sign failed: %s\n", err);
            return 1;
        }
        bytes_to_hex(out_sig, WUBU_PKG_SIGNATURE_LEN, hex);
        printf("%s\n", hex);
        return 0;
    }

    if (!init_db(db)) {
        fprintf(stderr, "ctl: cannot open pkgmgr DB under %s\n", db);
        return 1;
    }

    int rc = 0;
    if (strcmp(cmd, "bootstrap") == 0 && np >= 2) {
        bool ok = wubu_pkgmgr_key_admin_bootstrap(pos[1]);
        printf("%s bootstrap %s\n", ok ? "OK" : "REFUSED", pos[1]);
        rc = ok ? 0 : 1;
    } else if (strcmp(cmd, "revoke") == 0 && np >= 2) {
        bool ok = wubu_pkgmgr_key_revoke_authorized(pos[1], sig);
        printf("%s revoke %s\n", ok ? "OK" : "REFUSED", pos[1]);
        rc = ok ? 0 : 1;
    } else if (strcmp(cmd, "unrevoke") == 0 && np >= 2) {
        bool ok = wubu_pkgmgr_key_unrevoke_authorized(pos[1], sig);
        printf("%s unrevoke %s\n", ok ? "OK" : "REFUSED", pos[1]);
        rc = ok ? 0 : 1;
    } else if (strcmp(cmd, "list") == 0) {
        char keys[WUBU_PKGMGR_MAX_REVOKED_KEYS][65];
        int n = wubu_pkgmgr_key_revoked_list(keys, WUBU_PKGMGR_MAX_REVOKED_KEYS);
        printf("COUNT %d\n", n);
        for (int i = 0; i < n; i++) printf("%s\n", keys[i]);
        char admin[65];
        if (wubu_pkgmgr_key_admin_pubkey(admin, sizeof(admin)))
            printf("ADMIN %s\n", admin);
    } else {
        usage(argv[0]);
        rc = 2;
    }

    wubu_pkgmgr_shutdown();
    return rc;
}
