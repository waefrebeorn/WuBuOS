/* Dependency for the per-image search-directory gate.  Built into dirB/
 * with the SAME soname as the dirA copy, but a different exported magic. */
unsigned long vsl_imgdep_magic(void) {
    return 0x4449524200000002ULL;                  /* "DIRB" + 2 */
}
