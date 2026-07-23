/*
 * holyc_runtime.c  --  WuBuOS HolyC personality runtime (host effects)
 *
 * Implements the small set of HolyC / TempleOS library functions the
 * era demo and the desktop HolyC terminal actually call:
 *   - Print(const char *)        -> host stdout
 *   - FpWriteFile(name, contents) -> host file create+write
 *
 * These are REAL host operations, not stubs. They are registered as extern
 * C functions with the HolyC JIT (see holyc_codegen_api.c) so a compiled
 * `Print("...")` / `FpWriteFile(...)` becomes a real host call instead of a
 * null-pointer `call 0` (the previous behaviour, which SIGSEGV'd).
 *
 * C11, self-contained; depends only on holyc_codegen.h for the registration
 * API. ABI: System V AMD64 (args in rdi, rsi, ...; result in rax).
 */

#include "holyc_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- HolyC runtime host functions --------------------------------- */

/* Print(const char *s) -- emit a NUL-terminated string to stdout. */
int64_t wubu_print(const char *s) {
    if (!s) return -1;
    fputs(s, stdout);
    fflush(stdout);
    return (int64_t)strlen(s);
}

/* FpWriteFile(const char *name, const char *contents) -- create/truncate
 * the named file and write the contents into it. Returns byte count on
 * success, -1 on failure. */
int64_t wubu_fp_write_file(const char *name, const char *contents) {
    if (!name || !contents) return -1;
    FILE *f = fopen(name, "wb");
    if (!f) return -1;
    size_t len = strlen(contents);
    size_t got = fwrite(contents, 1, len, f);
    fclose(f);
    return (got == len) ? (int64_t)len : -1;
}

/* -- Extern registration ------------------------------------------ */

/* Register the HolyC personality runtime functions with a codegen context
 * so the JIT resolves `Print` / `FpWriteFile` to real host addresses. */
void hc_register_holyc_runtime(HCGen *gen) {
    if (!gen) return;
    if (gen->n_extern_funcs >= 32) return;

    gen->extern_funcs[gen->n_extern_funcs].c_name[0] = '\0';
    strncpy(gen->extern_funcs[gen->n_extern_funcs].c_name, "Print",
            HC_MAX_IDENT_LEN - 1);
    gen->extern_funcs[gen->n_extern_funcs].c_name[HC_MAX_IDENT_LEN - 1] = '\0';
    gen->extern_funcs[gen->n_extern_funcs].func_addr = (void *)wubu_print;
    gen->n_extern_funcs++;

    if (gen->n_extern_funcs >= 32) return;
    strncpy(gen->extern_funcs[gen->n_extern_funcs].c_name, "FpWriteFile",
            HC_MAX_IDENT_LEN - 1);
    gen->extern_funcs[gen->n_extern_funcs].c_name[HC_MAX_IDENT_LEN - 1] = '\0';
    gen->extern_funcs[gen->n_extern_funcs].func_addr = (void *)wubu_fp_write_file;
    gen->n_extern_funcs++;
}
