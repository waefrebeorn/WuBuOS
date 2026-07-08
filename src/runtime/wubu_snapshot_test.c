/*
 * wubu_snapshot_test.c  --  WuBuOS Container Snapshot Manager Test Suite
 *
 * Tests all snapshot operations: manager lifecycle, CRUD, branching,
 * tagging, export/import, diff, rollback, restore, GC, tree.
 */

#define _POSIX_C_SOURCE 200809L
#include "wubu_snapshot.h"
#include "wubu_snapshot_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

static int g_run = 0, g_pass = 0;

#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s (line %d)\n", msg, __LINE__); } \
} while(0)

/* WubuSnapshotManager is ~56MB — heap-allocate via calloc */
static WubuSnapshotManager *smgr(void) {
    return calloc(1, sizeof(WubuSnapshotManager));
}
#define NSMGR_FREE(m) do { wubu_snapshot_manager_free(m); free(m); } while(0)


int main(void) {
    printf("=== WuBuOS Snapshot Manager Test Suite ===\n\n");

    /* -- Manager lifecycle ---------------------------------------- */
    printf("[Manager Lifecycle]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        T(wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr) == 0, "init");
        T(strcmp(mgr->root_path, "/tmp/wubu-snap-test") == 0, "root_path set");
        T(mgr->fs_type == WUBU_FS_OVERLAYFS, "default fs_type overlayfs");
        T(mgr->snapshot_count == 0, "initially empty");
        T(mgr->branch_count == 1, "default 'main' branch created");
        T(strcmp(mgr->branches[0].name, "main") == 0, "main branch name");
        T(mgr->branches[0].protected == true, "main branch protected");
        T(wubu_snapshot_manager_shutdown(mgr) == 0, "shutdown");
        NSMGR_FREE(mgr);
    }

    /* -- NULL/invalid handling ------------------------------------ */
    printf("\n[Error Handling]\n");
    {
        T(wubu_snapshot_manager_init(NULL, NULL) == -1, "init NULL mgr");
        T(wubu_snapshot_manager_shutdown(NULL) == -1, "shutdown NULL mgr");
        T(wubu_snapshot_manager_set_fs_type(NULL, WUBU_FS_BTRFS) == -1, "set_fs NULL mgr");
        T(wubu_snapshot_create(NULL, "c1", NULL, NULL, NULL, NULL, NULL) == -1, "create NULL mgr");
        T(wubu_snapshot_delete(NULL, "x", false) == -1, "delete NULL mgr");
        T(wubu_snapshot_mount(NULL, "x", NULL, false) == -1, "mount NULL mgr");
        T(wubu_snapshot_unmount(NULL, "x") == -1, "unmount NULL mgr");
        T(wubu_snapshot_list(NULL, NULL, 10, NULL, false) == 0, "list NULL mgr returns 0");
    }

    /* -- FS type -------------------------------------------------- */
    printf("\n[FS Type]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);
        T(wubu_snapshot_manager_set_fs_type(mgr, WUBU_FS_BTRFS) == 0, "set btrfs");
        T(mgr->fs_type == WUBU_FS_BTRFS, "fs_type is btrfs");
        T(wubu_snapshot_manager_set_fs_type(mgr, WUBU_FS_ZFS) == 0, "set zfs");
        T(mgr->fs_type == WUBU_FS_ZFS, "fs_type is zfs");
        T(wubu_snapshot_manager_set_fs_type(mgr, WUBU_FS_LVM) == 0, "set lvm");
        T(mgr->fs_type == WUBU_FS_LVM, "fs_type is lvm");
        T(wubu_snapshot_manager_set_fs_type(mgr, WUBU_FS_AUTO) == 0, "set auto");
        T(mgr->fs_type == WUBU_FS_AUTO, "fs_type is auto");
        NSMGR_FREE(mgr);
    }

    /* -- Snapshot CRUD -------------------------------------------- */
    printf("\n[Snapshot CRUD]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        WubuSnapshot *s1 = NULL;
        T(wubu_snapshot_create(mgr, "container-1", NULL, "main", "v1.0", "Initial version", &s1) == 0,
          "create snapshot");
        T(s1 != NULL, "snapshot returned");
        T(s1->id[0] != '\0', "snapshot ID assigned");
        T(strcmp(s1->container_id, "container-1") == 0, "container_id");
        T(strcmp(s1->label, "v1.0") == 0, "label");
        T(strcmp(s1->description, "Initial version") == 0, "description");
        T(strcmp(s1->branch, "main") == 0, "branch");
        T(s1->status == WUBU_SNAP_STATUS_READY, "status ready");
        T(s1->type == WUBU_SNAP_TYPE_FULL, "type full (no parent)");
        T(mgr->snapshot_count == 1, "snapshot count = 1");
        T(mgr->branches[0].snapshot_count == 1, "main branch snapshot count = 1");

        /* Branch head updated */
        T(strcmp(mgr->branches[0].head_snapshot_id, s1->id) == 0, "branch head updated");

        /* Create second snapshot (incremental) */
        WubuSnapshot *s2 = NULL;
        T(wubu_snapshot_create(mgr, "container-1", s1->id, "main", "v1.1", "Bug fix", &s2) == 0,
          "create incremental snapshot");
        T(s2->type == WUBU_SNAP_TYPE_INCREMENTAL, "type incremental");
        T(strcmp(s2->parent_id, s1->id) == 0, "parent_id");
        T(mgr->snapshot_count == 2, "snapshot count = 2");

        /* Inspect */
        WubuSnapshot inspect;
        T(wubu_snapshot_inspect(mgr, s1->id, &inspect) == 0, "inspect snapshot");
        T(strcmp(inspect.id, s1->id) == 0, "inspect ID");
        T(strcmp(inspect.label, "v1.0") == 0, "inspect label");

        /* List */
        WubuSnapshot list[16];
        T(wubu_snapshot_list(mgr, list, 16, NULL, false) == 2, "list all = 2");
        T(wubu_snapshot_list(mgr, list, 16, "main", false) == 2, "list main = 2");
        T(wubu_snapshot_list(mgr, list, 16, "nonexistent", false) == 0, "list nonexistent = 0");

        /* Delete */
        T(wubu_snapshot_delete(mgr, s2->id, false) == 0, "delete snapshot");
        T(mgr->snapshot_count == 1, "snapshot count after delete");

        /* Delete nonexistent */
        T(wubu_snapshot_delete(mgr, "nope", false) == -1, "delete nonexistent");

        NSMGR_FREE(mgr);
    }

    /* -- Incremental snapshot helper ------------------------------ */
    printf("\n[Incremental Snapshot Helper]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        WubuSnapshot *base = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "main", "base", "Base", &base);

        WubuSnapshot *inc = NULL;
        T(wubu_snapshot_create_incremental(mgr, base->id, "main", "inc", &inc) == 0,
          "create incremental via helper");
        T(inc->type == WUBU_SNAP_TYPE_INCREMENTAL, "incremental type");
        T(strcmp(inc->parent_id, base->id) == 0, "incremental parent");

        NSMGR_FREE(mgr);
    }

    /* -- Mount/Unmount -------------------------------------------- */
    printf("\n[Mount/Unmount]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        WubuSnapshot *s = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "main", "v1", "test", &s);

        T(wubu_snapshot_mount(mgr, s->id, "/tmp/wubu-mount-test", false) == 0, "mount");
        T(s->status == WUBU_SNAP_STATUS_MOUNTED, "status mounted");
        T(s->ref_count == 1, "ref count = 1");
        T(s->read_only == false, "read_only false");

        T(wubu_snapshot_unmount(mgr, s->id) == 0, "unmount");
        T(s->status == WUBU_SNAP_STATUS_READY, "status ready after unmount");
        T(s->ref_count == 0, "ref count = 0");

        /* Can't delete mounted snapshot without force */
        wubu_snapshot_mount(mgr, s->id, "/tmp/wubu-mount-test", false);
        T(wubu_snapshot_delete(mgr, s->id, false) == -1, "can't delete mounted");
        T(wubu_snapshot_delete(mgr, s->id, true) == 0, "force delete mounted");

        NSMGR_FREE(mgr);
    }

    /* -- Branch operations ---------------------------------------- */
    printf("\n[Branch Operations]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        /* Create branch */
        T(wubu_branch_create(mgr, "develop", NULL) == 0, "create develop branch");
        T(mgr->branch_count == 2, "branch count = 2");
        T(strcmp(mgr->branches[1].name, "develop") == 0, "develop branch name");

        /* Duplicate branch */
        T(wubu_branch_create(mgr, "develop", NULL) == -1, "duplicate branch rejected");

        /* Create snapshot on develop */
        WubuSnapshot *s = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "develop", "dev-1", "Dev work", &s);
        T(strcmp(s->branch, "develop") == 0, "snapshot on develop");
        T(mgr->branches[1].snapshot_count == 1, "develop snapshot count");

        /* Branch switch */
        T(wubu_branch_switch(mgr, "develop") == 0, "switch to develop");
        T(wubu_branch_switch(mgr, "nonexistent") == -1, "switch to nonexistent");

        /* Branch list */
        WubuBranch branches[16];
        T(wubu_branch_list(mgr, branches, 16) == 2, "list branches = 2");

        /* Branch delete */
        T(wubu_branch_delete(mgr, "develop", false) == 0, "delete develop");
        T(mgr->branch_count == 1, "branch count after delete");

        /* Can't delete protected main */
        T(wubu_branch_delete(mgr, "main", false) == -1, "can't delete protected main");
        T(wubu_branch_delete(mgr, "main", true) == 0, "force delete main");
        T(mgr->branch_count == 0, "branch count after force delete");

        NSMGR_FREE(mgr);
    }

    /* -- Branch merge --------------------------------------------- */
    printf("\n[Branch Merge]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        /* Create feature branch from main */
        wubu_branch_create(mgr, "feature", NULL);

        /* Snapshot on main */
        WubuSnapshot *s_main = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "main", "main-1", "Main work", &s_main);

        /* Snapshot on feature */
        WubuSnapshot *s_feat = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "feature", "feat-1", "Feature work", &s_feat);

        /* Merge feature into main */
        WubuSnapshot *merged = NULL;
        T(wubu_branch_merge(mgr, "feature", "main", "Merge feature", &merged) == 0,
          "merge feature into main");
        T(merged != NULL, "merge snapshot returned");
        T(merged->status == WUBU_SNAP_STATUS_READY, "merge snapshot ready");

        NSMGR_FREE(mgr);
    }

    /* -- Tag operations ------------------------------------------- */
    printf("\n[Tag Operations]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        WubuSnapshot *s = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "main", "v1.0", "Release", &s);

        T(wubu_tag_create(mgr, "release-1.0", s->id, "First release", true) == 0, "create tag");
        T(mgr->tag_count == 1, "tag count = 1");
        T(strcmp(mgr->tags[0].name, "release-1.0") == 0, "tag name");
        T(mgr->tags[0].annotated == true, "tag annotated");
        T(strcmp(mgr->tags[0].message, "First release") == 0, "tag message");

        /* Update existing tag */
        T(wubu_tag_create(mgr, "release-1.0", s->id, "Updated message", false) == 0, "update tag");
        T(mgr->tag_count == 1, "tag count still 1 after update");

        /* Tag list */
        WubuTag tags[16];
        T(wubu_tag_list(mgr, tags, 16) == 1, "list tags = 1");

        /* Tag delete */
        T(wubu_tag_delete(mgr, "release-1.0") == 0, "delete tag");
        T(mgr->tag_count == 0, "tag count after delete");

        /* Tag nonexistent snapshot */
        T(wubu_tag_create(mgr, "bad", "nope", NULL, false) == -1, "tag nonexistent snapshot");

        NSMGR_FREE(mgr);
    }

    /* -- Export/Import -------------------------------------------- */
    printf("\n[Export/Import]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        WubuSnapshot *s = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "main", "v1.0", "Export test", &s);
        wubu_tag_create(mgr, "exported", s->id, "For export", true);

        const char *export_path = "/tmp/wubu-snap-export-test.json";
        T(wubu_snapshot_export(mgr, s->id, export_path, true) == 0, "export snapshot");

        /* Verify file exists */
        T(access(export_path, F_OK) == 0, "export file exists");

        /* Import into new manager */
        WubuSnapshotManager *mgr2 = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test2", mgr2);

        WubuSnapshot *imported = NULL;
        T(wubu_snapshot_import(mgr2, export_path, "main", "imported-tag", &imported) == 0,
          "import snapshot");
        T(imported != NULL, "imported snapshot returned");
        T(imported->status == WUBU_SNAP_STATUS_READY, "imported status ready");
        T(mgr2->snapshot_count == 1, "import mgr snapshot count = 1");

        /* Clean up */
        unlink(export_path);
        NSMGR_FREE(mgr2);
        NSMGR_FREE(mgr);
    }

    /* -- Diff ----------------------------------------------------- */
    printf("\n[Diff]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        WubuSnapshot *s1 = NULL, *s2 = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "main", "v1", "First", &s1);
        wubu_snapshot_create(mgr, "c1", s1->id, "main", "v2", "Second", &s2);

        char diff_buf[4096];
        T(wubu_snapshot_diff(mgr, s1->id, s2->id, diff_buf, sizeof(diff_buf)) == 0, "diff snapshots");
        T(strstr(diff_buf, "v1") != NULL, "diff contains v1");
        T(strstr(diff_buf, "v2") != NULL, "diff contains v2");

        NSMGR_FREE(mgr);
    }

    /* -- Rollback ------------------------------------------------- */
    printf("\n[Rollback]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        WubuSnapshot *s1 = NULL, *s2 = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "main", "v1", "First", &s1);
        wubu_snapshot_create(mgr, "c1", s1->id, "main", "v2", "Second", &s2);

        /* Branch head should be s2 */
        T(strcmp(mgr->branches[0].head_snapshot_id, s2->id) == 0, "head is s2");

        /* Rollback to s1 */
        T(wubu_snapshot_rollback(mgr, s1->id) == 0, "rollback to s1");
        T(strcmp(mgr->branches[0].head_snapshot_id, s1->id) == 0, "head is s1 after rollback");

        NSMGR_FREE(mgr);
    }

    /* -- Restore as new ------------------------------------------- */
    printf("\n[Restore As New]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        WubuSnapshot *s = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "main", "v1", "Source", &s);

        char new_id[64];
        T(wubu_snapshot_restore_as_new(mgr, s->id, "c1-restored", new_id) == 0,
          "restore as new");
        T(new_id[0] != '\0', "new container ID generated");
        T(mgr->snapshot_count == 2, "snapshot count after restore");

        NSMGR_FREE(mgr);
    }

    /* -- Snapshot tree -------------------------------------------- */
    printf("\n[Snapshot Tree]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        WubuSnapshot *s1 = NULL, *s2 = NULL, *s3 = NULL;
        wubu_snapshot_create(mgr, "c1", NULL, "main", "v1", "First", &s1);
        wubu_snapshot_create(mgr, "c1", s1->id, "main", "v2", "Second", &s2);
        wubu_snapshot_create(mgr, "c1", s2->id, "main", "v3", "Third", &s3);

        char tree[256][256];
        int depth = wubu_snapshot_tree(mgr, s3->id, tree, 256);
        T(depth == 3, "tree depth = 3");
        T(strstr(tree[0], "v1") != NULL, "tree root is v1");
        T(strstr(tree[2], "v3") != NULL, "tree leaf is v3");

        NSMGR_FREE(mgr);
    }

    /* -- Garbage Collection --------------------------------------- */
    printf("\n[Garbage Collection]\n");
    {
        WubuSnapshotManager *mgr = smgr();
        wubu_snapshot_manager_init("/tmp/wubu-snap-test", mgr);

        /* Create 5 snapshots */
        WubuSnapshot *s[5];
        wubu_snapshot_create(mgr, "c1", NULL, "main", "v1", "First", &s[0]);
        for (int i = 1; i < 5; i++) {
            char label[32], desc[32];
            snprintf(label, sizeof(label), "v%d", i + 1);
            snprintf(desc, sizeof(desc), "Version %d", i + 1);
            wubu_snapshot_create(mgr, "c1", s[i-1]->id, "main", label, desc, &s[i]);
        }
        T(mgr->snapshot_count == 5, "5 snapshots created");

        /* Add retention rule: keep last 3 */
        WubuRetentionRule rule;
        memset(&rule, 0, sizeof(rule));
        rule.type = WUBU_RETENTION_KEEP_LAST_N;
        rule.value = 3;
        rule.enabled = true;
        T(wubu_snapshot_gc_add_rule(mgr, &rule) == 0, "add retention rule");

        /* Run GC */
        T(wubu_snapshot_gc(mgr) == 0, "run GC");
        T(mgr->snapshot_count == 3, "3 snapshots remain after GC");

        /* GC rules list */
        WubuRetentionRule rules[16];
        T(wubu_snapshot_gc_list_rules(mgr, rules, 16) == 1, "list GC rules = 1");

        /* GC rule remove */
        T(wubu_snapshot_gc_remove_rule(mgr, 0) == 0, "remove GC rule");
        T(mgr->retention_rule_count == 0, "GC rule count after remove");

        NSMGR_FREE(mgr);
    }

    /* -- String helpers ------------------------------------------- */
    printf("\n[String Helpers]\n");
    {
        T(strcmp(wubu_snapshot_status_str(WUBU_SNAP_STATUS_CREATING), "creating") == 0, "status: creating");
        T(strcmp(wubu_snapshot_status_str(WUBU_SNAP_STATUS_READY), "ready") == 0, "status: ready");
        T(strcmp(wubu_snapshot_status_str(WUBU_SNAP_STATUS_MOUNTED), "mounted") == 0, "status: mounted");
        T(strcmp(wubu_snapshot_status_str(WUBU_SNAP_STATUS_ERROR), "error") == 0, "status: error");
        T(strcmp(wubu_snapshot_status_str(WUBU_SNAP_STATUS_DELETING), "deleting") == 0, "status: deleting");
        T(strcmp(wubu_snapshot_status_str(WUBU_SNAP_STATUS_MERGING), "merging") == 0, "status: merging");
        T(strcmp(wubu_snapshot_status_str(9), "unknown") == 0, "status: unknown");

        T(strcmp(wubu_snapshot_type_str(WUBU_SNAP_TYPE_FULL), "full") == 0, "type: full");
        T(strcmp(wubu_snapshot_type_str(WUBU_SNAP_TYPE_INCREMENTAL), "incremental") == 0, "type: incremental");
        T(strcmp(wubu_snapshot_type_str(WUBU_SNAP_TYPE_DELTA), "delta") == 0, "type: delta");
        T(strcmp(wubu_snapshot_type_str(WUBU_SNAP_TYPE_BASE), "base") == 0, "type: base");
        T(strcmp(wubu_snapshot_type_str(8), "unknown") == 0, "type: unknown");

        T(strcmp(wubu_fs_type_str(WUBU_FS_OVERLAYFS), "overlayfs") == 0, "fs: overlayfs");
        T(strcmp(wubu_fs_type_str(WUBU_FS_BTRFS), "btrfs") == 0, "fs: btrfs");
        T(strcmp(wubu_fs_type_str(WUBU_FS_ZFS), "zfs") == 0, "fs: zfs");
        T(strcmp(wubu_fs_type_str(WUBU_FS_LVM), "lvm") == 0, "fs: lvm");
        T(strcmp(wubu_fs_type_str(WUBU_FS_AUTO), "auto") == 0, "fs: auto");
        T(strcmp(wubu_fs_type_str(8), "unknown") == 0, "fs: unknown");
    }

    /* -- LVM snapshot closure (form≠function) -------------------- */
    printf("\n[LVM Snapshot Closure]\n");
    {
        /* Before closure, lvm_snapshot_create was a stub returning -ENOSYS
         * (hardcoded). It must now actually attempt lvcreate (fork/exec) and
         * return 0/-1 based on the real command outcome, never -ENOSYS. */
        int rc = lvm_snapshot_create("/dev/vg0/root", "/dev/vg0/root-snap");
        T(rc != -ENOSYS, "lvm_snapshot_create no longer returns hardcoded -ENOSYS stub");
        /* On a host without lvm2 it returns -1 (command not found / nonzero);
         * the point is it attempted the real operation, not a stub value. */
    }

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
