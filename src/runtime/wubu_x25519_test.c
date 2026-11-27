/* wubu_x25519_test.c -- the slerm gate for SLERM-2023.
 *
 * Oracles are Python's `cryptography` and RFC 7748 itself: both section 5.2 key pairs,
 * the published shared secret, four random pairs, and the iterated 1/1000 vectors that
 * exist precisely to catch a ladder that is only right for one input shape.
 *
 * A low-order peer point yields an all-zero secret; that is a refusal, not a key, and
 * the gate asserts the output is zeroed so a caller cannot use it by accident.
 */
#include "wubu_x25519.h"
#include <stdio.h>
#include <string.h>

static int pass, fail;

static int unhex(const char *h, unsigned char *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned v = 0;
        for (int k = 0; k < 2; k++) {
            char c = h[i*2+k];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else return 0;
        }
        out[i] = (unsigned char)v;
    }
    return 1;
}

typedef struct { const char *name; const char *scalar; const char *point; const char *expect; } Vec;

static const Vec VECS[] = {
    { "rfc-alice", "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a",
                   "0900000000000000000000000000000000000000000000000000000000000000",
                   "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a" },
    { "rfc-bob",   "5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb",
                   "0900000000000000000000000000000000000000000000000000000000000000",
                   "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f" },
    { "rfc-shared","77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a",
                   "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f",
                   "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742" },
    { "4dc8646fb5aef2c2a39bf6e91822998c30652f1a34ceee7584dda20b97ba9882", "452c9b14ed2e5e2631c03a661039fbe2b09937966d2fd5491f4ed6b003c95445", "f420c9f357baa43835cb9d971fcc9b1dc554f71ffc0fc303f7d62157a0c38c12", "9a5d7e15636fbf8ea2444429cf440c038e85a57235ba5e3d0e005c6a5d06d51e" },
    { "5f8435ee0bd7f4c6d87abdd5f5c5e851e7b59f61d6df3a09a684d559d10018aa", "2a47ae125c6e7eb175d00dc03b31ca3faf7a91e4b67226ab9301e8d0ad3cfc3e", "89fad45abb11db1d47191d604618fd0997af6694adce35211ffb05c51dc07657", "86ad1976e1727d4aee37cb585b838139371fe2b88619f3da1c29d0c1eeae3852" },
    { "1f6ca35d21f69254d4c76c7d71d673168956465fc266f1517484b775cbb06283", "e6d915de5aac492a22a474e8292166f8fb6bdcfb20ffa849d517196216823d9e", "f99089fb7d7f7e6fddc251b81be8bfb438f8a48dfd04b7bf7300ee266abf416b", "c7ee687dd5abb7d718957f20b7fa47f412d837f7fde2439efb22e00a26bb4207" },
    { "b576824662aa8ccf847f788645ffacc39f69ae643fa0132617eceef8bc7b732d", "cc4fd58e15bdd8b58635cc54de6ff34a4c97b9e3ad31253eb8e62d759ad891d6", "6a240ecf32c0ac7d24da9b825a5e7c0d418c4a67c2b32b3dbc27b400d7fa545d", "e75efb676b41ee04816a41a0871e96171ec6134d74ba635aeaae15168961254e" },
};

static void ok_hex(const char *name, const char *scalar, const char *point, const char *want) {
    unsigned char s[32], p[32], out[32], w[32];
    if (!unhex(scalar, s, 32) || !unhex(point, p, 32) || !unhex(want, w, 32)) {
        printf("  [FAIL] %s: bad vector\n", name); fail++; return;
    }
    int rc = wubu_x25519(out, s, p);
    if (rc == WUBU_X25519_OK && !memcmp(out, w, 32)) pass++;
    else {
        printf("  [FAIL] %-10s rc=%d got ", name, rc);
        for (int i = 0; i < 32; i++) printf("%02x", out[i]);
        printf("\n                  want ");
        for (int i = 0; i < 32; i++) printf("%02x", w[i]);
        printf("\n");
        fail++;
    }
}

int main(void) {
    printf("=== wubu_x25519 (slerm gate, SLERM-2023) ===\n");
    for (size_t i = 0; i < sizeof VECS / sizeof VECS[0]; i++)
        ok_hex(VECS[i].name, VECS[i].scalar, VECS[i].point, VECS[i].expect);

    /* RFC 7748 iterated test: k = u = 9, then k = X25519(k, u), u = old k. */
    {
        unsigned char k[32] = {0}, u[32] = {0}, out[32];
        k[0] = 9; u[0] = 9;
        unsigned char want1[32], want1000[32];
        unhex("422c8e7a6227d7bca1350b3e2bb7279f7897b87bb6854b783c60e80311ae3079", want1, 32);
        unhex("684cf59ba83309552800ef566f2f4d3c1c3887c49360e3875f2eb94d99532c51", want1000, 32);
        for (int i = 1; i <= 1000; i++) {
            unsigned char old_k[32];
            memcpy(old_k, k, 32);
            if (wubu_x25519(out, k, u) != WUBU_X25519_OK) { printf("  [FAIL] iterated %d refused\n", i); fail++; break; }
            memcpy(k, out, 32);
            memcpy(u, old_k, 32);
            if (i == 1) { if (!memcmp(k, want1, 32)) pass++; else { printf("  [FAIL] iterated 1\n"); fail++; } }
        }
        if (!memcmp(k, want1000, 32)) pass++; else { printf("  [FAIL] iterated 1000\n"); fail++; }
    }

    /* a low-order point must be refused and the output zeroed */
    {
        unsigned char zero[32] = {0}, out[32], scalar[32];
        memset(out, 0xAA, sizeof out);
        memset(scalar, 7, sizeof scalar);
        int rc = wubu_x25519(out, scalar, zero);
        int zeroed = 1;
        for (int i = 0; i < 32; i++) if (out[i]) zeroed = 0;
        if (rc == WUBU_X25519_LOW_ORDER && zeroed) pass++;
        else { printf("  [FAIL] low-order point rc=%d zeroed=%d\n", rc, zeroed); fail++; }
    }

    /* null arguments */
    {
        unsigned char a[32] = {0};
        if (wubu_x25519(NULL, a, a) == WUBU_X25519_ARGS &&
            wubu_x25519(a, NULL, a) == WUBU_X25519_ARGS &&
            wubu_x25519(a, a, NULL) == WUBU_X25519_ARGS) pass++;
        else { printf("  [FAIL] null arguments\n"); fail++; }
    }

    /* clamping is internal: an unclamped scalar and its clamped form agree */
    {
        unsigned char s[32], s2[32], p[32] = {0}, a[32], b[32];
        memset(s, 0xFF, 32);
        memcpy(s2, s, 32);
        s2[0] &= 248; s2[31] &= 127; s2[31] |= 64;
        p[0] = 9;
        int ra = wubu_x25519(a, s, p), rb = wubu_x25519(b, s2, p);
        if (ra == 0 && rb == 0 && !memcmp(a, b, 32)) pass++;
        else { printf("  [FAIL] scalar clamping is not applied\n"); fail++; }
    }

    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
