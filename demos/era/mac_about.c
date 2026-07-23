/*
 * mac_about.c -- Classic Mac OS 68K era demo (1984)  [EMULATOR GAP -- not runnable yet]
 *
 * This is the Classic Mac "About" demo WuBuOS *would* run through the
 * 0xB0 68K A-line personality (vsl_macclassic_syscall_dispatch). It draws
 * an "About" string to the (headless) screen via the A-line trap A88B
 * (WriteChar) and would exercise the Classic Mac personality's real host
 * effects (NewPtr/DisposePtr, NewHandle/DisposeHandle, Open/Read/Write/Delete,
 * TickCount) already implemented in vsl_syscall_macclassic.c.
 *
 * WHY IT IS A GAP (honest):
 *   * WuBuOS has the Classic Mac 68K A-LINE SYSCALL PERSONALITY (real host
 *     effects), but NO Motorola 68000 CPU EMULATOR. A Mac binary is 68K
 *     machine code; without a 68K interpreter to fetch/decode/execute the
 *     A-line traps, the binary cannot run.
 *   * No 68K cross-toolchain (e.g. m68k-elf-gcc / vbcc) is installed here to
 *     emit a Mac binary.
 *   * Building this requires: (1) a 68K interpreter, (2) a Mac Binary/
 *     resource-fork loader, (3) a 68K toolchain. See
 *     docs/ERA_APPS_AND_RESOLVE_GAPS.md "Classic Mac personality".
 *
 * The A-line trap the workload would issue (for when the emu lands):
 *   WriteChar('A'):  move.w #'A', -(sp); .word 0xA88B   ; _WriteChar
 *   The WuBuOS VSL router maps class 0xB0 + trap# (low 12 bits) to
 *   vsl_macclassic_syscall_dispatch(); once a 68K interpreter exists, this
 *   binary will run end-to-end through that router.
 *
 * (Left as C source documenting the intended workload; it is NOT compiled
 * here -- there is no 68K target. It is referenced by the era-apps registry
 * as a documented, non-runnable gap so the desktop shows it greyed.)
 */
void mac_about_demo(void) {
    /* Would emit, via 68K A-line traps:
     *   _WriteChar('W'); _WriteChar('u'); ... _WriteChar('\n');
     * backed by vsl_macclassic_syscall_dispatch(A88B, ...). */
}
