/*
 * wubu_gvfs_path_test.c -- path normalization and resolution.
 *
 * The load-bearing rule: ".." at the ROOT is a TRAVERSAL and is REJECTED, not
 * clamped.  Clamping silently hands the caller a different file than the one
 * they named, which is worse than an error.
 */
#include "wubu_gvfs_path.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static int norm(const char *in, char *out, size_t n)
{
    return wubu_gvfs_path_normalize(in, out, n) == NULL;
}

static bool norm_eq(const char *in, const char *want)
{
    char out[1024];
    if (!norm(in, out, sizeof(out))) return false;
    return strcmp(out, want) == 0;
}

int main(void)
{
    printf("=== test_wubu_gvfs_path ===\n");

    /* -- normalization -- */
    CHECK(norm_eq("a/b/c", "/a/b/c"), "a plain path is rooted");
    CHECK(norm_eq("/a/b/c", "/a/b/c"), "an absolute path is unchanged");
    CHECK(norm_eq("a/./b", "/a/b"), "a '.' component is dropped");
    CHECK(norm_eq("./a", "/a"), "a leading '.' is dropped");
    CHECK(norm_eq("a//b", "/a/b"), "empty components collapse");
    CHECK(norm_eq("a///b", "/a/b"), "and collapse more than two");
    CHECK(norm_eq("a/b/../c", "/a/c"), "'..' pops the previous component");
    CHECK(norm_eq("a/b/c/../../d", "/a/d"), "'..' pops more than one");
    CHECK(norm_eq("a/b/..", "/a"), "'..' at the end pops");
    CHECK(norm_eq("/", "/"), "the root stays the root");
    CHECK(norm_eq("", "/"), "an empty path is the root");
    CHECK(norm_eq("a/b/", "/a/b"), "a trailing slash is dropped");
    CHECK(norm_eq("a", "/a"), "a single component is rooted");

    /* -- traversal is REJECTED, not clamped -- */
    CHECK(!norm("../a", NULL, 0) || true, "probe");
    {
        char out[1024];
        CHECK(wubu_gvfs_path_normalize("../a", out, sizeof(out)) != NULL,
              "'..' above the root is a TRAVERSAL and is rejected");
        CHECK(wubu_gvfs_path_normalize("a/../../b", out, sizeof(out)) != NULL,
              "and so is a second '..' after one pop");
        CHECK(wubu_gvfs_path_normalize("/..", out, sizeof(out)) != NULL,
              "even from an absolute root");
        CHECK(wubu_gvfs_path_normalize("..", out, sizeof(out)) != NULL,
              "a bare '..' is a traversal");
    }
    /* but a '..' that stays INSIDE is fine */
    CHECK(norm_eq("a/../b", "/b"), "'..' that stays inside is legal");
    CHECK(norm_eq("a/b/../../../c", "/c") == false,
          "three '..' after two components is a traversal");

    /* -- is_safe: judged on the NORMALIZED form -- */
    CHECK(wubu_gvfs_path_is_safe("a/b"), "a plain path is safe");
    CHECK(wubu_gvfs_path_is_safe("a/../b"), "'a/../b' is safe (it stays inside)");
    CHECK(!wubu_gvfs_path_is_safe("../x"), "'../x' is NOT safe");
    CHECK(!wubu_gvfs_path_is_safe("a/../../b"), "'a/../../b' is NOT safe");
    CHECK(!wubu_gvfs_path_is_safe(NULL), "a null path is not safe");
    CHECK(wubu_gvfs_path_is_safe("/"), "the root is safe");

    /* -- absolute -- */
    CHECK(wubu_gvfs_path_is_absolute("/a"), "'/a' is absolute");
    CHECK(!wubu_gvfs_path_is_absolute("a"), "'a' is relative");
    CHECK(!wubu_gvfs_path_is_absolute(NULL), "null is not absolute");

    /* -- resolution -- */
    {
        char out[1024];
        CHECK(wubu_gvfs_path_resolve("/mnt", "file", out, sizeof(out)) == NULL &&
              strcmp(out, "/mnt/file") == 0,
              "a file against a directory lands inside it");
        CHECK(wubu_gvfs_path_resolve("/mnt/dir", "file", out, sizeof(out)) == NULL &&
              strcmp(out, "/mnt/dir/file") == 0,
              "the base is a DIRECTORY, so a file lands inside it");
        CHECK(wubu_gvfs_path_resolve("/mnt/dir", "../x", out, sizeof(out)) == NULL &&
              strcmp(out, "/mnt/x") == 0, "a relative '..' against a directory");
        CHECK(wubu_gvfs_path_resolve("/mnt/dir", "sub/./f", out, sizeof(out)) == NULL &&
              strcmp(out, "/mnt/dir/sub/f") == 0,
              "a nested reference is normalized against the base directory");
        CHECK(wubu_gvfs_path_resolve("/mnt/dir", "/abs", out, sizeof(out)) != NULL,
              "an ABSOLUTE reference cannot be resolved against a base");
        /* escaping the BASE is fine -- what matters is the ROOT */
        CHECK(wubu_gvfs_path_resolve("/mnt/dir", "../../x", out, sizeof(out)) == NULL &&
              strcmp(out, "/x") == 0,
              "a reference may rise above the base as long as it stays in the root");
        CHECK(wubu_gvfs_path_resolve("/mnt/dir", "../../../x", out, sizeof(out)) != NULL,
              "but a reference that escapes the ROOT is rejected");
        CHECK(wubu_gvfs_path_resolve(NULL, "f", out, sizeof(out)) != NULL,
              "a null base is refused");
        CHECK(wubu_gvfs_path_resolve("/mnt", NULL, out, sizeof(out)) != NULL,
              "a null reference is refused");
    }

    /* -- split -- */
    {
        char dir[512], name[512];
        CHECK(wubu_gvfs_path_split("/a/b/c", dir, sizeof(dir), name, sizeof(name))
              == NULL, "a path splits");
        CHECK(strcmp(dir, "/a/b") == 0 && strcmp(name, "c") == 0,
              "into the directory and the final component");
        CHECK(wubu_gvfs_path_split("name", dir, sizeof(dir), name, sizeof(name))
              == NULL, "a bare name splits");
        CHECK(strcmp(dir, "") == 0 && strcmp(name, "name") == 0,
              "with an empty directory");
        CHECK(wubu_gvfs_path_split("/a", dir, sizeof(dir), name, sizeof(name))
              == NULL, "a root-level name splits");
        CHECK(strcmp(dir, "/") == 0 && strcmp(name, "a") == 0,
              "with the root as its directory");
        CHECK(wubu_gvfs_path_split(NULL, dir, sizeof(dir), name, sizeof(name))
              != NULL, "a null path is refused");
    }

    /* -- bad arguments -- */
    {
        char out[1024];
        CHECK(wubu_gvfs_path_normalize(NULL, out, sizeof(out)) != NULL,
              "a null path is refused");
        CHECK(wubu_gvfs_path_normalize("a", NULL, sizeof(out)) != NULL,
              "a null out buffer is refused");
        CHECK(wubu_gvfs_path_normalize("a", out, 0) != NULL,
              "a zero-size buffer is refused");
        CHECK(wubu_gvfs_path_normalize("a", out, 2) != NULL,
              "a buffer too small for '/a' is refused");
    }

    if (failures == 0) printf("ALL WUBU_GVFS_PATH TESTS PASSED\n");
    else printf("%d WUBU_GVFS_PATH FAILURES\n", failures);
    return failures ? 1 : 0;
}
