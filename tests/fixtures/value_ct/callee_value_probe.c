/*
 * callee_value_probe.c -- injectable secret-VALUE-dependence probe for the
 * bignum value-independence auditor (tools/eddsa_bignum_value_check.py).
 *
 * `struct bn` mirrors the parts of mbedtls_mpi the ladder touches: a limb
 * count and a limb array behind a pointer.  Both bn_ct_limb_select and
 * bn_branchy_limb_select are compiled at -O2 with the same local compiler.
 * With WUBU_INJECT_VALUE_BRANCH defined, the second function gains a branch
 * on a SECRET-DERIVED LIMB VALUE -- the negative control the auditor must
 * flag.  Without it, that function is a branch-free mask/XOR select and must
 * produce no secret-governed site.
 *
 * This is a fixture, never a substitute for the shipped libmbedcrypto.a: the
 * shipped-object audit is a separate leg of the gate.
 */
struct bn {
    unsigned long n;
    unsigned long p[8];
    int s;
};

int bn_ct_limb_select(struct bn *out, const struct bn *a, const struct bn *b)
{
    unsigned long x = a->p[0];
    unsigned long y = b->p[0];
    unsigned long d = x - y;
    unsigned long m = (unsigned long)(-(long)(d >> 63));
    out->p[0] = (m & x) | (~m & y);
    return 0;
}

int bn_branchy_limb_select(struct bn *out, const struct bn *a, const struct bn *b)
{
#ifdef WUBU_INJECT_VALUE_BRANCH
    /* INJECTED SECRET-DEPENDENT BRANCH IN THE CALLEE */
    if (a->p[0] > b->p[0]) {
        out->p[0] = a->p[0];
        return 1;
    }
    out->p[0] = b->p[0];
    return 0;
#else
    unsigned long x = a->p[0];
    unsigned long y = b->p[0];
    unsigned long d = x - y;
    unsigned long m = (unsigned long)(-(long)(d >> 63));
    out->p[0] = (m & x) | (~m & y);
    return 0;
#endif
}
