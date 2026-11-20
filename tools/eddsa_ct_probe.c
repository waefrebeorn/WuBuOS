/* tools/eddsa_ct_probe.c -- constant-time EVIDENCE probe (not a proof).
 *
 * Drives the REAL production ladder core in src/gui/wubu_pkg_ed25519.c with
 * a counter on the constant-selection primitive (ge_cswap).  The counter is
 * compiled in only under WUBU_EDDSA_CT_PROBE, which is never present in a
 * product build.  Scalar classes cover the extremes of Hamming weight and
 * bit length; a data-dependent branch or trip count would change the count.
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "wubu_pkg_sign.h"

void wubu_pkg_sign_set_err(char *err, size_t errsz, const char *msg) {
    if (err && errsz) {
        strncpy(err, msg, errsz - 1);
        err[errsz - 1] = '\0';
    }
}

int  wubu_pkg_ed25519_ct_ladder_run(const char *scalar_hex, char *err, size_t errsz);
unsigned long wubu_pkg_ed25519_ct_cswap_calls(void);

int main(void) {
    static const struct { const char *name; const char *hex; } cases[] = {
        { "all-zero",   "0000000000000000000000000000000000000000000000000000000000000000" },
        { "all-ones",   "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff" },
        { "top-bit",    "0000000000000000000000000000000000000000000000000000000000000080" },
        { "alternating","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" },
        { "single-bit", "0000000000000000000000000000000000000000000000000000000000000001" },
        { "low-order",  "0100000000000000000000000000000000000000000000000000000000000000" },
    };
    unsigned long expected = 0;
    int fails = 0;
    printf("=== Ed25519 ladder constant-selection evidence ===\n");
    printf("%-14s %12s %s\n", "SCALAR_CLASS", "CSWAP_CALLS", "RESULT");
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char err[256] = {0};
        int rc = wubu_pkg_ed25519_ct_ladder_run(cases[i].hex, err, sizeof(err));
        unsigned long n = wubu_pkg_ed25519_ct_cswap_calls();
        if (i == 0) expected = n;
        int stable = (rc == 0 && n == expected);
        printf("%-14s %12lu %s%s\n", cases[i].name, n,
               stable ? "STABLE" : "UNSTABLE", stable ? "" : err);
        if (!stable) ++fails;
    }
    printf("EXPECTED_SWAPS=%lu (256 ladder iterations + 1 final undo)\n", expected);
    if (fails) {
        printf("[ct] FAIL: conditional-selection count varied with the secret scalar\n");
        return 1;
    }
    printf("[ct] PASS: selection count invariant across all secret-scalar classes\n");
    return 0;
}
