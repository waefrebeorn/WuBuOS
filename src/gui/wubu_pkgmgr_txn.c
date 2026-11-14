/*
 * wubu_pkgmgr_txn.c  --  WuBuOS Package Manager: Txn
 */

#include "../egojar.h"
#include "wubu_pkgmgr_internal.h"

#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   /* mkdtemp */

bool wubu_pkgmgr_txn_begin(wubu_pkg_transaction_t* txn, bool dry_run) {
    memset(txn, 0, sizeof(*txn));
    txn->dry_run = dry_run;

    /* mkdtemp, not a timestamped name.  strftime("%Y%m%d_%H%M%S") has SECOND
     * granularity, so two transactions begun in the same second got the SAME
     * rollback directory -- the same collision class as the guest store's fixed
     * ".tmp" path (fixed in de052ff): the second begin silently shared whatever the
     * first had put there, and a rollback would restore the wrong process's debris.
     *
     * mkdtemp creates the directory atomically and guarantees uniqueness, so no
     * locking and no clock are involved. */
    char tmpl[] = "/tmp/wubu_rollback_XXXXXX";
    if (mkdtemp(tmpl) == NULL) {
        txn->rollback_dir[0] = '\0';
        return false;
    }
    snprintf(txn->rollback_dir, sizeof(txn->rollback_dir), "%s", tmpl);
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
    if (!txn) return false;

    for (int i = 0; i < txn->n_items; i++) {
        wubu_pkg_txn_item_t* item = &txn->items[i];
        bool ok = false;

        switch (item->type) {
            case WUBU_PKG_TXN_INSTALL:
                ok = wubu_pkgmgr_install(item->pkg_id, txn->dry_run);
                break;
            case WUBU_PKG_TXN_REMOVE:
                ok = wubu_pkgmgr_remove(item->pkg_id, false);
                break;
            case WUBU_PKG_TXN_UPGRADE:
                ok = wubu_pkgmgr_upgrade(item->pkg_id, txn->dry_run);
                break;
            case WUBU_PKG_TXN_DOWNGRADE:
                /* Not implemented.  Reporting failure rather than skipping silently:
                 * a caller that asked for a downgrade and got `true` would believe it
                 * happened while the package stayed at its current version. */
                ok = false;
                break;
        }

        /* Fail CLOSED on the first failed item.  This used to ignore every return
         * value and return true unconditionally, so a transaction whose every item
         * failed still reported success -- and because rollback() was also a no-op,
         * there was nothing behind the success.  Stopping here at least makes the
         * damage visible; full undo needs the backup plumbing from audit item 3. */
        if (!ok) return false;
    }
    return true;
}

bool wubu_pkgmgr_txn_rollback(wubu_pkg_transaction_t* txn) {
    if (!txn) return false;

    /* FAIL CLOSED.  This function has never restored anything: begin() creates the
     * rollback directory, but neither install_package_files() (which copies straight
     * into the install root) nor wubu_pkgmgr_remove() (which runs rm -rf) ever writes
     * a backup into it.  There is no state to restore.
     *
     * It used to `return true`, and that was worse than doing nothing: a caller that
     * believes the rollback SUCCEEDED will carry on as though the system were clean,
     * while partial files remain installed or removed.  An explicit false lets the
     * caller surface the failure instead of hiding behind it.
     *
     * Making this real needs the backup plumbing first -- see item 3 in
     * docs/BMDS_GAP_ANALYSIS.md -- at which point this should walk a recorded list of
     * saved originals rather than reporting an empty directory as success. */
    return false;
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

