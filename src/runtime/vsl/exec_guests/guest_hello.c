/*
 * guest_hello.c -- REAL dynamically linked glibc hello-world fixture.
 *
 * Built by mk/tests.mk with plain gcc (no -static/-nostdlib/-nostartfiles):
 * it has PT_INTERP=/lib64/ld-linux-x86-64.so.2, DT_NEEDED libc.so.6 and
 * versioned imports puts@GLIBC_2.2.5 / __libc_start_main@GLIBC_2.34.  VSL's
 * bounded loader maps the real ld.so from the guest rootfs (the ld.so path)
 * and binds these main-image imports from a bounded guest libc shim, then
 * runs the guest to a real exit(0) with the exact output below.
 */
#include <stdio.h>

int main(void) {
    puts("VSL-HELLO-DYNAMIC-WORLD");
    return 0;
}
