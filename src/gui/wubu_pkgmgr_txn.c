/*
 * wubu_pkgmgr_txn.c  --  WuBuOS Package Manager: Txn
 */

#include "../egojar.h"
#include "wubu_pkgmgr_internal.h"

#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* mkdtemp */
#include <sys/stat.h>
#include <dirent.h>

/* ============================================================
 * Backup / restore primitives
 *
 * A transaction item is undone by restoring the state captured BEFORE it ran.
 * Each completed item stores, under <rollback_dir>/item_<i>/:
 *   state.db      -- consistent whole-DB snapshot (SQLite backup API)
 *   install_root  -- the exact path that was mutated
 *   tree/         -- copy of that path's pre-mutation directory tree (if any)
 *   desktop/      -- copy of the pkg's generated .desktop files (if any)
 * Replaying the items in REVERSE and restoring each pre-state leaves the DB
 * and filesystem exactly as they were before the first item, even when later
 * items touched the same package.
 * ============================================================ */

/* Recursive rm -rf without system()/fork (paths come from our own rollback
 * records, but recursion keeps it self-contained and signal-safe enough). */
static void txn_remove_tree(const char *path) {
    if (!path || !path[0]) return;
    struct stat st;
    if (stat(path, &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *e;
            char child[1024];
            while ((e = readdir(d)) != NULL) {
                if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
                snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
                txn_remove_tree(child);
            }
            closedir(d);
        }
        rmdir(path);
    } else {
        unlink(path);
    }
}

/* Binary-safe file copy (the internal copy_file() helper is text-oriented). */
static bool txn_copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return false;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return false; }
    char buf[8192];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
    }
    if (ferror(in)) ok = false;
    fclose(in);
    if (fclose(out) != 0) ok = false;
    struct stat st;
    if (ok && stat(src, &st) == 0) chmod(dst, st.st_mode & 0777);
    return ok;
}

static bool txn_copy_tree(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) != 0 || !S_ISDIR(st.st_mode)) return false;
    if (!ensure_dir(dst)) return false;
    DIR *d = opendir(src);
    if (!d) return false;
    bool ok = true;
    struct dirent *e;
    char s[1024], t[1024];
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(s, sizeof(s), "%s/%s", src, e->d_name);
        snprintf(t, sizeof(t), "%s/%s", dst, e->d_name);
        struct stat cs;
        if (stat(s, &cs) != 0) continue;
        if (S_ISDIR(cs.st_mode)) {
            if (!txn_copy_tree(s, t)) ok = false;
        } else if (S_ISREG(cs.st_mode)) {
            if (!txn_copy_file(s, t)) ok = false;
        }
    }
    closedir(d);
    return ok;
}

/* Consistent whole-DB copy.  sqlite3_backup is used instead of copying the
 * file with read()/write(): the live connection may have un-flushed pages. */
static bool txn_db_snapshot(const char *path) {
    sqlite3 *dst = NULL;
    if (sqlite3_open(path, &dst) != SQLITE_OK) {
        if (dst) sqlite3_close(dst);
        return false;
    }
    sqlite3_backup *bp = sqlite3_backup_init(dst, "main", g_pkgmgr.db, "main");
    bool ok = false;
    if (bp) {
        ok = (sqlite3_backup_step(bp, -1) == SQLITE_DONE);
        ok = (sqlite3_backup_finish(bp) == SQLITE_OK) && ok;
    }
    sqlite3_close(dst);
    return ok;
}

static bool txn_db_restore(const char *path) {
    sqlite3 *src = NULL;
    if (sqlite3_open_v2(path, &src, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (src) sqlite3_close(src);
        return false;
    }
    sqlite3_backup *bp = sqlite3_backup_init(g_pkgmgr.db, "main", src, "main");
    bool ok = false;
    if (bp) {
        ok = (sqlite3_backup_step(bp, -1) == SQLITE_DONE);
        ok = (sqlite3_backup_finish(bp) == SQLITE_OK) && ok;
    }
    sqlite3_close(src);
    return ok;
}

static void txn_desktop_glob(char *out, size_t n, const char *id) {
    snprintf(out, n, "%s/share/applications/%s-*.desktop",
             g_pkgmgr.config.install_prefix, id);
}

static void txn_snapshot_desktop(const char *id, const char *destdir) {
    if (!pkgmgr_valid_id(id)) return;
    char pat[768];
    txn_desktop_glob(pat, sizeof(pat), id);
    glob_t g;
    if (glob(pat, 0, NULL, &g) != 0) return;
    ensure_dir(destdir);
    for (size_t i = 0; i < g.gl_pathc; i++) {
        const char *base = strrchr(g.gl_pathv[i], '/');
        base = base ? base + 1 : g.gl_pathv[i];
        char dst[1024];
        snprintf(dst, sizeof(dst), "%s/%s", destdir, base);
        txn_copy_file(g.gl_pathv[i], dst);
    }
    globfree(&g);
}

static void txn_restore_desktop(const char *id, const char *srcdir) {
    if (!pkgmgr_valid_id(id)) return;
    char appdir[640], pat[768];
    snprintf(appdir, sizeof(appdir), "%s/share/applications",
             g_pkgmgr.config.install_prefix);
    txn_desktop_glob(pat, sizeof(pat), id);

    /* Remove whatever the transaction left behind, then put back the saved set. */
    glob_t g;
    if (glob(pat, 0, NULL, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; i++) unlink(g.gl_pathv[i]);
        globfree(&g);
    }
    DIR *d = opendir(srcdir);
    if (!d) return;
    ensure_dir(appdir);
    struct dirent *e;
    char s[1024], t[1024];
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(s, sizeof(s), "%s/%s", srcdir, e->d_name);
        snprintf(t, sizeof(t), "%s/%s", appdir, e->d_name);
        txn_copy_file(s, t);
    }
    closedir(d);
}

