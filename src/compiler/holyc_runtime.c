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

/* -- libc string/memory functions the kernel + desktop C code calls.
 * These are real host libc calls, registered so a compiled `strlen(s)`,
 * `strcmp(a,b)`, `memcpy(d,s,n)`, `memset(p,v,n)` resolves to the actual
 * libc symbol instead of the unresolved trap. Each wrapper keeps the SysV
 * register ABI (rdi/rsi/rdx/rcx, result in rax) with a cdecl-clean
 * signature. */

int64_t wubu_strlen(const char *s)    { return (int64_t)strlen(s); }
int64_t wubu_strcmp(const char *a, const char *b) { return strcmp(a, b); }
int64_t wubu_strcpy(char *d, const char *s) { return (int64_t)(uintptr_t)strcpy(d, s); }
int64_t wubu_memcpy(void *d, const void *s, size_t n) {
    memcpy(d, s, n); return (int64_t)(uintptr_t)d;
}
int64_t wubu_memset(void *p, int v, size_t n) {
    memset(p, v, n); return (int64_t)(uintptr_t)p;
}
int64_t wubu_memcmp(const void *a, const void *b, size_t n) {
    return memcmp(a, b, n);
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

    /* -i_make_shit_code: every language's favorite function, registered
     * so ANY submitted language's most common call resolves instead of
     * a null-pointer SIGSEGV (we ballin). */
    if (gen->n_extern_funcs >= 32) return;
    strncpy(gen->extern_funcs[gen->n_extern_funcs].c_name, "print",
            HC_MAX_IDENT_LEN - 1);
    gen->extern_funcs[gen->n_extern_funcs].c_name[HC_MAX_IDENT_LEN - 1] = '\0';
    gen->extern_funcs[gen->n_extern_funcs].func_addr = (void *)wubu_print;
    gen->n_extern_funcs++;

    if (gen->n_extern_funcs >= 32) return;
    strncpy(gen->extern_funcs[gen->n_extern_funcs].c_name, "puts",
            HC_MAX_IDENT_LEN - 1);
    gen->extern_funcs[gen->n_extern_funcs].c_name[HC_MAX_IDENT_LEN - 1] = '\0';
    gen->extern_funcs[gen->n_extern_funcs].func_addr = (void *)wubu_print;
    gen->n_extern_funcs++;

    /* libc string/memory functions every kernel driver + the desktop use. */
    struct { const char *name; void *addr; } const libc[] = {
        { "strlen",  (void *)wubu_strlen  },
        { "strcmp",  (void *)wubu_strcmp  },
        { "strcpy",  (void *)wubu_strcpy  },
        { "memcpy",  (void *)wubu_memcpy  },
        { "memset",  (void *)wubu_memset  },
        { "memcmp",  (void *)wubu_memcmp  },
    };
    for (size_t i = 0; i < sizeof(libc) / sizeof(libc[0]); i++) {
        if (gen->n_extern_funcs >= 32) return;
        strncpy(gen->extern_funcs[gen->n_extern_funcs].c_name, libc[i].name,
                HC_MAX_IDENT_LEN - 1);
        gen->extern_funcs[gen->n_extern_funcs].c_name[HC_MAX_IDENT_LEN - 1] = '\0';
        gen->extern_funcs[gen->n_extern_funcs].func_addr = libc[i].addr;
        gen->n_extern_funcs++;
    }
}
