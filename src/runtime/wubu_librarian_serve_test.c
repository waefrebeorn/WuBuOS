/* src/runtime/wubu_librarian_serve_test.c
 *
 * The librarian's LIVE interop entry point (Track D2, wired). Verifies the
 * default participants self-register and cross-system calls route through:
 *   1. init registers the default WuBu participants
 *   2. a foreign app opens a KV path -> served from /n/kv
 *   3. a capability query resolves to the offering participant's root
 *   4. an unknown target is refused
 */
#include "wubu_librarian_serve.h"
#include "wubu_librarian.h"
#include "wubu_test.h"
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

int main(void)
{
    printf("=== wubu_librarian_serve_test (live interop entry point) ===\n");

    /* 1. default participants register */
    CHECK(wubu_librarian_serve_init() == 0, "init registers default participants");
    CHECK(wubu_librarian_find("kv") != NULL, "the KV store is registered");
    CHECK(wubu_librarian_find("bonzi") != NULL, "the companion is registered");

    /* a foreign Windows app registers itself and is reachable */
    CHECK(wubu_librarian_register("winapp", WUBU_ABI_WIN,
                                  "/n/foreign/win/c/apps", "edit docs") == 0,
          "a foreign Windows app registers");

    /* 2. the foreign app opens a file on the KV store -> served from /n/kv */
    char path[512]; wubu_shu_op_t op = WUBU_SHU_REFUSE;
    int rc = wubu_librarian_serve("winapp", "kv", WUBU_FSC_OPEN,
                                  "/layer/0/slot", path, sizeof path, &op);
    CHECK(rc == 0, "cross-system open routed");
    CHECK(op == WUBU_SHU_FILE, "served as a FILE op");
    CHECK(strcmp(path, "/n/kv/layer/0/slot") == 0,
          "path served from the KV store's namespace root");

    /* 3. capability query: who offers companion memory? */
    char root[256];
    CHECK(wubu_librarian_serve_who("companion memory", root, sizeof root) == 0,
          "capability query resolves");
    CHECK(strcmp(root, "/n/bonzi") == 0, "companion memory -> /n/bonzi");

    /* 4. unknown target refused */
    rc = wubu_librarian_serve("winapp", "ghost", WUBU_FSC_OPEN,
                              "/x", path, sizeof path, &op);
    CHECK(rc == -1, "unknown target refused");

    printf("=== wubu_librarian_serve_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
