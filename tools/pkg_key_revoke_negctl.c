/*
 * pkg_key_revoke_negctl.c -- test-only negative control harness.
 *
 * NOT part of the operator CLI.  It performs the VERBATIM pre-change
 * revocation call (wubu_pkgmgr_key_revoke with no signature and no auth
 * check).  If this still writes the DB while the CLI refuses the same
 * unsigned command, the refusal is caused by the new authorization gate
 * and not by a broken DB path.  The harness is compiled only by the opt-in
 * pkg_key_revoke gate; no key material is stored in the repository.
 */
#include "wubu_pkgmgr.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[1], "legacy_direct") != 0) {
        fprintf(stderr, "usage: %s legacy_direct <dbdir> <key_hex>\n", argv[0]);
        return 2;
    }
    const char *dbdir = argv[2], *key = argv[3];
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
    if (!wubu_pkgmgr_init(&cfg)) {
        fprintf(stderr, "negctl: pkgmgr init failed\n");
        return 1;
    }
    /* Pre-change behaviour: direct DB write, no signature, no gate. */
    bool ok = wubu_pkgmgr_key_revoke(key);
    printf("%s\n", ok ? "LEGACY_OK" : "LEGACY_FAIL");
    wubu_pkgmgr_shutdown();
    return ok ? 0 : 1;
}