static bool txn_read_text(const char *path, char *out, size_t n) {
    if (out) out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return false;
    if (!fgets(out, (int)n, f)) { fclose(f); return false; }
    fclose(f);
    out[n - 1] = '\0';
    return true;
}

/* The one backup hook the install/remove paths call.  Called with the TRUE
 * id and the exact install root, BEFORE the item mutates either one. */
void pkgmgr_backup_capture(const char *pkg_id, const char *install_root) {
    wubu_pkg_transaction_t *txn = g_pkgmgr.active_txn;
    if (!txn || !pkg_id || !*pkg_id) return;
    int idx = g_pkgmgr.active_item;
    if (idx < 0 || idx >= txn->n_items) return;
    wubu_pkg_txn_item_t *item = &txn->items[idx];
    if (item->backup_taken) return;  /* upgrade: first snapshot wins */

    char itemdir[640];
    snprintf(itemdir, sizeof(itemdir), "%s/item_%d", txn->rollback_dir, idx);
    if (!ensure_dir(itemdir)) return;

    snprintf(item->applied_id, sizeof(item->applied_id), "%s", pkg_id);
    item->applied_id[sizeof(item->applied_id) - 1] = '\0';

    char rootfile[768], dbfile[768], tree[768], desk[768];
    snprintf(rootfile, sizeof(rootfile), "%s/install_root", itemdir);
    snprintf(dbfile, sizeof(dbfile), "%s/state.db", itemdir);
    snprintf(tree, sizeof(tree), "%s/tree", itemdir);
    snprintf(desk, sizeof(desk), "%s/desktop", itemdir);

    write_file(rootfile, install_root ? install_root : "");
    bool dbok = txn_db_snapshot(dbfile);

    bool treeok = true;
    struct stat st;
    if (install_root && install_root[0] &&
        stat(install_root, &st) == 0 && S_ISDIR(st.st_mode))
        treeok = txn_copy_tree(install_root, tree);

    txn_snapshot_desktop(item->applied_id, desk);

    /* Fail closed: an item we could not snapshot is an item we cannot undo. */
    if (dbok && treeok) item->backup_taken = true;
    else fprintf(stderr, "[pkgmgr] txn backup failed for '%s' (db=%d tree=%d)\n",
                 pkg_id, dbok, treeok);
}

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
    memset(item, 0, sizeof(*item));
    item->type = type;
    strncpy(item->pkg_id, pkg_id, sizeof(item->pkg_id)-1);
    if (old_ver) strncpy(item->old_version, old_ver, sizeof(item->old_version)-1);
    if (new_ver) strncpy(item->new_version, new_ver, sizeof(item->new_version)-1);
    item->is_dependency = is_dep;
    return true;
}

bool wubu_pkgmgr_txn_commit(wubu_pkg_transaction_t* txn) {
    if (!txn) return false;

    /* Publish the txn so the install/remove/upgrade paths snapshot through
     * pkgmgr_backup_capture().  Cleared on every exit path. */
    g_pkgmgr.active_txn = txn;
    g_pkgmgr.active_item = -1;
    bool all_ok = true;

    for (int i = 0; i < txn->n_items; i++) {
        wubu_pkg_txn_item_t* item = &txn->items[i];
        g_pkgmgr.active_item = i;
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

        /* Fail CLOSED on the first failed item.  Items already applied stay
         * backed up, so the caller can call rollback() and have them undone. */
        if (!ok) { all_ok = false; break; }
    }

    g_pkgmgr.active_txn = NULL;
    g_pkgmgr.active_item = -1;
    return all_ok;
}

bool wubu_pkgmgr_txn_rollback(wubu_pkg_transaction_t* txn) {
    if (!txn || !txn->rollback_dir[0]) return false;

    int restored = 0;
    bool all_ok = true;

    /* Reverse order: the last applied item is undone first.  Because every
     * snapshot is the WHOLE DB taken before that item, restoring them in this
     * order walks the DB back to its exact pre-transaction state. */
    for (int i = txn->n_items - 1; i >= 0; i--) {
        wubu_pkg_txn_item_t* item = &txn->items[i];
        if (!item->backup_taken) continue;

        char itemdir[640], dbfile[768], rootfile[768], tree[768], desk[768];
        snprintf(itemdir, sizeof(itemdir), "%s/item_%d", txn->rollback_dir, i);
        snprintf(dbfile, sizeof(dbfile), "%s/state.db", itemdir);
        snprintf(rootfile, sizeof(rootfile), "%s/install_root", itemdir);
        snprintf(tree, sizeof(tree), "%s/tree", itemdir);
        snprintf(desk, sizeof(desk), "%s/desktop", itemdir);

        bool dbok = txn_db_restore(dbfile);

        char root[1024];
        txn_read_text(rootfile, root, sizeof(root));
        if (root[0]) {
            /* Drop the files this item left behind, then reinstate its
             * pre-mutation tree (empty/absent before install => stays gone). */
            txn_remove_tree(root);
            struct stat st;
            if (stat(tree, &st) == 0 && S_ISDIR(st.st_mode))
                txn_copy_tree(tree, root);
        }
        txn_restore_desktop(item->applied_id, desk);

        if (!dbok) {
            fprintf(stderr, "[pkgmgr] txn rollback: DB restore failed for item %d\n", i);
            all_ok = false;
        }
        item->backup_taken = false;   /* idempotent: no second restore */
        restored++;
    }

    return restored > 0 && all_ok;
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
    /* pkg_id also becomes a glob pattern below. A wildcard-bearing id
     * would delete other packages' desktop files, so use the one charset
     * validator the install path already owns. */
    if (!pkgmgr_valid_id(pkg_id)) return false;
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

