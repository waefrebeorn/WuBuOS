/*
 * wubu_pkgmgr_txn.c  --  WuBuOS Package Manager: Txn
 */

#include "wubu_pkgmgr_internal.h"

#include <glob.h>
#include <unistd.h>
#include <sys/wait.h>

bool wubu_pkgmgr_txn_begin(wubu_pkg_transaction_t* txn, bool dry_run) {
    memset(txn, 0, sizeof(*txn));
    txn->dry_run = dry_run;
    time_t now = time(NULL);
    struct tm* tm_now = gmtime(&now);
    strftime(txn->rollback_dir, sizeof(txn->rollback_dir), "/tmp/wubu_rollback_%Y%m%d_%H%M%S", tm_now);
    ensure_dir(txn->rollback_dir);
    return true;
}

bool wubu_pkgmgr_txn_add(wubu_pkg_transaction_t* txn, wubu_pkg_txn_type_t type,
                          const char* pkg_id, const char* old_ver, const char* new_ver,
                          bool is_dep) {
    if (txn->n_items >= 128) return false;
    wubu_pkg_txn_item_t* item = &txn->items[txn->n_items++];
    item->type = type;
    strncpy(item->pkg_id, pkg_id, sizeof(item->pkg_id)-1);
    if (old_ver) strncpy(item->old_version, old_ver, sizeof(item->old_version)-1);
    if (new_ver) strncpy(item->new_version, new_ver, sizeof(item->new_version)-1);
    item->is_dependency = is_dep;
    return true;
}

bool wubu_pkgmgr_txn_commit(wubu_pkg_transaction_t* txn) {
    for (int i = 0; i < txn->n_items; i++) {
        wubu_pkg_txn_item_t* item = &txn->items[i];
        switch (item->type) {
            case WUBU_PKG_TXN_INSTALL:
                wubu_pkgmgr_install(item->pkg_id, txn->dry_run);
                break;
            case WUBU_PKG_TXN_REMOVE:
                wubu_pkgmgr_remove(item->pkg_id, false);
                break;
            case WUBU_PKG_TXN_UPGRADE:
                wubu_pkgmgr_upgrade(item->pkg_id, txn->dry_run);
                break;
            case WUBU_PKG_TXN_DOWNGRADE:
                /* Install specific version */
                break;
        }
    }
    return true;
}

bool wubu_pkgmgr_txn_rollback(wubu_pkg_transaction_t* txn) {
    /* Restore from rollback_dir */
    return true;
}

/* ============================================================
 * App Integration
 * ============================================================ */

bool wubu_pkgmgr_register_desktop(const wubu_pkg_installed_t* pkg) {
    for (int i = 0; i < pkg->manifest.n_entrypoints; i++) {
        generate_desktop_entry(&pkg->manifest, i, pkg->install_path);
    }
    return true;
}

bool wubu_pkgmgr_unregister_desktop(const char* pkg_id) {
    char desktop_pattern[512];
    snprintf(desktop_pattern, sizeof(desktop_pattern), "%s/share/applications/%s-*.desktop",
             g_pkgmgr.config.install_prefix, pkg_id);
    
    /* Use glob to find and remove matching files */
    glob_t glob_result;
    if (glob(desktop_pattern, 0, NULL, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            unlink(glob_result.gl_pathv[i]);
        }
        globfree(&glob_result);
    }
    return true;
}

bool wubu_pkgmgr_generate_desktop_files(const wubu_pkg_installed_t* pkg) {
    return wubu_pkgmgr_register_desktop(pkg);
}

