/* probe.c -- CLI wrapper around the shipped bignum-free ladder for the gate.
 * argv[1] = scalar as a big-endian 64-hex-digit integer; argv[2], if present,
 * = point as the literal 32-byte little-endian hex encoding. */
#include <stdio.h>
#include <string.h>
#include "wubu_ed25519_fixed.h"

static int hx(const char *s, unsigned char *o, int reverse) {
    if (strlen(s) != 64) return -1;
    for (int i = 0; i < 32; i++) {
        int j = reverse ? (31 - i) : i;
        unsigned v;
        if (sscanf(s + 2 * j, "%2x", &v) != 1) return -1;
        o[i] = (unsigned char)v;
    }
    return 0;
}

int main(int argc, char **argv) {
    unsigned char s[32], p[32], o[32];
    if (argc < 2 || hx(argv[1], s, 1) != 0) return 2;
    int rc;
    if (argc > 2) {
        if (hx(argv[2], p, 0) != 0) return 2;
        rc = wubu_ed25519_fixed_scalarmult(o, s, p);
    } else {
        rc = wubu_ed25519_fixed_base_scalarmult(o, s);
    }
    if (rc != 0) { printf("ERR %d\n", rc); return 1; }
    for (int i = 0; i < 32; i++) printf("%02x", o[i]);
    printf("\n");
    return 0;
}
