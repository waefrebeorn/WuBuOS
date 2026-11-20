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
#include "wubu_pkgmgr_revoke_sync.h"
#include "wubu_pkg_sign.h"
#include "wubu_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_POS 16

static void usage(const char *name) {
    fprintf(stderr,
        "usage: %s [--db DIR] bootstrap <admin_pub_hex>\n"
        "       %s [--db DIR] revoke <key_hex> --sig <sig_hex>\n"
        "       %s [--db DIR] unrevoke <key_hex> --sig <sig_hex>\n"
        "       %s [--db DIR] list\n"
        "       %s [--db DIR] rotate <new_admin_pub_hex> --sig <sig_hex>\n"
        "       %s [--db DIR] set-expiry <key_hex> <unix_epoch> --sig <sig_hex>\n"
        "       %s [--db DIR] quorum-config <M> <key_hex>... --sig <sig_hex>\n"
        "       %s [--db DIR] quorum-status\n"
        "       %s [--db DIR] feed-quorum <feed_file> --sigs <s1,s2,...>\n"
        "       %s [--db DIR] escrow-config <M> <key_hex>... --sig <sig_hex>\n"
        "       %s [--db DIR] escrow-status\n"
        "       %s [--db DIR] expiry-status <key_hex>\n"
        "       %s [--db DIR] recover <new_admin_hex> --sigs <s1,s2,...>\n"
        "       %s sign <revoke|unrevoke> <key_hex> --seed <seed_hex>\n"
        "       %s sign-feed <feed_file> --seed <seed_hex>\n"
        "       %s sign-rotate <new_admin_pub_hex> --seed <seed_hex>\n"
        "       %s sign-expiry <key_hex> <unix_epoch> --seed <seed_hex>\n"
        "       %s sign-quorum <M> <key_hex>... --seed <seed_hex>\n"
        "       %s sign-escrow <M> <key_hex>... --seed <seed_hex>\n"
        "       %s sign-recover <new_admin_pub_hex> --seed <seed_hex>\n"
        "       %s [--db DIR] feed <feed_file> --sig <sig_hex>\n"
        "       %s [--db DIR] sync <url> [--trusted <url[,url...]>]\n"
        "       %s [--db DIR] set-sync-url <url[,url...]>\n"
        "       %s [--db DIR] get-sync-url\n"
        "       %s sign <revoke|unrevoke> <key_hex> --seed <seed_hex>\n",
        name, name, name, name, name, name, name, name, name, name,
        name, name, name, name, name, name, name, name, name, name,
        name, name, name, name, name, name);
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
             strcmp(argv[i], "--sigs") == 0 || strcmp(argv[i], "--seed") == 0 ||
             strcmp(argv[i], "--trusted") == 0) &&
            i + 1 < argc) {
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

/* Read the EXACT feed bytes, bounded to the importer's limit. */
static uint8_t *read_feed_bytes(const char *path, size_t *out_len) {
    *out_len = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0 || (size_t)n > WUBU_PKGMGR_FEED_MAX_BYTES) { fclose(f); return NULL; }
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)n ? (size_t)n : 1);
    if (!buf) { fclose(f); return NULL; }
    if (n && fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
    fclose(f);
    *out_len = (size_t)n;
    return buf;
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
    const char *trusted = opt_value(argc, argv, "--trusted");

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

    /* sign-feed needs no DB: hash the exact feed bytes and use the leaf. */
    if (strcmp(cmd, "sign-feed") == 0) {
        if (np < 2 || !seed) { usage(argv[0]); return 2; }
        size_t len = 0;
        uint8_t *data = read_feed_bytes(pos[1], &len);
        if (!data) { fprintf(stderr, "ctl: cannot read feed %s\n", pos[1]); return 2; }
        uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN];
        char err[256], hex[2 * WUBU_PKG_SIGNATURE_LEN + 1];
        bool ok = wubu_pkg_sign_bytes(data, len, WUBU_PKG_REVOCATION_FEED_DOMAIN,
                                      seed, out_sig, err, sizeof(err));
        free(data);
        if (!ok) { fprintf(stderr, "ctl: sign-feed failed: %s\n", err); return 1; }
        bytes_to_hex(out_sig, WUBU_PKG_SIGNATURE_LEN, hex);
        printf("%s\n", hex);
        return 0;
    }

    /* sign-rotate: sign the admin-rotation command with a seed (no DB). */
    if (strcmp(cmd, "sign-rotate") == 0) {
        if (np < 2 || !seed) { usage(argv[0]); return 2; }
        char command[256];
        if (!wubu_pkgmgr_key_rotate_command_string(pos[1], command, sizeof(command))) {
            fprintf(stderr, "ctl: cannot build rotate command\n");
            return 2;
        }
        uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN];
        char err[256], hex[2 * WUBU_PKG_SIGNATURE_LEN + 1];
        if (!wubu_pkg_sign_command(command, seed, out_sig, err, sizeof(err))) {
            fprintf(stderr, "ctl: sign-rotate failed: %s\n", err);
            return 1;
        }
        bytes_to_hex(out_sig, WUBU_PKG_SIGNATURE_LEN, hex);
        printf("%s\n", hex);
        return 0;
    }

    /* sign-quorum: sign an M-of-N configuration command with a seed. */
    if (strcmp(cmd, "sign-quorum") == 0) {
        if (np < 3 || !seed) { usage(argv[0]); return 2; }
        int m = atoi(pos[1]);
        int n = np - 2;
        const char *keys[MAX_POS];
        for (int i = 0; i < n; i++) keys[i] = pos[2 + i];
        char command[1024];
        if (!wubu_pkgmgr_key_quorum_command_string(m, n, keys, command, sizeof(command))) {
            fprintf(stderr, "ctl: cannot build quorum command\n");
            return 2;
        }
        uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN];
        char err[256], hex[2 * WUBU_PKG_SIGNATURE_LEN + 1];
        if (!wubu_pkg_sign_command(command, seed, out_sig, err, sizeof(err))) {
            fprintf(stderr, "ctl: sign-quorum failed: %s\n", err);
            return 1;
        }
        bytes_to_hex(out_sig, WUBU_PKG_SIGNATURE_LEN, hex);
        printf("%s\n", hex);
        return 0;
    }

    /* sign-expiry: domain-separated admin command for an expiry change. */
    if (strcmp(cmd, "sign-expiry") == 0) {
        if (np < 3 || !seed) { usage(argv[0]); return 2; }
        int64_t exp = strtoll(pos[2], NULL, 10);
        char command[256];
        if (!wubu_pkgmgr_anchor_expiry_command_string(pos[1], exp,
                                                      command, sizeof(command))) {
            fprintf(stderr, "ctl: cannot build expiry command\n");
            return 2;
        }
        uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN];
        char err[256], hex[2 * WUBU_PKG_SIGNATURE_LEN + 1];
        if (!wubu_pkg_sign_command(command, seed, out_sig, err, sizeof(err))) {
            fprintf(stderr, "ctl: sign-expiry failed: %s\n", err);
            return 1;
        }
        bytes_to_hex(out_sig, WUBU_PKG_SIGNATURE_LEN, hex);
        printf("%s\n", hex);
        return 0;
    }

    /* sign-escrow: sign an escrow configuration command with a seed. */
    if (strcmp(cmd, "sign-escrow") == 0) {
        if (np < 4 || !seed) { usage(argv[0]); return 2; }
        int m = atoi(pos[1]);
        int n = np - 2;
        const char *keys[MAX_POS];
        for (int i = 0; i < n; i++) keys[i] = pos[2 + i];
        char command[1024];
        if (!wubu_pkgmgr_escrow_command_string(m, n, keys, command, sizeof(command))) {
            fprintf(stderr, "ctl: cannot build escrow command\n");
            return 2;
        }
        uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN];
        char err[256], hex[2 * WUBU_PKG_SIGNATURE_LEN + 1];
        if (!wubu_pkg_sign_command(command, seed, out_sig, err, sizeof(err))) {
            fprintf(stderr, "ctl: sign-escrow failed: %s\n", err);
            return 1;
        }
        bytes_to_hex(out_sig, WUBU_PKG_SIGNATURE_LEN, hex);
        printf("%s\n", hex);
        return 0;
    }

    /* sign-recover: sign a lost-admin recovery command with an escrow seed. */
    if (strcmp(cmd, "sign-recover") == 0) {
        if (np < 2 || !seed) { usage(argv[0]); return 2; }
        char command[256];
        if (!wubu_pkgmgr_escrow_recovery_command_string(pos[1], command, sizeof(command))) {
            fprintf(stderr, "ctl: cannot build recovery command\n");
            return 2;
        }
        uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN];
        char err[256], hex[2 * WUBU_PKG_SIGNATURE_LEN + 1];
        if (!wubu_pkg_sign_command(command, seed, out_sig, err, sizeof(err))) {
            fprintf(stderr, "ctl: sign-recover failed: %s\n", err);
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
    } else if (strcmp(cmd, "feed") == 0 && np >= 2) {
        size_t applied = 0;
        char err[256];
        bool ok = wubu_pkgmgr_key_feed_import(pos[1], sig, &applied, err, sizeof(err));
        if (!ok && err[0]) fprintf(stderr, "ctl: feed: %s\n", err);
        printf("%s feed %s applied %zu\n", ok ? "OK" : "REFUSED", pos[1], applied);
        rc = ok ? 0 : 1;
    } else if (strcmp(cmd, "sync") == 0 && np >= 2) {
        /* GUI settings store is a first-class trusted-URL read path. */
        wubu_settings_init();
        wubu_pkg_sync_config_t sync_cfg;
        char cfgpath[600];
        size_t applied = 0;
        char err[256];
        bool ok;
        wubu_pkg_sync_config_init(&sync_cfg);
        snprintf(cfgpath, sizeof(cfgpath), "%s/sync.conf", db);
        if (wubu_pkg_sync_config_load(cfgpath, trusted, &sync_cfg) < 0) {
            fprintf(stderr, "ctl: sync: malformed trusted sync config\n");
            printf("REFUSED sync %s\n", pos[1]);
            rc = 1;
        } else {
            ok = wubu_pkgmgr_key_feed_sync(pos[1], &sync_cfg,
                                           &applied, err, sizeof(err));
            if (!ok && err[0]) fprintf(stderr, "ctl: sync: %s\n", err);
            printf("%s sync %s applied %zu\n",
                   ok ? "OK" : "REFUSED", pos[1], applied);
            rc = ok ? 0 : 1;
        }
    } else if (strcmp(cmd, "rotate") == 0 && np >= 2) {
        bool ok = wubu_pkgmgr_key_admin_rotate(pos[1], sig);
        printf("%s rotate %s\n", ok ? "OK" : "REFUSED", pos[1]);
        rc = ok ? 0 : 1;
    } else if (strcmp(cmd, "set-expiry") == 0 && np >= 3) {
        int64_t exp = strtoll(pos[2], NULL, 10);
        bool ok = wubu_pkgmgr_anchor_set_expiry_authorized(pos[1], exp, sig);
        printf("%s set-expiry %s %lld\n", ok ? "OK" : "REFUSED", pos[1],
               (long long)exp);
        rc = ok ? 0 : 1;
    } else if (strcmp(cmd, "quorum-config") == 0 && np >= 2) {
        int m = atoi(pos[1]);
        int n = np - 2;
        const char *keys[MAX_POS];
        for (int i = 0; i < n; i++) keys[i] = pos[2 + i];
        bool ok = wubu_pkgmgr_key_quorum_configure(m, n, keys, sig);
        printf("%s quorum-config %d-of-%d\n", ok ? "OK" : "REFUSED", m, n);
        rc = ok ? 0 : 1;
    } else if (strcmp(cmd, "quorum-status") == 0) {
        int m = 0, n = 0;
        bool set = wubu_pkgmgr_key_quorum_status(&m, &n);
        printf("QUORUM %d %d %s\n", m, n, set ? "SET" : "UNSET");
    } else if (strcmp(cmd, "feed-quorum") == 0 && np >= 2) {
        const char *sigs[MAX_POS];
        size_t ns = 0;
        const char *raw = opt_value(argc, argv, "--sigs");
        char buf[4096];
        if (raw && *raw && strlen(raw) < sizeof(buf)) {
            snprintf(buf, sizeof(buf), "%s", raw);
            char *tok = strtok(buf, ",");
            while (tok && ns < MAX_POS) { sigs[ns++] = tok; tok = strtok(NULL, ","); }
        }
        size_t applied = 0;
        char err[256];
        bool ok = wubu_pkgmgr_key_feed_import_quorum(pos[1], sigs, ns,
                                                    &applied, err, sizeof(err));
        if (!ok && err[0]) fprintf(stderr, "ctl: feed-quorum: %s\n", err);
        printf("%s feed-quorum %s applied %zu\n", ok ? "OK" : "REFUSED",
               pos[1], applied);
        rc = ok ? 0 : 1;
    } else if (strcmp(cmd, "escrow-config") == 0 && np >= 3) {
        int m = atoi(pos[1]);
        int n = np - 2;
        const char *keys[MAX_POS];
        for (int i = 0; i < n; i++) keys[i] = pos[2 + i];
        bool ok = wubu_pkgmgr_escrow_configure(m, n, keys, sig);
        printf("%s escrow-config %d-of-%d\n", ok ? "OK" : "REFUSED", m, n);
        rc = ok ? 0 : 1;
    } else if (strcmp(cmd, "escrow-status") == 0) {
        int m = 0, n = 0;
        bool set = wubu_pkgmgr_escrow_status(&m, &n);
        printf("ESCROW %d %d %s\n", m, n, set ? "SET" : "UNSET");
    } else if (strcmp(cmd, "recover") == 0 && np >= 2) {
        const char *sigs[MAX_POS];
        size_t ns = 0;
        const char *raw = opt_value(argc, argv, "--sigs");
        char buf[4096];
        if (raw && *raw && strlen(raw) < sizeof(buf)) {
            snprintf(buf, sizeof(buf), "%s", raw);
            char *tok = strtok(buf, ",");
            while (tok && ns < MAX_POS) { sigs[ns++] = tok; tok = strtok(NULL, ","); }
        }
        bool ok = wubu_pkgmgr_key_admin_recover(pos[1], sigs, ns);
        printf("%s recover %s\n", ok ? "OK" : "REFUSED", pos[1]);
        rc = ok ? 0 : 1;
    } else if (strcmp(cmd, "expiry-status") == 0 && np >= 2) {
        int64_t exp = 0;
        if (wubu_pkgmgr_anchor_expiry(pos[1], &exp))
            printf("EXPIRY %s %lld\n", pos[1], (long long)exp);
        else
            printf("NOEXPIRY %s\n", pos[1]);
    } else if (strcmp(cmd, "set-sync-url") == 0 && np >= 2) {
        wubu_settings_init();
        WubuSettings *s = wubu_settings_mut();
        if (strlen(pos[1]) >= sizeof(s->trusted_sync_url)) {
            fprintf(stderr, "ctl: set-sync-url: value too long\n");
            rc = 1;
        } else {
            snprintf(s->trusted_sync_url, sizeof(s->trusted_sync_url), "%s", pos[1]);
            wubu_settings_save();
            printf("OK set-sync-url %s\n", s->trusted_sync_url);
        }
    } else if (strcmp(cmd, "get-sync-url") == 0) {
        wubu_settings_init();
        printf("SYNCURL %s\n", wubu_settings_get()->trusted_sync_url);
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
