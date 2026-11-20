/* Dependency for the per-image search-directory gate.  Built into dirA/
 * with the SAME soname as the dirB copy, but a different exported magic.
 * A per-image dir list decides which copy is loaded; no global allow-list
 * is involved.  No libc, no ld.so. */
unsigned long vsl_imgdep_magic(void) {
    return 0x4449524100000001ULL;                  /* "DIRA" + 1 */
}
