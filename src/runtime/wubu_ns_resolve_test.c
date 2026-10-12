/* src/runtime/wubu_ns_resolve_test.c
 *
 * The /n path resolver test. Verifies a /n/... path maps to the live
 * namespace tree, non-/n paths pass through, and an unmounted /n is
 * reported (not silently mis-resolved):
 *   1. /n/bonzi/enabled -> <root>/bonzi/enabled when mounted
 *   2. a host path passes through unchanged
 *   3. /n path with no root mounted -> -1, empty out
 *   4. is_ns_path classification
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_resolve.h"
#include "wubu_test.h"
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

int main(void)
{
    printf("=== wubu_ns_resolve_test (/n path resolver) ===\n");
    char out[512];

    /* 1. mounted -> resolves under the root */
    g_ns_root = "/tmp/nsroot";
    CHECK(wubu_ns_resolve("/n/bonzi/enabled", out, sizeof out) == 0,
          "mounted /n path resolves");
    CHECK(strcmp(out, "/tmp/nsroot/bonzi/enabled") == 0,
          "/n/bonzi/enabled -> <root>/bonzi/enabled");

    /* 2. host path passes through */
    CHECK(wubu_ns_resolve("/etc/hosts", out, sizeof out) == 0 &&
          strcmp(out, "/etc/hosts") == 0, "host path passes through unchanged");

    /* 3. unmounted /n -> -1, empty */
    g_ns_root = NULL;
    CHECK(wubu_ns_resolve("/n/bonzi/enabled", out, sizeof out) == -1,
          "unmounted /n path returns -1");
    CHECK(out[0] == '\0', "unmounted /n path yields empty out");

    /* 4. classification */
    CHECK(wubu_ns_is_ns_path("/n/kv") == 1, "/n/kv is a ns path");
    CHECK(wubu_ns_is_ns_path("/n") == 1, "/n is a ns path");
    CHECK(wubu_ns_is_ns_path("/home/x") == 0, "/home/x is not a ns path");

    printf("=== wubu_ns_resolve_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
