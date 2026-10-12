/* src/runtime/wubu_syscall_shim_test.c
 *
 * Foreign-ABI syscall translation (Track D1 shim). Verifies foreign
 * syscalls map onto WuBu operations, file paths translate, and unmapped
 * syscalls are refused (fail-closed):
 *   1. file syscalls -> FILE op + translated namespace path
 *   2. graphics syscalls -> GRAPHICS op (no path)
 *   3. net syscalls -> NET op
 *   4. an unknown syscall -> REFUSE (never guessed)
 */
#include "wubu_syscall_shim.h"
#include "wubu_test.h"
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } } while (0)

int main(void)
{
    printf("=== wubu_syscall_shim_test (foreign syscall translation) ===\n");
    char path[512];

    /* 1. a Windows file-open translates to a FILE op + namespace path */
    wubu_shu_op_t op = wubu_syscall_shim_translate(WUBU_ABI_WIN, WUBU_FSC_OPEN,
                                                   "C:\\data\\f.txt", path, sizeof path);
    CHECK(op == WUBU_SHU_FILE, "file open -> FILE op");
    CHECK(strcmp(path, "/n/foreign/win/c/data/f.txt") == 0,
          "file open path translated onto the namespace");

    /* 2. a graphics syscall -> GRAPHICS, no path */
    op = wubu_syscall_shim_translate(WUBU_ABI_WIN, WUBU_FSC_FILL_RECT,
                                     NULL, path, sizeof path);
    CHECK(op == WUBU_SHU_GRAPHICS, "fill_rect -> GRAPHICS op");
    CHECK(path[0] == '\0', "graphics op carries no path");

    /* 3. a net syscall -> NET */
    op = wubu_syscall_shim_translate(WUBU_ABI_UNIX, WUBU_FSC_SOCKET,
                                     NULL, path, sizeof path);
    CHECK(op == WUBU_SHU_NET, "socket -> NET op");

    /* 4. unknown syscall -> REFUSE (fail-closed) */
    op = wubu_syscall_shim_classify(WUBU_FSC_UNKNOWN);
    CHECK(op == WUBU_SHU_REFUSE, "unknown syscall -> REFUSE");
    op = wubu_syscall_shim_translate(WUBU_ABI_UNIX, WUBU_FSC_UNKNOWN,
                                     "/x", path, sizeof path);
    CHECK(op == WUBU_SHU_REFUSE, "unknown syscall translate -> REFUSE");

    /* 5. op names */
    CHECK(strcmp(wubu_syscall_shim_op_name(WUBU_SHU_REFUSE), "refuse") == 0,
          "refuse name");

    printf("=== wubu_syscall_shim_test: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
