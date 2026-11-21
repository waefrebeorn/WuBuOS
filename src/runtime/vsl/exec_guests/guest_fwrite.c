/*
 * guest_fwrite.c -- REAL dynamically linked glibc fwrite fixture.
 *
 * Built by mk/tests.mk with plain gcc (no -static/-nostdlib/-nostartfiles),
 * exactly like guest_hello.c: PT_INTERP=/lib64/ld-linux-x86-64.so.2,
 * DT_NEEDED libc.so.6, versioned imports fwrite@GLIBC_2.2.5 (JUMP_SLOT),
 * stdout@GLIBC_2.2.5 (COPY), __libc_start_main@GLIBC_2.34 (GLOB_DAT) and
 * __cxa_finalize@GLIBC_2.2.5 (GLOB_DAT).  This is the printf-format-free
 * stream-write shape: no format string, a fixed byte buffer and an explicit
 * item count.  With the bounded stdout/FILE stream model ON, VSL selects the
 * bounded host fwrite writer by the exact
 * (fwrite, GLIBC_2.2.5, libc.so.6) triple and emits the exact buffer bytes to
 * fd 1 -- byte-exact against a native build of this same source.  No guest
 * FILE state is touched, so the exit flush has nothing pending.
 */
#include <stdio.h>

int main(void) {
    static const char msg[] = "VSL-FWRITE-STREAM-WORLD";
    fwrite(msg, 1, sizeof(msg) - 1, stdout);
    return 0;
}
