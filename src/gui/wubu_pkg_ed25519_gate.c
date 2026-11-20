/* wubu_pkg_ed25519_gate.c -- opt-in differential gate harness.
 * Built only by tools/pkg_ed25519_gate.sh with WUBU_HAVE_MBEDTLS.
 * Proves a real Ed25519 signature verifies and that every theatrical or
 * tampered substitute is rejected by the same verification path.
 */
#include "wubu_pkgmgr.h"
#include "wubu_pkgmgr_internal.h"
#include "wubu_pkg_sign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    rewind(f);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

static bool spill(const char *path, const uint8_t *buf, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    size_t w = fwrite(buf, 1, len, f);
    fclose(f);
    return w == len;
}

/* Exact FNV-1a placeholder from the pre-Ed25519 writer, for the negative
 * control: a hash-shaped signature must NOT be accepted as Ed25519. */
static void legacy_fnv_signature(const uint8_t *hdr, size_t sig_off,
                                 const uint8_t *manifest, size_t mlen,
                                 const uint8_t *payload, size_t plen,
                                 uint32_t crc, uint8_t out[64]) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < sig_off; i++) { hash ^= hdr[i]; hash *= 0x100000001b3ULL; }
    for (size_t i = 0; i < mlen; i++) { hash ^= manifest[i]; hash *= 0x100000001b3ULL; }
    for (size_t i = 0; i < plen; i++) { hash ^= payload[i]; hash *= 0x100000001b3ULL; }
    memcpy(out, &hash, sizeof(hash));
    hash ^= (uint64_t)crc;
    hash *= 0x100000001b3ULL;
    memcpy(out + 8, &hash, sizeof(hash));
    for (int i = 16; i < 64; i++)
        out[i] = (uint8_t)(hash >> ((i % 8) * 8)) ^ (uint8_t)i;
}

