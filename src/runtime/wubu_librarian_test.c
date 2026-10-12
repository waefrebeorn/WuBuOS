/* src/runtime/wubu_librarian_test.c
 *
 * The INTEROP LIBRARIAN (Track D2). Verifies systems find each other and
 * call across boundaries, with a translation table that grows as it goes:
 *   1. register participants; find by name; find by offered capability
 *   2. route a cross-ABI file call -> target's namespace path + FILE op
 *   3. the crossing is RECORDED (dictionary grows); a second identical
 *      route is a lookup hit, not a re-compute
 *   4. an unknown target is refused (fail-closed)
 *   5. an unmapped syscall is refused
 */
#include "wubu_librarian.h"
#include "wubu_test.h"
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

int main(void)
{
    printf("=== wubu_librarian_test (the interop librarian) ===\n");

    /* 1. register a native app, a Windows app, a Unix service */
    CHECK(wubu_librarian_register("notepad", WUBU_ABI_WIN,
                                  "/n/foreign/win/c/apps", "edit text") == 0,
          "register Windows notepad");
    CHECK(wubu_librarian_register("filesvc", WUBU_ABI_UNIX,
                                  "/n/foreign/unix", "store files") == 0,
          "register Unix filesvc");
    CHECK(wubu_librarian_count() == 2, "two participants registered");

    /* find by name */
    const wubu_lib_entry_t *np = wubu_librarian_find("notepad");
    CHECK(np != NULL && np->abi == WUBU_ABI_WIN, "find notepad by name");

    /* find by capability -- apps find each other by what they offer */
    const wubu_lib_entry_t *fs = wubu_librarian_who_offers("store");
    CHECK(fs != NULL && strcmp(fs->name, "filesvc") == 0,
          "who_offers('store') resolves to filesvc");

    /* 2. route a cross-ABI file call: notepad (win) opens a file on filesvc (unix) */
    int before = wubu_librarian_xlate_count();
    char root[512]; wubu_shu_op_t op = WUBU_SHU_REFUSE;
    int rc = wubu_librarian_route("notepad", "filesvc", WUBU_FSC_OPEN,
                                  "/srv/data.db", root, sizeof root, &op);
    CHECK(rc == 0, "cross-ABI file call routed");
    CHECK(op == WUBU_SHU_FILE, "served as a FILE op");
    CHECK(strcmp(root, "/n/foreign/unix/srv/data.db") == 0,
          "path resolved against the target's namespace root");

    /* 3. the crossing is recorded -- the dictionary grew */
    CHECK(wubu_librarian_xlate_count() == before + 1,
          "the translation was recorded (dictionary grows)");
    wubu_lib_xlate_t hit;
    CHECK(wubu_librarian_xlate_lookup(WUBU_ABI_WIN, WUBU_ABI_UNIX,
                                      WUBU_SHU_FILE, &hit) == 1,
          "the crossing is now a lookup hit");
    int cnt2 = wubu_librarian_xlate_count();
    wubu_librarian_route("notepad", "filesvc", WUBU_FSC_OPEN,
                         "/srv/data.db", root, sizeof root, &op);
    CHECK(wubu_librarian_xlate_count() == cnt2,
          "re-routing the same crossing does not re-grow the table");

    /* 4. unknown target refused (fail-closed) */
    rc = wubu_librarian_route("notepad", "ghost", WUBU_FSC_OPEN,
                              "/x", root, sizeof root, &op);
    CHECK(rc == -1, "unknown target refused");

    /* 5. unmapped syscall refused */
    rc = wubu_librarian_route("notepad", "filesvc", WUBU_FSC_UNKNOWN,
                              "/x", root, sizeof root, &op);
    CHECK(rc == -1, "unmapped syscall refused");

    printf("=== wubu_librarian_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
