/* tools/ed25519_native_gate.c -- focused differential harness for the opt-in
 * OpenSSL EdDSA backend.  Built three ways by ed25519_native_gate.sh:
 *   native   : -DWUBU_HAVE_OPENSSL_EDDSA              (OpenSSL core)
 *   ladder   : no macro                               (hardened ladder)
 *   fallback : both -DWUBU_HAVE_OPENSSL_EDDSA and
 *              -DWUBU_EDDSA_FORCE_LADDER              (simulated outage)
 * All three call the SAME wubu_pkg_sign_* entry points on the SAME canonical
 * 32-byte digest.  Output is machine-readable for the shell driver.
 */
#include "wubu_pkg_sign.h"
#include "wubu_pkg_sign_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int from_hex(const char *s, uint8_t *out, size_t n) {
    if (!s || strlen(s) != 2 * n) return -1;
    for (size_t i = 0; i < n; ++i) {
        int hi = hexval(s[2 * i]), lo = hexval(s[2 * i + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

static void print_native_ops(void) {
#ifdef WUBU_HAVE_OPENSSL_EDDSA
    printf("NATIVE_OPS=%u\n", wubu_pkg_ed25519_native_ops());
#else
    printf("NATIVE_OPS=0\n");
#endif
}

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr, "usage: %s <sign|verify> <seed_hex> <pub_hex> <digest_hex> <sig_hex|->\n",
                argv[0]);
        return 2;
    }
    const char *mode = argv[1];
    uint8_t seed[32], pub[32], digest[32], sig[64];
    if (from_hex(argv[2], seed, sizeof(seed)) != 0 ||
        from_hex(argv[3], pub, sizeof(pub)) != 0 ||
        from_hex(argv[4], digest, sizeof(digest)) != 0) {
        fprintf(stderr, "BADARGS\n");
        return 2;
    }
    char err[256];
    err[0] = '\0';

    if (strcmp(mode, "sign") == 0) {
        if (!wubu_pkg_sign_ed25519(digest, argv[2], sig, err, sizeof(err))) {
            printf("SIGN=FAIL ERR=%s\n", err);
            return 1;
        }
        printf("SIGN=OK SIG=");
        for (size_t i = 0; i < sizeof(sig); ++i) printf("%02x", sig[i]);
        printf("\n");
        print_native_ops();
        return 0;
    }

    if (strcmp(mode, "verify") == 0) {
        if (from_hex(argv[5], sig, sizeof(sig)) != 0) {
            printf("RESULT=FAIL ERR=bad_sig_hex\n");
            print_native_ops();
            return 0;
        }
        bool ok = wubu_pkg_sign_verify_ed25519(digest, argv[3], sig, err, sizeof(err));
        printf("RESULT=%s ERR=%s\n", ok ? "PASS" : "FAIL", err[0] ? err : "(none)");
        print_native_ops();
        return 0;  /* outcome is in RESULT=; shell asserts expected value */
    }

    fprintf(stderr, "BADMODE\n");
    return 2;
}