static int expect(const char *label, bool got, bool want) {
    bool pass = got == want;
    printf("[gate] %-34s %s (verify returned %s)\n", label,
           pass ? "PASS" : "FAIL", got ? "true" : "false");
    return pass ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: %s <pkg> <srcdir> <seed_hex> <pub_hex> <wrong_pub_hex>\n", argv[0]);
        return 2;
    }
    const char *pkg = argv[1], *srcdir = argv[2];
    const char *seed = argv[3], *pub = argv[4], *wrong_pub = argv[5];

    wubu_pkg_manifest_t m;
    memset(&m, 0, sizeof(m));
    snprintf(m.id, sizeof(m.id), "ed25519-gate");
    snprintf(m.name, sizeof(m.name), "Ed25519 Gate App");
    snprintf(m.version, sizeof(m.version), "1.0.0");
    snprintf(m.description, sizeof(m.description), "differential signature gate");
    snprintf(m.license, sizeof(m.license), "MIT");
    m.arch = WUBU_PKG_ARCH_X86_64;
    m.payload_type = WUBU_PKG_PAYLOAD_DATA;
    snprintf(m.sandbox_profile, sizeof(m.sandbox_profile), "default");
    snprintf(m.files[0].src, sizeof(m.files[0].src), "payload.txt");
    snprintf(m.files[0].dst, sizeof(m.files[0].dst), "payload.txt");
    m.files[0].mode = 0644;
    m.n_files = 1;

    int fails = 0;
    size_t hdr_len = sizeof(wubu_pkg_header_t);
    size_t sig_off = offsetof(wubu_pkg_header_t, signature);

    /* Invalid key material must be rejected at creation time. */
    fails += expect("bad seed rejected at sign",
                    wubu_pkgmgr_create_package(srcdir, "/tmp/wubu_bad_seed.wubu",
                                               &m, "deadbeef"), false);
    remove("/tmp/wubu_bad_seed.wubu");

    if (!wubu_pkgmgr_create_package(srcdir, pkg, &m, seed)) {
        fprintf(stderr, "[gate] FAIL: could not create a signed package\n");
        return 1;
    }
    printf("[gate] created signed package: %s\n", pkg);

    /* 1. The genuine signature verifies against the matching public key. */
    fails += expect("valid signed package", wubu_pkgmgr_verify_package(pkg, pub), true);

    size_t total = 0;
    uint8_t *orig = slurp(pkg, &total);
    if (!orig || total < hdr_len + 16) {
        fprintf(stderr, "[gate] FAIL: cannot read package\n");
        return 1;
    }
    wubu_pkg_header_t *h = (wubu_pkg_header_t *)orig;
    size_t m_off = hdr_len, p_off = hdr_len + h->manifest_size;
    size_t m_len = h->manifest_size, p_len = total - p_off;

    /* 2. Tampered payload: flip a byte in the compressed payload. */
    {
        uint8_t *t = (uint8_t *)malloc(total);
        memcpy(t, orig, total);
        t[p_off + 16] ^= 0x5a;
        if (!spill(pkg, t, total)) return 1;
        fails += expect("tampered payload rejected", wubu_pkgmgr_verify_package(pkg, pub), false);
        free(t);
    }

    /* 3. Tampered signature: flip a single signature byte. */
    {
        uint8_t *t = (uint8_t *)malloc(total);
        memcpy(t, orig, total);
        t[sig_off + 7] ^= 0x01;
        if (!spill(pkg, t, total)) return 1;
        fails += expect("tampered signature rejected", wubu_pkgmgr_verify_package(pkg, pub), false);
        free(t);
    }

    /* 4. Negative control: the old FNV placeholder is not an Ed25519 sig. */
    {
        uint8_t *t = (uint8_t *)malloc(total);
        memcpy(t, orig, total);
        memset(t + sig_off, 0, WUBU_PKG_SIG_SIZE);
        legacy_fnv_signature(t, sig_off, t + m_off, m_len,
                             t + p_off, p_len, h->crc32, t + sig_off);
        if (!spill(pkg, t, total)) return 1;
        fails += expect("negative control: legacy FNV rejected",
                        wubu_pkgmgr_verify_package(pkg, pub), false);
        free(t);
    }

    /* 5. Unsigned package (zero signature) is rejected. */
    {
        uint8_t *t = (uint8_t *)malloc(total);
        memcpy(t, orig, total);
        memset(t + sig_off, 0, WUBU_PKG_SIG_SIZE);
        if (!spill(pkg, t, total)) return 1;
        fails += expect("unsigned package rejected", wubu_pkgmgr_verify_package(pkg, pub), false);
        free(t);
    }

    /* 6. No trust anchor / wrong trust anchor are both rejected. */
    if (!spill(pkg, orig, total)) return 1;
    fails += expect("missing trust anchor rejected", wubu_pkgmgr_verify_package(pkg, NULL), false);
    fails += expect("wrong public key rejected", wubu_pkgmgr_verify_package(pkg, wrong_pub), false);

    /* 6b. Small-subgroup forgery: the identity point (y=1, x=0) makes
     * R + [k]A == R for every challenge, so ANY signature with R=[S]B
     * verifies without the signing key if the cofactor is not checked.
     * The genuine signature above is exactly such an R/S pair. */
    {
        char small_pub[65];
        small_pub[0] = '0'; small_pub[1] = '1';
        memset(small_pub + 2, '0', 62);
        small_pub[64] = '\0';
        /* Forged signature: S=1 and R=[1]B (the RFC 8032 basepoint).
         * With A = identity, [S]B == R == R + [k]A for any challenge. */
        uint8_t *t = (uint8_t *)malloc(total);
        if (!t) return 1;
        memcpy(t, orig, total);
        static const uint8_t base_enc[32] = {
            0x58,0x66,0x66,0x66,0x66,0x66,0x66,0x66,
            0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,
            0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,
            0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66
        };
        memset(t + sig_off, 0, WUBU_PKG_SIG_SIZE);
        memcpy(t + sig_off, base_enc, 32);
        t[sig_off + 32] = 1;                       /* S = 1, little endian */
        if (!spill(pkg, t, total)) return 1;
        fails += expect("small-subgroup public key rejected",
                        wubu_pkgmgr_verify_package(pkg, small_pub), false);
        if (!spill(pkg, orig, total)) return 1;
        free(t);
    }

    /* 7. Restore the pristine signed package and re-verify. */
    fails += expect("pristine package still verifies", wubu_pkgmgr_verify_package(pkg, pub), true);

    /* 8. Install trust anchors: every enabled repo key is tried, not just
     * the first one. A valid package signed by the SECOND configured repo
     * must verify; with no matching key it must be refused. */
    memset(g_pkgmgr.repos, 0, sizeof(g_pkgmgr.repos));
    g_pkgmgr.n_repos = 2;
    g_pkgmgr.repos[0].enabled = true;
    snprintf(g_pkgmgr.repos[0].pubkey, sizeof(g_pkgmgr.repos[0].pubkey), "%s", wrong_pub);
    g_pkgmgr.repos[1].enabled = true;
    snprintf(g_pkgmgr.repos[1].pubkey, sizeof(g_pkgmgr.repos[1].pubkey), "%s", pub);
    fails += expect("second repo trust anchor verifies",
                    wubu_pkgmgr_verify_against_repos(pkg), true);
    snprintf(g_pkgmgr.repos[1].pubkey, sizeof(g_pkgmgr.repos[1].pubkey), "%s", wrong_pub);
    fails += expect("no matching trust anchor rejected",
                    wubu_pkgmgr_verify_against_repos(pkg), false);
    g_pkgmgr.n_repos = 0;

    free(orig);
    printf("[gate] %s (%d failure%s)\n", fails ? "FAILED" : "ALL CHECKS PASSED",
           fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
