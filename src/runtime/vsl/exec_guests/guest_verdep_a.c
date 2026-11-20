/* Freestanding ET_DYN dependency A for the VSL symbol-versioning gate.
 *
 * Exports the WRONG version of vsl_ver_entry as the DEFAULT, STRONG binding
 * (VSL_1).  A pre-change base-name/first-wins loader picks this one; the
 * version-aware VSL loader must instead match the requested VSL_2 from dep B.
 * Also exports a WEAK vsl_weakpick@VSL_1 that a first-wins loader would pick
 * ahead of dep B's strong definition.  No libc, no ld.so. */
unsigned long vsl_ver_entry_v1(void) {
    return 0x4141523100000001ULL;                /* "AAR1" + 1: wrong version */
}

unsigned long vsl_weakpick_weak(void) __attribute__((weak));
unsigned long vsl_weakpick_weak(void) {
    return 0x5745414b00000001ULL;                /* "WEAK" + 1: weak definition */
}

__asm__(".symver vsl_ver_entry_v1,vsl_ver_entry@@VSL_1");
__asm__(".symver vsl_weakpick_weak,vsl_weakpick@@VSL_1");
