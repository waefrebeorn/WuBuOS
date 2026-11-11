/*
 * wubu_gvfs_mount_test.c -- the mount table and path-to-mount resolution.
 *
 * The load-bearing rule: LONGEST PREFIX WINS.  With mounts at "/mnt" and
 * "/mnt/smb", the path "/mnt/smb/x" belongs to "/mnt/smb" -- a first-match
 * resolver hands the deeper mount's files to the shallower filesystem.
 */
#include "wubu_gvfs_mount.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_wubu_gvfs_mount ===\n");
    wubu_gvfs_mount_table_t t;
    wubu_gvfs_mount_table_init(&t);
    const wubu_gvfs_mount_t *m;
    char inner[512];

    CHECK(t.count == 0, "the table starts empty");

    /* -- mounting -- */
    CHECK(wubu_gvfs_mount(&t, "/mnt", "file", false) == 0, "a mount is added");
    CHECK(t.count == 1, "one mount");
    CHECK(wubu_gvfs_mount(&t, "/mnt/smb", "smb", false) == 0, "a NESTED mount is added");
    CHECK(t.count == 2, "two mounts");

    /* -- longest prefix wins -- */
    m = wubu_gvfs_resolve(&t, "/mnt/smb/photos/x.jpg", inner, sizeof(inner));
    CHECK(m != NULL, "a nested path resolves");
    CHECK(m && strcmp(m->scheme, "smb") == 0,
          "to the DEEPEST mount, not the shallow one (a sandbox escape)");
    CHECK(strcmp(inner, "/photos/x.jpg") == 0,
          "and the inner path is relative to the DEEP root");

    m = wubu_gvfs_resolve(&t, "/mnt/other.txt", inner, sizeof(inner));
    CHECK(m != NULL && strcmp(m->scheme, "file") == 0,
          "a path under the shallow mount resolves to the shallow one");
    CHECK(strcmp(inner, "/other.txt") == 0, "with the shallow inner path");

    m = wubu_gvfs_resolve(&t, "/mnt", inner, sizeof(inner));
    CHECK(m != NULL && strcmp(m->scheme, "file") == 0,
          "the mount point itself resolves to its own mount");
    CHECK(strcmp(inner, "/") == 0, "with the root as its inner path");

    /* -- a path that matches no mount resolves to NOTHING -- */
    CHECK(wubu_gvfs_resolve(&t, "/etc/passwd", inner, sizeof(inner)) == NULL,
          "a path outside every mount resolves to nothing (never the root by default)");
    CHECK(wubu_gvfs_resolve(&t, "/", inner, sizeof(inner)) == NULL,
          "and the bare root matches no mount here");

    /* -- "/mnt" must not match "/mntx" -- */
    CHECK(wubu_gvfs_resolve(&t, "/mntx/file", inner, sizeof(inner)) == NULL,
          "a prefix that is not a path boundary does not match");

    /* -- the path is normalized BEFORE matching -- */
    m = wubu_gvfs_resolve(&t, "/mnt/smb/../../etc/passwd", inner, sizeof(inner));
    CHECK(m == NULL,
          "'/mnt/smb/../../etc/passwd' normalizes to /etc/passwd, which no mount owns");
    m = wubu_gvfs_resolve(&t, "/mnt/./smb/photos/../x", inner, sizeof(inner));
    CHECK(m != NULL && strcmp(m->scheme, "smb") == 0,
          "a path that normalizes INTO a mount still resolves");
    CHECK(strcmp(inner, "/x") == 0, "with the normalized inner path");

    /* -- bad roots -- */
    CHECK(wubu_gvfs_mount(&t, "relative/path", "file", false) == -1,
          "an unnormalized root is refused");
    CHECK(wubu_gvfs_mount(&t, "/bad/../root", "file", false) == -1,
          "a root containing '..' is refused (it is a traversal)");
    CHECK(wubu_gvfs_mount(&t, "/trailing/", "file", false) == -1,
          "a trailing slash is refused (it is not normalized)");
    CHECK(wubu_gvfs_mount(&t, "/dup", "file", false) == 0, "a mount is added");
    CHECK(wubu_gvfs_mount(&t, "/dup", "smb", false) == -1,
          "a DUPLICATE root is an ERROR, not a replace (it would redirect traffic)");
    CHECK(t.count == 3, "and the duplicate did not overwrite the first");

    /* -- read-only is recorded -- */
    CHECK(wubu_gvfs_mount(&t, "/ro", "cd", true) == 0, "a read-only mount is added");
    m = wubu_gvfs_resolve(&t, "/ro/x", inner, sizeof(inner));
    CHECK(m != NULL && m->read_only, "its read-only flag survives");

    /* -- contains -- */
    CHECK(wubu_gvfs_mount_contains(m, "/ro/x"), "a mount contains its own path");
    CHECK(!wubu_gvfs_mount_contains(m, "/rox"), "and not a path that merely shares a prefix");
    CHECK(!wubu_gvfs_mount_contains(NULL, "/ro/x"), "a null mount contains nothing");

    /* -- unmount -- */
    CHECK(wubu_gvfs_unmount(&t, "/mnt/smb") == 0, "a mount is unmounted");
    CHECK(wubu_gvfs_unmount(&t, "/mnt/smb") == -1, "unmounting twice is an error");
    CHECK(wubu_gvfs_unmount(&t, "/nope") == -1, "an unknown root is an error");
    m = wubu_gvfs_resolve(&t, "/mnt/smb/x", inner, sizeof(inner));
    CHECK(m != NULL && strcmp(m->scheme, "file") == 0,
          "after unmounting, the SHALLOWER mount takes over its subtree");
    CHECK(!wubu_gvfs_mount_contains(wubu_gvfs_resolve(&t, "/ro/x", inner, sizeof(inner)),
                                     "/ro/x") == false,
          "the read-only mount is unaffected");

    /* -- the table is bounded -- */
    {
        wubu_gvfs_mount_table_t big;
        wubu_gvfs_mount_table_init(&big);
        int added = 0;
        char root[64];
        for (int i = 0; i < WUBU_GVFS_MAX_MOUNTS + 5; i++) {
            snprintf(root, sizeof(root), "/m%d", i);
            if (wubu_gvfs_mount(&big, root, "file", false) == 0) added++;
        }
        CHECK(added == WUBU_GVFS_MAX_MOUNTS, "the table holds exactly its bound");
        CHECK(wubu_gvfs_mount(&big, "/overflow", "file", false) == -1,
              "a full table refuses new mounts");
    }

    /* -- null handling -- */
    wubu_gvfs_mount_table_init(NULL);
    CHECK(wubu_gvfs_mount(NULL, "/x", "file", false) == -1, "a null table is refused");
    CHECK(wubu_gvfs_mount(&t, NULL, "file", false) == -1, "a null root is refused");
    CHECK(wubu_gvfs_mount(&t, "/x", NULL, false) == -1, "a null scheme is refused");
    CHECK(wubu_gvfs_unmount(NULL, "/x") == -1, "a null unmount is refused");
    CHECK(wubu_gvfs_resolve(NULL, "/x", inner, sizeof(inner)) == NULL,
          "a null table resolves nothing");
    CHECK(wubu_gvfs_resolve(&t, NULL, inner, sizeof(inner)) == NULL,
          "a null path resolves nothing");

    if (failures == 0) printf("ALL WUBU_GVFS_MOUNT TESTS PASSED\n");
    else printf("%d WUBU_GVFS_MOUNT FAILURES\n", failures);
    return failures ? 1 : 0;
}
