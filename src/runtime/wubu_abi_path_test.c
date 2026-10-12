/* src/runtime/wubu_abi_path_test.c
 *
 * Foreign-ABI path translation (Track D1). Verifies each ABI's paths map
 * onto the WuBu namespace so a shim can serve any foreign OS app:
 *   1. Windows C:\a\b -> /n/foreign/win/c/a/b
 *   2. Unix /usr/bin -> /n/foreign/unix/usr/bin
 *   3. DOS C:\PROG -> /n/foreign/dos/c/PROG
 *   4. two drives don't collide (C: vs D:)
 *   5. messy input (double slashes, trailing slash) is normalized
 */
#include "wubu_abi_path.h"
#include "wubu_test.h"
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

static int eq(const char *foreign, wubu_abi_t abi, const char *want) {
    char out[512];
    if (wubu_abi_path_translate(abi, foreign, out, sizeof out) != 0) return 0;
    return strcmp(out, want) == 0;
}

int main(void)
{
    printf("=== wubu_abi_path_test (foreign-ABI path translation) ===\n");

    /* 1. Windows */
    CHECK(eq("C:\\Users\\bob\\file.txt", WUBU_ABI_WIN,
             "/n/foreign/win/c/Users/bob/file.txt"),
          "Windows C:\\a\\b -> /n/foreign/win/c/a/b");

    /* 2. Unix */
    CHECK(eq("/usr/bin/sh", WUBU_ABI_UNIX, "/n/foreign/unix/usr/bin/sh"),
          "Unix /usr/bin -> /n/foreign/unix/usr/bin");

    /* 3. DOS */
    CHECK(eq("C:\\PROG\\GAME.EXE", WUBU_ABI_DOS, "/n/foreign/dos/c/PROG/GAME.EXE"),
          "DOS C:\\PROG -> /n/foreign/dos/c/PROG");

    /* 4. two drives don't collide */
    CHECK(eq("D:\\data", WUBU_ABI_WIN, "/n/foreign/win/d/data"),
          "Windows D: maps to its own namespace component");

    /* 5. messy input normalized */
    CHECK(eq("C:\\\\a\\\\b\\\\", WUBU_ABI_WIN, "/n/foreign/win/c/a/b"),
          "double + trailing separators collapse");
    CHECK(eq("/usr//bin//", WUBU_ABI_UNIX, "/n/foreign/unix/usr/bin"),
          "Unix double slashes collapse");

    /* 6. bad args rejected */
    char out[64];
    CHECK(wubu_abi_path_translate(WUBU_ABI_WIN, NULL, out, sizeof out) == -1,
          "NULL foreign path rejected");

    printf("=== wubu_abi_path_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
