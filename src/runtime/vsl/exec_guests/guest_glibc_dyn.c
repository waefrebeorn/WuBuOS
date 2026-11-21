/*
 * guest_glibc_dyn.c -- REAL dynamically linked glibc binary fixture.
 *
 * Built by mk/tests.mk with plain `gcc` (no -static, no -nostdlib).  It has
 * PT_INTERP /lib64/ld-linux-x86-64.so.2, DT_NEEDED libc.so.6, DT_INIT/
 * DT_FINI and GLOB_DAT/JUMP_SLOT imports (__libc_start_main, __cxa_finalize,
 * ...) that the bounded VSL host table does not provide.  The old loader
 * rejects it as ERR_RELOC; the new loader recognises PT_INTERP and refuses a
 * full glibc ld.so run as an explicitly verified negative.
 */
int main(void) { return 0; }
