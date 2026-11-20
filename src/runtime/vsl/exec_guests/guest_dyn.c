/* Real ET_DYN (.so) fixture for the VSL dynamic-ELF differential gate.
 *
 * guest_ptr is a static pointer to a local string.  Because it is built
 * -shared -fPIC, the linker emits exactly one R_X86_64_RELATIVE relocation
 * for this slot (verified with readelf).  At load time VSL must apply
 *   *guest_ptr = load_bias + addend
 * and _start (in guest_dyn_start.S) writes the string through guest_ptr.
 * No libc, no external symbols, no GLOB_DAT/JUMP_SLOT. */
__attribute__((visibility("hidden"))) const char guest_msg[] = "VSL-DYN-SO\n";
__attribute__((visibility("hidden"))) void *guest_ptr = (void *)guest_msg;
