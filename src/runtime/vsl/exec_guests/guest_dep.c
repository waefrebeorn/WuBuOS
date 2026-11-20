/* Freestanding ET_DYN DEPENDENCY fixture for the VSL DT_NEEDED gate.
 *
 * This is a real gcc -shared object, but it links no libc and is loaded by
 * NO ld.so: VSL maps it as a guest ET_DYN at its own in-window bias, applies
 * its relocations, and merges its exported dynsyms into the bounded symbol
 * table.  Its `write` import resolves from VSL's fixed host table; its
 * vsl_dep_entry export is called by the main guest through a JUMP_SLOT.
 *
 * The marker is written BEFORE the main guest's own output, so the gate can
 * prove real control flow: dependency loaded -> relocations applied -> main
 * GOT resolved -> dependency code executed -> main continued. */
extern long write(long fd, const void *buf, unsigned long len);

unsigned long vsl_dep_entry(void) {
    write(1, "VSLDEP-CALLED\n", 14);
    return 0x56534445504d4141ULL;                /* "VSDPMAA" */
}
