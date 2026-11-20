/* Freestanding ET_DYN dependency B for the VSL symbol-versioning gate.
 *
 * Exports the requested vsl_ver_entry@VSL_2 as a HIDDEN, WEAK alias: exact
 * version-name matching must still select it over dep A's default strong
 * VSL_1 definition.  Also exports a STRONG vsl_weakpick@VSL_1 that must beat
 * dep A's weak definition despite A being first in DT_NEEDED order.
 * No libc, no ld.so. */
unsigned long vsl_ver_entry_v2(void) __attribute__((weak));
unsigned long vsl_ver_entry_v2(void) {
    return 0x4242523200000002ULL;                /* "BBR2" + 2: wanted version */
}

unsigned long vsl_weakpick_strong(void) {
    return 0x5354524f00000002ULL;                /* "STRO" + 2: strong definition */
}

__asm__(".symver vsl_ver_entry_v2,vsl_ver_entry@VSL_2");
__asm__(".symver vsl_weakpick_strong,vsl_weakpick@VSL_1");
