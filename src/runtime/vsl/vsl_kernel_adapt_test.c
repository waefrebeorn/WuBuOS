/* vsl_kernel_adapt_test.c -- the real personalities, reachable through the
 * interchange.  This is the proof that the network is not decorative: the
 * actual VSL dispatchers answer, and the SELECTED kernel owns the bare range.
 */
#include <stdio.h>
#include <string.h>

#include "vsl_syscall.h"
#include "vsl_syscall_numbers.h"
#include "vsl_nt_bridge.h"
#include "../wubu_kernel_xchg.h"

extern int vsl_nt_bridge_init(vsl_nt_bridge_ctx_t *ctx) __attribute__((weak));
extern int64_t vsl_nt_syscall_dispatch(vsl_nt_bridge_ctx_t *, uint16_t,
                                       uint64_t *, int) __attribute__((weak));

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== vsl_kernel_adapt ===\n");
    uint64_t regs[6] = {0, 0, 0, 0, 0, 0};
    const char *by = NULL;

    CHECK(vsl_kernel_interchange_init() == 0, "the interchange initialises");
    CHECK(wubu_kernel_count() == 5, "five real kernels registered");
    CHECK(strcmp(wubu_kernel_active(), "linux") == 0, "linux is the default kernel");

    /* the four class-bit ranges still reach the same dispatchers */
    int64_t r = vsl_kernel_interchange_dispatch(VSL_SYS_GETPID, regs, &by);
    CHECK(by && strcmp(by, "linux") == 0, "a bare syscall still reaches linux");
    (void)r;

    by = NULL;
    vsl_kernel_interchange_dispatch(0xC0000100u, regs, &by);   /* cpm fn 1 */
    CHECK(by && strcmp(by, "cpm") == 0, "0xC0 class reaches cpm");

    by = NULL;
    vsl_kernel_interchange_dispatch(0xB0000004u, regs, &by);   /* macclassic trap 4 */
    CHECK(by && strcmp(by, "macclassic") == 0, "0xB0 class reaches macclassic");

    by = NULL;
    vsl_kernel_interchange_dispatch(0x01000001u, regs, &by);   /* macOS */
    CHECK(by && strcmp(by, "macos") == 0, "a high-byte class reaches macos");

    /* selection changes who owns the bare range -- but only a link that
     * carries the NT bridge can actually answer for NT; otherwise the call
     * falls through to linux, which is the designed behaviour. */
    if (vsl_nt_bridge_init && vsl_nt_syscall_dispatch) {
        CHECK(wubu_kernel_select("nt") == 0, "nt can be selected");
        by = NULL;
        vsl_kernel_interchange_dispatch(VSL_SYS_GETPID, regs, &by);
        CHECK(by && strcmp(by, "nt") == 0, "the bare range follows the selected kernel");
        CHECK(wubu_kernel_select("linux") == 0, "back to linux");
        by = NULL;
        vsl_kernel_interchange_dispatch(VSL_SYS_GETPID, regs, &by);
        CHECK(by && strcmp(by, "linux") == 0, "linux owns the bare range again");
    } else {
        /* The SELECTED kernel is authoritative for the bare range: with no
         * bridge linked, NT cannot answer, and the call is UNHANDLED rather
         * than silently answered by a kernel that was moved out of charge. */
        CHECK(wubu_kernel_select("nt") == 0, "nt is registered even without its bridge");
        by = NULL;
        int64_t r2 = vsl_kernel_interchange_dispatch(VSL_SYS_GETPID, regs, &by);
        CHECK(by == NULL && r2 == WUBU_KERNEL_ENOSYS,
              "the selected kernel is authoritative: no bridge means unhandled");
        CHECK(wubu_kernel_select("linux") == 0, "reselect linux");
        by = NULL;
        vsl_kernel_interchange_dispatch(VSL_SYS_GETPID, regs, &by);
        CHECK(by && strcmp(by, "linux") == 0, "linux answers again once reselected");
    }

    /* payload routing through the real probes */
    CHECK(strcmp(wubu_kernel_who_execs("a.elf", NULL, 0), "linux") == 0, "elf -> linux");
    CHECK(strcmp(wubu_kernel_who_execs("a.exe", NULL, 0), "nt") == 0, "pe -> nt");
    CHECK(strcmp(wubu_kernel_who_execs("a.macho", NULL, 0), "macos") == 0, "macho -> macos");

    /* the network state is publishable */
    char buf[1024];
    CHECK(wubu_kernel_xchg_dump(buf, sizeof(buf)) == 0, "dump written");
    CHECK(strstr(buf, "kernel nt ") != NULL, "dump lists nt");
    CHECK(strstr(buf, "kernel cpm ") != NULL, "dump lists cpm");

    if (failures == 0) printf("ALL VSL_KERNEL_ADAPT TESTS PASSED\n");
    else printf("%d VSL_KERNEL_ADAPT FAILURES\n", failures);
    return failures ? 1 : 0;
}
