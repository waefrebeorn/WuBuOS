/* NEGATIVE CONTROL fixture for the assembly-level branchy-cswap detector.
 *
 * A secret-dependent conditional swap: the `swap` byte (SysV arg1 = dil/edi)
 * guards a real `je`.  The gate compiles this at -O2, disassembles it, and
 * requires tools/eddsa_callee_asm_check.py --expect leak to flag the secret-
 * governed jump.  This proves the assembly auditor is not vacuous.  Never
 * linked into a product build.
 */
int branchy_cswap(unsigned char swap, unsigned long *a, unsigned long *b)
{
    if (swap) {
        unsigned long t = *a;
        *a = *b;
        *b = t;
    }
    return 0;
}
