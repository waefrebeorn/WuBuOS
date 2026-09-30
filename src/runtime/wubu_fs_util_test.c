/*
 * wubu_fs_util_test.c -- regression tests for the shared filesystem helpers.
 *
 * The load-bearing case is wubu_fs_rm_rf over a MIXED tree (regular files +
 * nested subdirs + symlinks). That is the exact shape the NT registry delete
 * path deletes, and it is where the FTW_DP enum-shadowing bug lived: when
 * FTW_DP collided with FTW_F, the unlink callback rmdir()'d every regular
 * file, got ENOTDIR, and nftw aborted early -- leaving the tree on disk and
 * NtDeleteKey returning NT_STATUS_UNSUCCESSFUL. A rm_rf test that only deleted
 * an empty directory would NOT have caught it.
 *
 * C11, self-contained.
 */
#include "wubu_gnu_compat.h"
#include "wubu_fs_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do {                                   \
    if (cond) { g_pass++; printf("  [PASS] %s\n", (msg)); }     \
    else      { g_fail++; printf("  [FAIL] %s\n", (msg)); }     \
} while (0)

static int g_rm_root = 0;

static const char *rm_root(void) {
    static char buf[256];
    if (!g_rm_root) {
        snprintf(buf, sizeof(buf), "/tmp/wubu_fsutil_test_%d", (int)getpid());
        g_rm_root = 1;
    }
    return buf;
}

static void rm_root_reset(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", rm_root());
    if (system(cmd) != 0) { /* best effort */ }
}

static int write_file_at(const char *path, const char *content) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    size_t n = strlen(content);
    if (n && fwrite(content, 1, n, fp) != n) { fclose(fp); return -1; }
    return fclose(fp) == 0 ? 0 : -1;
}

static int path_exists(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0;
}

/* Build a mixed tree:
 *   root/a.txt            regular file
 *   root/sub/b.txt        regular file in a subdir
 *   root/sub/deep/c.txt   regular file two levels down
 *   root/empty/           empty directory
 *   root/link -> a.txt    symlink (must not be followed: FTW_PHYS)
 */
static int build_mixed_tree(char *root_out, size_t root_sz) {
    char p[512];
    snprintf(root_out, root_sz, "%s", rm_root());

    if (mkdir(root_out, 0755) != 0 && errno != EEXIST) return -1;

    snprintf(p, sizeof(p), "%s/a.txt", root_out);
    if (write_file_at(p, "alpha")) return -1;

    snprintf(p, sizeof(p), "%s/sub", root_out);
    if (mkdir(p, 0755) != 0) return -1;
    snprintf(p, sizeof(p), "%s/sub/b.txt", root_out);
    if (write_file_at(p, "beta")) return -1;

    snprintf(p, sizeof(p), "%s/sub/deep", root_out);
    if (mkdir(p, 0755) != 0) return -1;
    snprintf(p, sizeof(p), "%s/sub/deep/c.txt", root_out);
    if (write_file_at(p, "gamma")) return -1;

    snprintf(p, sizeof(p), "%s/empty", root_out);
    if (mkdir(p, 0755) != 0) return -1;

    snprintf(p, sizeof(p), "%s/link", root_out);
    if (symlink("a.txt", p) != 0) return -1;

    return 0;
}

int main(void) {
    char root[256];

    printf("=== wubu_fs_util regression tests ===\n");

    /* --- 1. rm_rf over a MIXED tree (the FTW_DP bug's exact shape) --- */
    rm_root_reset();
    if (build_mixed_tree(root, sizeof(root)) != 0) {
        printf("  [FAIL] could not build mixed tree under %s\n", root);
        g_fail++;
    } else {
        char p[512];
        snprintf(p, sizeof(p), "%s/a.txt", root);
        CHECK(path_exists(p), "precondition: regular file exists");
        snprintf(p, sizeof(p), "%s/sub/deep/c.txt", root);
        CHECK(path_exists(p), "precondition: nested regular file exists");

        int rc = wubu_fs_rm_rf(root);
        CHECK(rc == 0, "rm_rf over mixed tree returns 0");
        CHECK(!path_exists(root), "rm_rf removed the entire tree (no leftovers)");

        /* The specific leftovers the FTW_DP bug produced: a subdirectory whose
         * files were never unlinked, i.e. the tree root still present. */
        CHECK(!path_exists(root), "rm_rf did not abort early (FTW_DP not == FTW_F)");
    }

    /* --- 2. rm_rf on an empty dir --- */
    {
        char e[512];
        snprintf(e, sizeof(e), "%s_empty", rm_root());
        rm_root_reset();
        if (mkdir(e, 0755) == 0) {
            CHECK(wubu_fs_rm_rf(e) == 0, "rm_rf on empty dir returns 0");
            CHECK(!path_exists(e), "rm_rf on empty dir removed it");
        }
    }

    /* --- 3. rm_rf on a single regular file --- */
    {
        char f[512];
        snprintf(f, sizeof(f), "%s_file", rm_root());
        rm_root_reset();
        if (write_file_at(f, "solo") == 0) {
            CHECK(wubu_fs_rm_rf(f) == 0, "rm_rf on a single file returns 0");
            CHECK(!path_exists(f), "rm_rf on a single file removed it");
        }
    }

    /* --- 4. rm_rf(NULL) is rejected, not a crash --- */
    CHECK(wubu_fs_rm_rf(NULL) != 0, "rm_rf(NULL) returns nonzero (no crash)");

    /* --- 5. rm_rf on a nonexistent path fails cleanly --- */
    {
        char n[512];
        snprintf(n, sizeof(n), "%s_does_not_exist_12345", rm_root());
        CHECK(wubu_fs_rm_rf(n) != 0, "rm_rf on nonexistent path returns nonzero");
    }

    rm_root_reset();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
