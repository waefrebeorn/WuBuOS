/*
 * wubu_pkg_resolve_test.c -- versions, constraints, dependency resolution.
 *
 * The load-bearing rules: components compare NUMERICALLY (1.10 > 1.9), the
 * order is a POST-ORDER (a dependency before its dependent), a diamond is
 * emitted ONCE, a cycle is detected rather than recursed into, and a removal
 * may not leave an installed dependent broken.
 */
#include "wubu_pkg.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_wubu_pkg_resolve ===\n");

    /* -- version comparison: NUMERIC per component -- */
    CHECK(pkg_version_cmp("1.0", "1.0") == 0, "1.0 == 1.0");
    CHECK(pkg_version_cmp("1.0", "1.0.0") == 0, "1.0 == 1.0.0 (missing == zero)");
    CHECK(pkg_version_cmp("1.10", "1.9") > 0, "1.10 > 1.9 (NUMERIC, not lexicographic)");
    CHECK(pkg_version_cmp("1.9", "1.10") < 0, "1.9 < 1.10");
    CHECK(pkg_version_cmp("2.0", "1.99") > 0, "2.0 > 1.99");
    CHECK(pkg_version_cmp("1.0.1", "1.0") > 0, "1.0.1 > 1.0");
    CHECK(pkg_version_cmp("1.0", "1.0.1") < 0, "1.0 < 1.0.1 (the shorter is less)");
    CHECK(pkg_version_cmp("1.0a", "1.0") > 0, "1.0a > 1.0 (a suffix sorts after)");
    CHECK(pkg_version_cmp("1.0", "1.0a") < 0, "1.0 < 1.0a");
    CHECK(pkg_version_cmp("1.0a", "1.0b") < 0, "1.0a < 1.0b");
    CHECK(pkg_version_cmp(NULL, "1.0") == 0, "a null version compares equal");

    /* -- constraints -- */
    CHECK(pkg_satisfies("1.5.0", ""), "an empty constraint accepts anything");
    CHECK(pkg_satisfies("1.5.0", NULL), "a null constraint accepts anything");
    CHECK(pkg_satisfies("1.5.0", "=1.5.0"), "= matches exactly");
    CHECK(!pkg_satisfies("1.5.1", "=1.5.0"), "= rejects a different version");
    CHECK(pkg_satisfies("1.5.0", "1.5.0"), "a bare version means '='");
    CHECK(pkg_satisfies("2.0", ">=1.0"), ">= accepts a greater version");
    CHECK(pkg_satisfies("1.0", ">=1.0"), ">= accepts an equal version");
    CHECK(!pkg_satisfies("0.9", ">=1.0"), ">= rejects a lesser version");
    CHECK(pkg_satisfies("1.0", "<=1.0"), "<= accepts an equal version");
    CHECK(pkg_satisfies("0.9", "<=1.0"), "<= accepts a lesser version");
    CHECK(!pkg_satisfies("1.1", "<=1.0"), "<= rejects a greater version");
    CHECK(pkg_satisfies("2.0", ">1.0"), "> accepts a greater version");
    CHECK(!pkg_satisfies("1.0", ">1.0"), "> rejects an equal version");
    CHECK(pkg_satisfies("0.9", "<1.0"), "< accepts a lesser version");
    CHECK(!pkg_satisfies("1.0", "<1.0"), "< rejects an equal version");
    CHECK(pkg_satisfies("1.7.3", "1.*"), "'1.*' matches any 1.x");
    CHECK(!pkg_satisfies("2.0.0", "1.*"), "'1.*' rejects a 2.x");
    CHECK(pkg_satisfies("1.0", "==1.0"), "'==' also means exactly");
    CHECK(!pkg_satisfies(NULL, ">=1.0"), "a null version satisfies nothing");

    /* -- resolution: a chain a -> b -> c -- */
    {
        PkgManager m;
        pkg_init(&m);
        pkg_register(&m, "a", "1.0", "", 1);
        pkg_register(&m, "b", "1.0", "", 1);
        pkg_register(&m, "c", "1.0", "", 1);
        pkg_add_dep(&m, "a", "b");
        pkg_add_dep(&m, "b", "c");

        char order[16][PKG_MAX_NAME];
        int n = 0;
        CHECK(pkg_resolve_order(&m, "a", order, 16, &n) == PKG_RESOLVE_OK,
              "a chain resolves");
        CHECK(n == 3, "three packages");
        CHECK(strcmp(order[0], "c") == 0, "the DEEPEST dependency comes first");
        CHECK(strcmp(order[1], "b") == 0, "then its dependent");
        CHECK(strcmp(order[2], "a") == 0, "and the target LAST (post-order)");
        pkg_shutdown(&m);
    }

    /* -- resolution: a diamond, emitted once -- */
    {
        PkgManager m;
        pkg_init(&m);
        pkg_register(&m, "top", "1.0", "", 1);
        pkg_register(&m, "left", "1.0", "", 1);
        pkg_register(&m, "right", "1.0", "", 1);
        pkg_register(&m, "bottom", "1.0", "", 1);
        pkg_add_dep(&m, "top", "left");
        pkg_add_dep(&m, "top", "right");
        pkg_add_dep(&m, "left", "bottom");
        pkg_add_dep(&m, "right", "bottom");

        char order[16][PKG_MAX_NAME];
        int n = 0;
        CHECK(pkg_resolve_order(&m, "top", order, 16, &n) == PKG_RESOLVE_OK,
              "a diamond resolves");
        CHECK(n == 4, "exactly four -- the shared dependency is emitted ONCE");
        CHECK(strcmp(order[0], "bottom") == 0, "the shared dependency comes first");
        CHECK(strcmp(order[3], "top") == 0, "the target comes last");
        /* left and right must both be after bottom and before top */
        int li = -1, ri = -1;
        for (int i = 0; i < n; i++) {
            if (strcmp(order[i], "left") == 0) li = i;
            if (strcmp(order[i], "right") == 0) ri = i;
        }
        CHECK(li > 0 && ri > 0 && li < 3 && ri < 3,
              "both middle packages are between the dependency and the target");
        pkg_shutdown(&m);
    }

    /* -- resolution: a cycle is DETECTED, not recursed into -- */
    {
        PkgManager m;
        pkg_init(&m);
        pkg_register(&m, "x", "1.0", "", 1);
        pkg_register(&m, "y", "1.0", "", 1);
        pkg_add_dep(&m, "x", "y");
        pkg_add_dep(&m, "y", "x");

        char order[16][PKG_MAX_NAME];
        int n = 99;
        CHECK(pkg_resolve_order(&m, "x", order, 16, &n) == PKG_RESOLVE_CYCLE,
              "a two-package cycle is detected");
        CHECK(n == 0, "and nothing is emitted");
        CHECK(strcmp(pkg_resolve_status_name(PKG_RESOLVE_CYCLE), "cycle") == 0,
              "the status has a readable name");
        pkg_shutdown(&m);
    }
    {
        PkgManager m;
        pkg_init(&m);
        pkg_register(&m, "self", "1.0", "", 1);
        pkg_add_dep(&m, "self", "self");
        char order[16][PKG_MAX_NAME];
        int n = 0;
        CHECK(pkg_resolve_order(&m, "self", order, 16, &n) == PKG_RESOLVE_CYCLE,
              "a SELF-dependency is a cycle");
        pkg_shutdown(&m);
    }

    /* -- resolution: a missing dependency is reported -- */
    {
        PkgManager m;
        pkg_init(&m);
        pkg_register(&m, "needs", "1.0", "", 1);
        pkg_add_dep(&m, "needs", "absent");
        char order[16][PKG_MAX_NAME];
        int n = 99;
        CHECK(pkg_resolve_order(&m, "needs", order, 16, &n) == PKG_RESOLVE_MISSING,
              "a missing dependency is reported by status");
        CHECK(n == 0, "and nothing is emitted");
        pkg_shutdown(&m);
    }

    /* -- resolution: bad arguments -- */
    {
        PkgManager m;
        pkg_init(&m);
        pkg_register(&m, "solo", "1.0", "", 1);
        char order[16][PKG_MAX_NAME];
        int n = 0;
        CHECK(pkg_resolve_order(&m, "nope", order, 16, &n) == PKG_RESOLVE_NOT_FOUND,
              "an unknown target is NOT_FOUND");
        CHECK(pkg_resolve_order(NULL, "solo", order, 16, &n) == PKG_RESOLVE_BAD_ARGUMENT,
              "a null manager is refused");
        CHECK(pkg_resolve_order(&m, NULL, order, 16, &n) == PKG_RESOLVE_BAD_ARGUMENT,
              "a null target is refused");
        CHECK(pkg_resolve_order(&m, "solo", NULL, 16, &n) == PKG_RESOLVE_BAD_ARGUMENT,
              "a null output is refused");
        CHECK(pkg_resolve_order(&m, "solo", order, 0, &n) == PKG_RESOLVE_BAD_ARGUMENT,
              "a zero-size output is refused");
        CHECK(pkg_resolve_order(&m, "solo", order, 16, NULL) == PKG_RESOLVE_OK,
              "a null out_n is tolerated");
        CHECK(strcmp(pkg_resolve_status_name((PkgResolveStatus)99), "unknown") == 0,
              "an unknown status has a name");
        pkg_shutdown(&m);
    }

    /* -- removal may not leave an installed dependent broken -- */
    {
        PkgManager m;
        pkg_init(&m);
        pkg_register(&m, "lib", "1.0", "", 1);
        pkg_register(&m, "app", "1.0", "", 1);
        pkg_add_dep(&m, "app", "lib");

        CHECK(pkg_install(&m, "lib") == 0, "the library is installed");
        CHECK(pkg_install(&m, "app") == 0, "the app is installed");
        CHECK(pkg_remove(&m, "lib") == -1,
              "removing a dependency of an INSTALLED package is REFUSED");
        CHECK(pkg_is_installed(&m, "lib"), "and the library is still installed");

        /* once the dependent is gone, the removal succeeds */
        CHECK(pkg_remove(&m, "app") == 0, "the app can be removed");
        CHECK(pkg_remove(&m, "lib") == 0, "and then the library");
        CHECK(!pkg_is_installed(&m, "lib"), "and it is gone");

        /* removing twice is refused */
        CHECK(pkg_remove(&m, "lib") == -1, "removing twice is an error");
        pkg_shutdown(&m);
    }

    /* -- dependents -- */
    {
        PkgManager m;
        pkg_init(&m);
        pkg_register(&m, "base", "1.0", "", 1);
        pkg_register(&m, "one", "1.0", "", 1);
        pkg_register(&m, "two", "1.0", "", 1);
        pkg_register(&m, "three", "1.0", "", 1);
        pkg_add_dep(&m, "one", "base");
        pkg_add_dep(&m, "two", "base");
        pkg_add_dep(&m, "three", "unrelated");

        char d[8][PKG_MAX_NAME];
        CHECK(pkg_dependents(&m, "base", d, 8) == 0,
              "nothing depends on it while nothing is installed");
        pkg_install(&m, "one");
        pkg_install(&m, "two");
        CHECK(pkg_dependents(&m, "base", d, 8) == 2,
              "two INSTALLED packages depend on it");
        CHECK(pkg_dependents(NULL, "base", d, 8) == 0, "a null manager yields none");
        pkg_shutdown(&m);
    }

    if (failures == 0) printf("ALL WUBU_PKG_RESOLVE TESTS PASSED\n");
    else printf("%d WUBU_PKG_RESOLVE FAILURES\n", failures);
    return failures ? 1 : 0;
}
