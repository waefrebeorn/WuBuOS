/* tools/wemu_run_elf.c -- run one guest ELF in WEMU and report its exit status.
 *
 * The two halves of this tree meet here: WuBuNOS's holyd_emit_elf produces an
 * ELF, WEMU's user-mode path loads and executes ELFs (it already runs a static
 * busybox). Nothing connected the two ends until this tool:
 *
 *     holyd_emit_elf prog.hc prog.elf      (WuBuNOS)
 *     wemu_run_elf prog.elf                (WEMU)
 *
 * Usage: wemu_run_elf <path> [args...]
 * Prints the guest's stdout/stderr as it is captured, then exits with the
 * guest's exit code (or 125 when the image could not be run at all, which is
 * deliberately a different number from any guest exit).
 */
#define _POSIX_C_SOURCE 200809L
#include "wemu_usermode.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: wemu_run_elf <path> [args...]\n");
        return 125;
    }
    WemuUmResult res;
    int rc = wemu_um_run_elf(argv[1], &argv[1], NULL, &res);
    if (rc != 0) {
        fprintf(stderr, "wemu_run_elf: %s could not run (wemu_um_run_elf=%d)\n",
                argv[1], rc);
        return 125;
    }
    /* WemuUmResult captures fd 1 and 2 together in `out` (bounded, NUL
     * terminated when it is shorter than OUT_MAX). */
    if (res.out_len) fwrite(res.out, 1, res.out_len, stdout);
    return res.exit_code & 0xFF;
}
