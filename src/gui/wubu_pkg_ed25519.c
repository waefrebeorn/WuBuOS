/*
 * wubu_pkg_ed25519.c -- RFC 8032 Ed25519 signing/verification leaf.
 * Two mutually exclusive backends share the same entry points and the same
 * canonical 32-byte digest:
 *   WUBU_HAVE_MBEDTLS        -- SHA-512 + bignum ladder (+ optional OpenSSL).
 *   WUBU_HAVE_OPENSSL_EDDSA  -- OpenSSL EVP Ed25519, with the bignum-free
 *                               verification policy in wubu_eddsa_policy.c
 *                               (used when no mbedTLS dev root is present).
 * With WUBU_EDDSA_FIXED_LADDER the mbedTLS bignum backend is replaced as a
 * whole by the self-contained fixed-width path: wubu_ed25519_fixed.c supplies
 * all field/point/scalar arithmetic and wubu_ed25519_fixed_sign.c supplies
 * RFC 8032 sign/verify (SHA-512 from mbedTLS is a hash, not a bignum).  The
 * package-signing gate (tools/pkg_ed25519_gate.sh) selects this path by
 * DEFAULT; the documented legacy override WUBU_EDDSA_FIXED=0 restores the
 * mbedTLS MPI backend.  Source-level builds that define neither macro keep
 * the mbedTLS ladder as the fallback.
 * Never use MBEDTLS_SSL_H as the feature macro: it is mbedTLS's own guard.
 */
#ifdef WUBU_EDDSA_USE_BUILD_CONFIG
/* Auto-detected backend config from tools/detect_native_eddsa.sh.  Included
 * FIRST so the feature macros are visible to wubu_pkg_sign_internal.h. */
#include "native_eddsa_config.h"
#endif
#include "wubu_pkg_sign.h"
#include "wubu_pkg_sign_internal.h"
#include "../runtime/wubu_crypto.h"

#include <string.h>

#ifdef WUBU_EDDSA_FIXED_LADDER
/* Bignum-free fixed-width path (the default in the package-signing gate). */
#include "wubu_ed25519_fixed.h"
#include "wubu_ed25519_fixed_sign.h"
#endif

#ifdef WUBU_HAVE_MBEDTLS
#include <mbedtls/sha512.h>
#if !defined(WUBU_EDDSA_FIXED_LADDER)
/* The fixed path never includes the bignum header, including when an
 * OpenSSL backend is also detected; preferring the fixed path must not
 * silently reintroduce mbedTLS MPI. */
#include <mbedtls/bignum.h>
#endif
#ifdef WUBU_HAVE_OPENSSL_EDDSA
#include <openssl/evp.h>
#endif

#if defined(WUBU_EDDSA_FIXED_LADDER)
/* -- DEFAULT: fully bignum-free EdDSA package-signing path --------------- *
 * All group/scalar arithmetic is in wubu_ed25519_fixed.c; SHA-512 (a hash,
 * not a bignum) is the only mbedTLS primitive used.  No mbedtls_mpi symbol is
 * compiled on this path.  This path takes precedence over the detected
 * OpenSSL backend in the product build and in the package-signing gate.
 * The legacy MPI backend below remains selectable via WUBU_EDDSA_FIXED=0 and
 * is untouched.
 */
static int hex_to_32(const char *hex, uint8_t out[32]) {
    static const char digits[] = "0123456789abcdef";
    if (!hex || !out || strlen(hex) != 64) return -1;
    for (size_t i = 0; i < 32; i++) {
        char c = hex[2 * i], d = hex[2 * i + 1];
        const char *p1 = strchr(digits, c), *p2 = strchr(digits, d);
        if (!p1 || !p2) return -1;
        out[i] = (uint8_t)(((p1 - digits) << 4) | (p2 - digits));
    }
    return 0;
}

bool wubu_pkg_sign_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                           const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    uint8_t seed[32];
    if (!digest || !out_sig || hex_to_32(seed_hex, seed) != 0) {
        wubu_pkg_sign_set_err(err, errsz, "Ed25519 sign needs a 32-byte seed hex (64 chars)");
        return false;
    }
    if (wubu_ed25519_fixed_sign(digest, seed, out_sig) != 0) {
        wubu_pkg_sign_set_err(err, errsz, "Ed25519 sign failed (fixed-width bignum-free path)");
        return false;
    }
    return true;
}

bool wubu_pkg_sign_verify_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                  const char *pubkey_hex,
                                  const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                                  char *err, size_t errsz) {
    uint8_t pub[32];
    if (!digest || !sig || hex_to_32(pubkey_hex, pub) != 0) {
        wubu_pkg_sign_set_err(err, errsz, "Ed25519 verify needs a 32-byte public key hex (64 chars)");
        return false;
    }
    if (wubu_ed25519_fixed_verify(digest, pub, sig) != 0) {
        wubu_pkg_sign_set_err(err, errsz, "Ed25519 signature does not verify (bignum-free policy)");
        return false;
    }
    return true;
}

/* -- Arbitrary-length command signing (revocation CLI) ---------------- */
static bool command_digest(const char *command,
                           uint8_t out[WUBU_PKG_SIGN_DIGEST_LEN]) {
    char hex[2 * WUBU_PKG_SIGN_DIGEST_LEN + 1];
    if (!command || !out) return false;
    wubu_sha256_digest(command, strlen(command), hex, sizeof(hex));
    return hex_to_32(hex, out) == 0;
}

bool wubu_pkg_sign_command(const char *command, const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!command_digest(command, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash command for signing");
        return false;
    }
    return wubu_pkg_sign_ed25519(digest, seed_hex, out_sig, err, errsz);
}

bool wubu_pkg_verify_command(const char *command, const char *pubkey_hex,
                             const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                             char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!command_digest(command, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash command for verification");
        return false;
    }
    return wubu_pkg_sign_verify_ed25519(digest, pubkey_hex, sig, err, errsz);
}

/* -- Domain-separated hashing of arbitrary byte buffers (feed import) -- */
static bool bytes_digest(const uint8_t *data, size_t len, const char *domain,
                         uint8_t out[WUBU_PKG_SIGN_DIGEST_LEN]) {
    if (!out || !domain || !domain[0] || (!data && len)) return false;
    char hdr[128];
    int hn = snprintf(hdr, sizeof(hdr), "%s\n%zu\n", domain, len);
    if (hn < 0 || (size_t)hn >= sizeof(hdr)) return false;
    wubu_SHA256_CTX ctx;
    wubu_sha256_init(&ctx);
    wubu_sha256_update(&ctx, hdr, (size_t)hn);
    if (len) wubu_sha256_update(&ctx, data, len);
    wubu_sha256_final(&ctx, out);
    return true;
}

bool wubu_pkg_sign_bytes(const uint8_t *data, size_t len, const char *domain,
                         const char *seed_hex,
                         uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                         char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!bytes_digest(data, len, domain, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash bytes for signing");
        return false;
    }
    return wubu_pkg_sign_ed25519(digest, seed_hex, out_sig, err, errsz);
}

bool wubu_pkg_verify_bytes(const uint8_t *data, size_t len, const char *domain,
                           const char *pubkey_hex,
                           const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!bytes_digest(data, len, domain, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash bytes for verification");
        return false;
    }
    return wubu_pkg_sign_verify_ed25519(digest, pubkey_hex, sig, err, errsz);
}

#else
/* -- LEGACY mbedTLS bignum backend (WUBU_EDDSA_FIXED=0) ----------------- */

/* -- Ed25519 (RFC 8032) over mbedTLS SHA-512 + bignum -------------- */

/* q = 2^255-19, d = -121665/121666 mod q, sqrt(-1), base point, (q+3)/8.
 * All are public RFC 8032 constants. */
static mbedtls_mpi Q, DD, SQRTM1, BX, BY, EXP8, ONE, L;
static bool constants_ready = false;

static bool mpi_const(mbedtls_mpi *v, const char *hex) {
    return mbedtls_mpi_read_string(v, 16, hex) == 0;
}

static bool constants_init(void) {
    if (constants_ready) return true;
    mbedtls_mpi_init(&Q); mbedtls_mpi_init(&DD); mbedtls_mpi_init(&SQRTM1);
    mbedtls_mpi_init(&BX); mbedtls_mpi_init(&BY); mbedtls_mpi_init(&EXP8);
    mbedtls_mpi_init(&ONE); mbedtls_mpi_init(&L);
    bool ok = mpi_const(&Q, "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffed") &&
              mpi_const(&L, "1000000000000000000000000000000014def9dea2f79cd65812631a5cf5d3ed") &&
              mpi_const(&DD, "52036cee2b6ffe738cc740797779e89800700a4d4141d8ab75eb4dca135978a3") &&
              mpi_const(&SQRTM1, "2b8324804fc1df0b2b4d00993dfbd7a72f431806ad2fe478c4ee1b274a0ea0b0") &&
              mpi_const(&BX, "216936d3cd6e53fec0a4e231fdd6dc5c692cc7609525a7b2c9562d608f25d51a") &&
              mpi_const(&BY, "6666666666666666666666666666666666666666666666666666666666666658") &&
              mpi_const(&EXP8, "0ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe") &&
              (mbedtls_mpi_lset(&ONE, 1) == 0);
    constants_ready = ok;
    return ok;
}

#define TRY(expr) do { rc = (expr); if (rc != 0) goto cleanup; } while (0)

typedef struct {
    mbedtls_mpi x, y, z, t;   /* extended twisted Edwards (a = -1) */
} ge;

static void ge_init(ge *p) {
    mbedtls_mpi_init(&p->x); mbedtls_mpi_init(&p->y);
    mbedtls_mpi_init(&p->z); mbedtls_mpi_init(&p->t);
}
static void ge_free(ge *p) {
    mbedtls_mpi_free(&p->x); mbedtls_mpi_free(&p->y);
    mbedtls_mpi_free(&p->z); mbedtls_mpi_free(&p->t);
}
static int ge_copy(ge *r, const ge *p) {
    int rc = mbedtls_mpi_copy(&r->x, &p->x);
    if (!rc) rc = mbedtls_mpi_copy(&r->y, &p->y);
    if (!rc) rc = mbedtls_mpi_copy(&r->z, &p->z);
    if (!rc) rc = mbedtls_mpi_copy(&r->t, &p->t);
    return rc;
}
static int ge_identity(ge *p) {
    int rc = mbedtls_mpi_lset(&p->x, 0);
    if (!rc) rc = mbedtls_mpi_lset(&p->y, 1);
    if (!rc) rc = mbedtls_mpi_lset(&p->z, 1);
    if (!rc) rc = mbedtls_mpi_lset(&p->t, 0);
    return rc;
}
static int ge_base(ge *p) {
    int rc = mbedtls_mpi_copy(&p->x, &BX);
    if (!rc) rc = mbedtls_mpi_copy(&p->y, &BY);
    if (!rc) rc = mbedtls_mpi_lset(&p->z, 1);
    if (!rc) rc = mbedtls_mpi_mul_mpi(&p->t, &BX, &BY);
    if (!rc) rc = mbedtls_mpi_mod_mpi(&p->t, &p->t, &Q);
    return rc;
}

static int mod_reduce(mbedtls_mpi *a) {
    int rc = mbedtls_mpi_mod_mpi(a, a, &Q);
    if (rc) return rc;
    if (a->s < 0) rc = mbedtls_mpi_add_mpi(a, a, &Q);
    return rc;
}
static int mpi_addmod(mbedtls_mpi *r, const mbedtls_mpi *a,
                      const mbedtls_mpi *b) {
    int rc = mbedtls_mpi_add_mpi(r, a, b);
    return rc ? rc : mod_reduce(r);
}
static int mpi_submod(mbedtls_mpi *r, const mbedtls_mpi *a,
                      const mbedtls_mpi *b) {
    int rc = mbedtls_mpi_copy(r, a);
    if (rc) return rc;
    if (mbedtls_mpi_cmp_mpi(r, b) < 0) {
        rc = mbedtls_mpi_add_mpi(r, r, &Q);
        if (rc) return rc;
    }
    rc = mbedtls_mpi_sub_mpi(r, r, b);
    return rc ? rc : mod_reduce(r);
}
static int mpi_mulmod(mbedtls_mpi *r, const mbedtls_mpi *a,
                      const mbedtls_mpi *b) {
    int rc = mbedtls_mpi_mul_mpi(r, a, b);
    return rc ? rc : mod_reduce(r);
}

/* Complete addition formulas for a = -1 twisted Edwards (add-2008-hwcd).
 * Works for doubling too, so no separate doubling path is needed. */
static int ge_add(ge *r, const ge *p, const ge *q) {
    mbedtls_mpi t1, t2, A, B, C, D, E, F, G, H, tmp;
    int rc = 0;
    mbedtls_mpi_init(&t1); mbedtls_mpi_init(&t2); mbedtls_mpi_init(&A);
    mbedtls_mpi_init(&B); mbedtls_mpi_init(&C); mbedtls_mpi_init(&D);
    mbedtls_mpi_init(&E); mbedtls_mpi_init(&F); mbedtls_mpi_init(&G);
    mbedtls_mpi_init(&H); mbedtls_mpi_init(&tmp);

    TRY(mpi_submod(&t1, &p->y, &p->x));
    TRY(mpi_submod(&t2, &q->y, &q->x));
    TRY(mpi_mulmod(&A, &t1, &t2));
    TRY(mpi_addmod(&t1, &p->y, &p->x));
    TRY(mpi_addmod(&t2, &q->y, &q->x));
    TRY(mpi_mulmod(&B, &t1, &t2));
    TRY(mpi_mulmod(&C, &p->t, &q->t));
    TRY(mbedtls_mpi_mul_int(&C, &C, 2));
    TRY(mpi_mulmod(&C, &C, &DD));
    TRY(mpi_mulmod(&D, &p->z, &q->z));
    TRY(mbedtls_mpi_mul_int(&D, &D, 2));
    TRY(mpi_submod(&E, &B, &A));
    TRY(mpi_submod(&F, &D, &C));
    TRY(mpi_addmod(&G, &D, &C));
    TRY(mpi_addmod(&H, &B, &A));
    TRY(mpi_mulmod(&tmp, &E, &F)); TRY(mbedtls_mpi_copy(&r->x, &tmp));
    TRY(mpi_mulmod(&tmp, &G, &H)); TRY(mbedtls_mpi_copy(&r->y, &tmp));
    TRY(mpi_mulmod(&tmp, &E, &H)); TRY(mbedtls_mpi_copy(&r->t, &tmp));
    TRY(mpi_mulmod(&tmp, &F, &G)); TRY(mbedtls_mpi_copy(&r->z, &tmp));

cleanup:
    mbedtls_mpi_free(&t1); mbedtls_mpi_free(&t2); mbedtls_mpi_free(&A);
    mbedtls_mpi_free(&B); mbedtls_mpi_free(&C); mbedtls_mpi_free(&D);
    mbedtls_mpi_free(&E); mbedtls_mpi_free(&F); mbedtls_mpi_free(&G);
    mbedtls_mpi_free(&H); mbedtls_mpi_free(&tmp);
    return rc;
}

/* -- Constant-time scalar multiplication -------------------------------- *
 * The previous double-and-add loop branched on every secret scalar bit
 * (mbedtls_mpi_get_bit) and ran mbedtls_mpi_bitlen(n) times, so the count and
 * pattern of ge_add() calls leaked both the Hamming weight and the bit length
 * of the two signing scalars (a and r).  This is the timing side channel the
 * differential probe in tools/ed25519_timing_gate.sh measures.
 *
 * Replacement is a full-width Montgomery ladder.  Each iteration performs
 * exactly one group addition, one doubling and one conditional swap of all
 * four projective coordinates; the trip count is the compile-time constant
 * WUBU_ED25519_LADDER_BITS, and no secret bit selects a branch or indexes a
 * table.  Conditional selection uses mbedTLS's own constant-time
 * mbedtls_mpi_safe_cond_swap(), and the accumulator limbs are pre-sized so
 * that no allocation can occur once the ladder is running.
 *
 * ge_mul_doubleadd() is retained ONLY as the negative control for the timing
 * gate.  It is compiled out of every production build; the probe opts in with
 * WUBU_ED25519_TIMING_PROBE, and WUBU_ED25519_LEGACY_MUL selects it as the
 * public ge_mul only for the legacy comparison build of the probe.
 */
#define WUBU_ED25519_LADDER_BITS 256u
#define WUBU_ED25519_FIELD_LIMBS \
    ((255u / (sizeof(mbedtls_mpi_uint) * 8u)) + 1u)

#ifdef WUBU_EDDSA_CT_PROBE
/* Evidence-only counter: compiled ONLY by tools/eddsa_ct_evidence.sh, never
 * by a product or gate build. Counts constant-selection calls. */
static unsigned long wubu_ct_cswap_calls = 0;
unsigned long wubu_pkg_ed25519_ct_cswap_calls(void) { return wubu_ct_cswap_calls; }
#endif

static int ge_grow(ge *p, size_t limbs) {
    int rc = mbedtls_mpi_grow(&p->x, limbs);
    if (!rc) rc = mbedtls_mpi_grow(&p->y, limbs);
    if (!rc) rc = mbedtls_mpi_grow(&p->z, limbs);
    if (!rc) rc = mbedtls_mpi_grow(&p->t, limbs);
    return rc;
}

/* Constant-time conditional swap of a projective point.  mbedTLS documents
 * mbedtls_mpi_safe_cond_swap() as leaking nothing about `swap`; its shipped
 * 2.28.8 object code masks the swap byte into an all-ones/all-zeros limb mask
 * with neg/or/sar and XOR-selects every limb (no data-dependent branch). */
static int ge_cswap(ge *a, ge *b, unsigned char swap) {
#ifdef WUBU_EDDSA_CT_PROBE
    ++wubu_ct_cswap_calls;
#endif
    int rc = mbedtls_mpi_safe_cond_swap(&a->x, &b->x, swap);
    if (!rc) rc = mbedtls_mpi_safe_cond_swap(&a->y, &b->y, swap);
    if (!rc) rc = mbedtls_mpi_safe_cond_swap(&a->z, &b->z, swap);
    if (!rc) rc = mbedtls_mpi_safe_cond_swap(&a->t, &b->t, swap);
    return rc;
}

#if defined(WUBU_ED25519_TIMING_PROBE) || defined(WUBU_ED25519_LEGACY_MUL)
/* VERBATIM pre-hardening implementation -- NEGATIVE CONTROL ONLY.
 * Secret-dependent branch + secret-dependent trip count. Never compiled into
 * a package-signing binary; see tools/ed25519_timing_gate.sh. */
static int ge_mul_doubleadd(ge *r, const ge *p, const mbedtls_mpi *n) {
    ge acc;
    int rc = 0;
    ge_init(&acc);
    TRY(ge_identity(&acc));
    size_t bits = mbedtls_mpi_bitlen(n);
    for (size_t i = bits; i > 0; i--) {
        TRY(ge_add(&acc, &acc, &acc));
        if (mbedtls_mpi_get_bit(n, i - 1))
            TRY(ge_add(&acc, &acc, p));
    }
    TRY(ge_copy(r, &acc));
cleanup:
    ge_free(&acc);
    return rc;
}
#endif

/* Production scalar multiplication: constant schedule, constant selection. */
static int ge_mul_ladder(ge *r, const ge *p, const mbedtls_mpi *n) {
    ge r0, r1, sum, dbl;
    int rc = 0;
    unsigned char swap = 0, bit;
    ge_init(&r0); ge_init(&r1); ge_init(&sum); ge_init(&dbl);

    TRY(ge_identity(&r0));            /* r0 = [0]P */
    TRY(ge_copy(&r1, p));             /* r1 = [1]P */
    /* Fix all limb counts before the loop: safe_cond_swap() grows MPIs, and
     * a reallocation at a secret-dependent point would itself be a leak. */
    TRY(ge_grow(&r0, WUBU_ED25519_FIELD_LIMBS));
    TRY(ge_grow(&r1, WUBU_ED25519_FIELD_LIMBS));
    TRY(ge_grow(&sum, WUBU_ED25519_FIELD_LIMBS));
    TRY(ge_grow(&dbl, WUBU_ED25519_FIELD_LIMBS));

    /* Invariant at loop top: r0 = [x]P, r1 = [x+1]P for the prefix x already
     * consumed.  b = bit ^ swap restores that invariant; both the following
     * group operations and the swap masks are independent of b's value. */
    for (unsigned int i = WUBU_ED25519_LADDER_BITS; i > 0; --i) {
        bit = (unsigned char)mbedtls_mpi_get_bit(n, i - 1);
        swap ^= bit;
        TRY(ge_cswap(&r0, &r1, swap));
        TRY(ge_add(&sum, &r0, &r1));   /* [2x+1]P */
        TRY(ge_add(&dbl, &r0, &r0));   /* [2x]P   */
        TRY(ge_copy(&r1, &sum));
        TRY(ge_copy(&r0, &dbl));
        swap = bit;
    }
    TRY(ge_cswap(&r0, &r1, swap));     /* undo the final pending swap */
    TRY(ge_copy(r, &r0));
cleanup:
    ge_free(&r0); ge_free(&r1); ge_free(&sum); ge_free(&dbl);
    return rc;
}

/* Forward declarations for the opt-in fixed-width ladder adapter; the body is
 * after ge_tobytes()/ge_frombytes() below.  Only point encode/decode uses
 * mbedTLS here -- the ladder itself lives in wubu_ed25519_fixed.c. */
#ifdef WUBU_EDDSA_FIXED_LADDER
static int ge_frombytes(ge *p, const uint8_t enc[32]);
static int ge_tobytes(uint8_t enc[32], const ge *p);
static int ge_mul_fixed(ge *r, const ge *p, const mbedtls_mpi *n);
#endif

/* Selecting the insecure loop requires BOTH macros: the probe defines the
 * TIMING_PROBE macro itself, so a stray WUBU_ED25519_LEGACY_MUL in a normal
 * build cannot silently put the old code back on the signing path. */
#if defined(WUBU_EDDSA_FIXED_LADDER)
#define ge_mul ge_mul_fixed
#elif defined(WUBU_ED25519_LEGACY_MUL) && defined(WUBU_ED25519_TIMING_PROBE)
#define ge_mul ge_mul_doubleadd
#else
#define ge_mul ge_mul_ladder
#endif

static int ge_frombytes(ge *p, const uint8_t enc[32]) {
    uint8_t buf[32];
    int sign;
    mbedtls_mpi y2, num, den, inv, x2, x, chk;
    int rc = 0;

    memcpy(buf, enc, 32);
    sign = buf[31] >> 7;
    buf[31] &= 0x7f;

    mbedtls_mpi_init(&y2); mbedtls_mpi_init(&num); mbedtls_mpi_init(&den);
    mbedtls_mpi_init(&inv); mbedtls_mpi_init(&x2); mbedtls_mpi_init(&x);
    mbedtls_mpi_init(&chk);

    TRY(mbedtls_mpi_read_binary_le(&p->y, buf, 32));
    if (mbedtls_mpi_cmp_mpi(&p->y, &Q) >= 0) { rc = -1; goto cleanup; }
    TRY(mpi_mulmod(&y2, &p->y, &p->y));
    TRY(mpi_submod(&num, &y2, &ONE));
    TRY(mpi_mulmod(&den, &DD, &y2));
    TRY(mpi_addmod(&den, &den, &ONE));
    TRY(mbedtls_mpi_inv_mod(&inv, &den, &Q));
    TRY(mpi_mulmod(&x2, &num, &inv));
    TRY(mbedtls_mpi_exp_mod(&x, &x2, &EXP8, &Q, NULL));
    TRY(mpi_mulmod(&chk, &x, &x));
    if (mbedtls_mpi_cmp_mpi(&chk, &x2) != 0)
        TRY(mpi_mulmod(&x, &x, &SQRTM1));
    TRY(mpi_mulmod(&chk, &x, &x));
    if (mbedtls_mpi_cmp_mpi(&chk, &x2) != 0) { rc = -1; goto cleanup; }
    if ((int)mbedtls_mpi_get_bit(&x, 0) != sign) {
        mbedtls_mpi tmp2;
        mbedtls_mpi_init(&tmp2);
        rc = mpi_submod(&tmp2, &Q, &x);
        if (!rc) rc = mbedtls_mpi_copy(&x, &tmp2);
        mbedtls_mpi_free(&tmp2);
        if (rc) goto cleanup;
    }
    if (mbedtls_mpi_cmp_mpi(&x, &Q) >= 0 ||
        (sign && mbedtls_mpi_cmp_int(&x, 0) == 0)) { rc = -1; goto cleanup; }
    TRY(mbedtls_mpi_copy(&p->x, &x));
    TRY(mbedtls_mpi_lset(&p->z, 1));
    TRY(mpi_mulmod(&p->t, &p->x, &p->y));

cleanup:
    mbedtls_mpi_free(&y2); mbedtls_mpi_free(&num); mbedtls_mpi_free(&den);
    mbedtls_mpi_free(&inv); mbedtls_mpi_free(&x2); mbedtls_mpi_free(&x);
    mbedtls_mpi_free(&chk);
    return rc;
}

static int ge_tobytes(uint8_t enc[32], const ge *p) {
    mbedtls_mpi inv, x, y;
    int rc = 0;
    mbedtls_mpi_init(&inv); mbedtls_mpi_init(&x); mbedtls_mpi_init(&y);
    TRY(mbedtls_mpi_inv_mod(&inv, &p->z, &Q));
    TRY(mpi_mulmod(&x, &p->x, &inv));
    TRY(mpi_mulmod(&y, &p->y, &inv));
    TRY(mbedtls_mpi_write_binary_le(&y, enc, 32));
    enc[31] &= 0x7f;
    enc[31] |= (uint8_t)(mbedtls_mpi_get_bit(&x, 0) << 7);
cleanup:
    mbedtls_mpi_free(&inv); mbedtls_mpi_free(&x); mbedtls_mpi_free(&y);
    return rc;
}

#ifdef WUBU_EDDSA_FIXED_LADDER
/* Opt-in scalar multiplication through the self-contained fixed-width ladder.
 * The scalar is written as fixed 32-byte little-endian, the point is encoded
 * with the existing (public-point) encoder, and the result is decoded back.
 * No mbedtls_mpi call occurs inside the ladder itself; encode/decode are the
 * only bignum uses in this adapter and are outside the CT claim (the inputs
 * are the public base point, the public key, or a published signature R). */
static int ge_mul_fixed(ge *r, const ge *p, const mbedtls_mpi *n) {
    uint8_t pbuf[32], rbuf[32], nbuf[32];
    int rc = mbedtls_mpi_write_binary_le(n, nbuf, sizeof(nbuf));
    if (rc != 0) return rc;
    ge_tobytes(pbuf, p);
    rc = wubu_ed25519_fixed_scalarmult(rbuf, nbuf, pbuf);
    if (rc != 0) return rc;
    return ge_frombytes(r, rbuf);
}
#endif

static int ge_equal(const ge *a, const ge *b) {
    mbedtls_mpi l, r;
    int rc = 0, eq = 0;
    mbedtls_mpi_init(&l); mbedtls_mpi_init(&r);
    TRY(mpi_mulmod(&l, &a->x, &b->z));
    TRY(mpi_mulmod(&r, &b->x, &a->z));
    if (mbedtls_mpi_cmp_mpi(&l, &r) != 0) goto cleanup;
    TRY(mpi_mulmod(&l, &a->y, &b->z));
    TRY(mpi_mulmod(&r, &b->y, &a->z));
    eq = (mbedtls_mpi_cmp_mpi(&l, &r) == 0);
cleanup:
    mbedtls_mpi_free(&l); mbedtls_mpi_free(&r);
    return rc ? rc : eq;
}

static int hex_to_32(const char *hex, uint8_t out[32]) {
    static const char digits[] = "0123456789abcdef";
    if (!hex || !out || strlen(hex) != 64) return -1;
    for (size_t i = 0; i < 32; i++) {
        char c = hex[2 * i], d = hex[2 * i + 1];
        const char *p1 = strchr(digits, c), *p2 = strchr(digits, d);
        if (!p1 || !p2) return -1;
        out[i] = (uint8_t)(((p1 - digits) << 4) | (p2 - digits));
    }
    return 0;
}

static void sha512_one(const uint8_t *msg, size_t len, uint8_t out[64]) {
    mbedtls_sha512_context ctx;
    mbedtls_sha512_init(&ctx);
    mbedtls_sha512_starts_ret(&ctx, 0);
    mbedtls_sha512_update_ret(&ctx, msg, len);
    mbedtls_sha512_finish_ret(&ctx, out);
    mbedtls_sha512_free(&ctx);
}

static int hash_challenge(const uint8_t r[32], const uint8_t a[32],
                          const uint8_t digest[32], uint8_t out[64]) {
    mbedtls_sha512_context ctx;
    mbedtls_sha512_init(&ctx);
    mbedtls_sha512_starts_ret(&ctx, 0);
    mbedtls_sha512_update_ret(&ctx, r, 32);
    mbedtls_sha512_update_ret(&ctx, a, 32);
    mbedtls_sha512_update_ret(&ctx, digest, 32);
    mbedtls_sha512_finish_ret(&ctx, out);
    mbedtls_sha512_free(&ctx);
    return 0;
}

/* Ed25519 scalars are reduced modulo the subgroup order L, NOT the field
 * prime Q.  Confusing the two yields syntactically valid but invalid sigs. */
static int scalar_from_le(mbedtls_mpi *s, const uint8_t b[64], size_t n) {
    int rc = mbedtls_mpi_read_binary_le(s, b, n);
    if (rc) return rc;
    return mbedtls_mpi_mod_mpi(s, s, &L);
}
/* S = (k*a + r) mod L  (field helpers below are mod Q and must NOT be used) */
static int scalar_mul_add(mbedtls_mpi *S, const mbedtls_mpi *k,
                          const mbedtls_mpi *a, const mbedtls_mpi *r) {
    mbedtls_mpi t;
    int rc;
    mbedtls_mpi_init(&t);
    rc = mbedtls_mpi_mul_mpi(&t, k, a);
    if (!rc) rc = mbedtls_mpi_mod_mpi(&t, &t, &L);
    if (!rc) rc = mbedtls_mpi_add_mpi(S, &t, r);
    if (!rc) rc = mbedtls_mpi_mod_mpi(S, S, &L);
    mbedtls_mpi_free(&t);
    return rc;
}

/* -- Opt-in native EdDSA backend (OpenSSL 3 EVP_PKEY_ED25519) -----------
 * WUBU_HAVE_OPENSSL_EDDSA is a BUILD-TIME opt-in.  When set, the sign/verify
 * entry points below use OpenSSL's RFC 8032 primitive for the group math.
 * The message passed to both backends is the SAME canonical 32-byte digest
 * from wubu_pkg_sign_digest(): plain Ed25519 with M == digest, so signatures
 * are byte-for-byte interchangeable with the in-repo ladder.
 *
 * Policy is NOT delegated: non-canonical S and small-subgroup A are rejected
 * by wubu_pkg_sign_verify_ed25519() before the native verifier is called, on
 * both backends.  If OpenSSL cannot initialise (or WUBU_EDDSA_FORCE_LADDER is
 * defined to simulate that), both helpers return -1 and the caller falls
 * closed onto the hardened Montgomery ladder -- never onto a weaker path.
 */
#ifdef WUBU_HAVE_OPENSSL_EDDSA
static unsigned wubu_openssl_native_ops = 0;

unsigned wubu_pkg_ed25519_native_ops(void) { return wubu_openssl_native_ops; }

/* 1 = signed/verified natively, 0 = native ran and rejected, -1 = unavailable. */
static int wubu_openssl_ed25519_sign(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                     const uint8_t seed[32],
                                     uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN]) {
#ifdef WUBU_EDDSA_FORCE_LADDER
    (void)digest; (void)seed; (void)out_sig;
    return -1;
#else
    EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, seed, 32);
    if (!pkey) return -1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { EVP_PKEY_free(pkey); return -1; }
    size_t siglen = WUBU_PKG_SIGNATURE_LEN;  /* one-shot: input = buffer capacity */
    int result = -1;
    if (EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey) == 1 &&
        EVP_DigestSign(ctx, out_sig, &siglen, digest, WUBU_PKG_SIGN_DIGEST_LEN) == 1 &&
        siglen == WUBU_PKG_SIGNATURE_LEN) {
        result = 1;
        ++wubu_openssl_native_ops;
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return result;
#endif
}

static int wubu_openssl_ed25519_verify(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                       const uint8_t pub[32],
                                       const uint8_t sig[WUBU_PKG_SIGNATURE_LEN]) {
#ifdef WUBU_EDDSA_FORCE_LADDER
    (void)digest; (void)pub; (void)sig;
    return -1;
#else
    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub, 32);
    if (!pkey) return -1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { EVP_PKEY_free(pkey); return -1; }
    int result = 0;  /* 0 = native verifier ran and rejected */
    if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey) == 1 &&
        EVP_DigestVerify(ctx, sig, WUBU_PKG_SIGNATURE_LEN,
                         digest, WUBU_PKG_SIGN_DIGEST_LEN) == 1) {
        result = 1;
        ++wubu_openssl_native_ops;
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return result;
#endif
}
#endif /* WUBU_HAVE_OPENSSL_EDDSA */

bool wubu_pkg_sign_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                           const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    if (!constants_init()) { wubu_pkg_sign_set_err(err, errsz, "mbedTLS constants init failed"); return false; }
    uint8_t seed[32];
    if (hex_to_32(seed_hex, seed) != 0 || !digest || !out_sig) {
        wubu_pkg_sign_set_err(err, errsz, "Ed25519 sign needs a 32-byte seed hex (64 chars)"); return false;
    }

#ifdef WUBU_HAVE_OPENSSL_EDDSA
    /* Native path signs the same canonical digest.  -1 (unavailable) or 0
     * (native failed) both fail closed onto the hardened ladder below. */
    if (wubu_openssl_ed25519_sign(digest, seed, out_sig) == 1) return true;
#endif

    uint8_t h[64], rh[64], kh[64], renc[32], aenc[32];
    sha512_one(seed, 32, h);
    h[0] &= 248; h[31] &= 63; h[31] |= 64;

    mbedtls_mpi a, r, k, S;
    ge B, R, A;
    int rc = 0;
    mbedtls_mpi_init(&a); mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&k); mbedtls_mpi_init(&S);
    ge_init(&B); ge_init(&R); ge_init(&A);

    TRY(scalar_from_le(&a, h, 32));
    {
        mbedtls_sha512_context ctx;
        mbedtls_sha512_init(&ctx);
        mbedtls_sha512_starts_ret(&ctx, 0);
        mbedtls_sha512_update_ret(&ctx, h + 32, 32);
        mbedtls_sha512_update_ret(&ctx, digest, 32);
        mbedtls_sha512_finish_ret(&ctx, rh);
        mbedtls_sha512_free(&ctx);
    }
    TRY(scalar_from_le(&r, rh, 64));
    TRY(ge_base(&B));
    TRY(ge_mul(&R, &B, &r));
    TRY(ge_tobytes(renc, &R));
    TRY(ge_mul(&A, &B, &a));
    TRY(ge_tobytes(aenc, &A));
    TRY(hash_challenge(renc, aenc, digest, kh));
    TRY(scalar_from_le(&k, kh, 64));
    TRY(scalar_mul_add(&S, &k, &a, &r));
    TRY(mbedtls_mpi_write_binary_le(&S, out_sig + 32, 32));
    memcpy(out_sig, renc, 32);

cleanup:
    mbedtls_mpi_free(&a); mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&k); mbedtls_mpi_free(&S);
    ge_free(&B); ge_free(&R); ge_free(&A);
    if (rc != 0) { wubu_pkg_sign_set_err(err, errsz, "Ed25519 sign failed (mbedTLS bignum error)"); return false; }
    return true;
}

bool wubu_pkg_sign_verify_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                  const char *pubkey_hex,
                                  const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                                  char *err, size_t errsz) {
    if (!constants_init()) { wubu_pkg_sign_set_err(err, errsz, "mbedTLS constants init failed"); return false; }
    uint8_t pub[32];
    if (hex_to_32(pubkey_hex, pub) != 0 || !digest || !sig) {
        wubu_pkg_sign_set_err(err, errsz, "Ed25519 verify needs a 32-byte public key hex (64 chars)");
        return false;
    }

    mbedtls_mpi S, k, eight;
    ge B, R, A, kA, lhs, rhs, eightA, ident;
    int rc = 0, ok = 0;
    uint8_t kh[64];

    mbedtls_mpi_init(&S); mbedtls_mpi_init(&k); mbedtls_mpi_init(&eight);
    ge_init(&B); ge_init(&R); ge_init(&A);
    ge_init(&kA); ge_init(&lhs); ge_init(&rhs);
    ge_init(&eightA); ge_init(&ident);

    TRY(mbedtls_mpi_read_binary_le(&S, sig + 32, 32));
    if (mbedtls_mpi_cmp_mpi(&S, &L) >= 0) { wubu_pkg_sign_set_err(err, errsz, "non-canonical Ed25519 S"); goto cleanup; }
    TRY(ge_frombytes(&R, sig));
    TRY(ge_frombytes(&A, pub));
    /* Reject small-subgroup public keys: [8]A must not be the identity.
     * Without this, A=identity makes R+[k]A == R for every challenge and
     * S=1, R=[1]B forges a signature with no secret key. */
    TRY(mbedtls_mpi_lset(&eight, 8));
    TRY(ge_identity(&ident));
    TRY(ge_mul(&eightA, &A, &eight));
    {
        int small = ge_equal(&eightA, &ident);
        if (small < 0) { rc = small; goto cleanup; }
        if (small > 0) {
            wubu_pkg_sign_set_err(err, errsz, "small-subgroup public key rejected");
            goto cleanup;
        }
    }
    /* Policy above is shared by both backends and is never delegated to
     * OpenSSL.  Only the core [S]B == R + [k]A check is backend-specific. */
#ifdef WUBU_HAVE_OPENSSL_EDDSA
    {
        int nv = wubu_openssl_ed25519_verify(digest, pub, sig);
        if (nv >= 0) {  /* 1 = verified, 0 = rejected; -1 = unavailable -> ladder */
            ok = nv;
            if (!ok) wubu_pkg_sign_set_err(err, errsz, "Ed25519 signature does not verify");
            goto cleanup;
        }
    }
#endif
    TRY(hash_challenge(sig, pub, digest, kh));
    TRY(scalar_from_le(&k, kh, 64));
    TRY(ge_base(&B));
    TRY(ge_mul(&lhs, &B, &S));      /* [S]B */
    TRY(ge_mul(&kA, &A, &k));       /* [k]A */
    TRY(ge_add(&rhs, &R, &kA));     /* R + [k]A */
    {
        int eq = ge_equal(&lhs, &rhs);
        if (eq < 0) { rc = eq; goto cleanup; }
        ok = eq;
    }
    if (!ok) wubu_pkg_sign_set_err(err, errsz, "Ed25519 signature does not verify");

cleanup:
    mbedtls_mpi_free(&S); mbedtls_mpi_free(&k); mbedtls_mpi_free(&eight);
    ge_free(&B); ge_free(&R); ge_free(&A);
    ge_free(&kA); ge_free(&lhs); ge_free(&rhs);
    ge_free(&eightA); ge_free(&ident);
    if (rc != 0) {
        wubu_pkg_sign_set_err(err, errsz, "Ed25519 verify failed (invalid point or bignum error)");
        return false;
    }
    return ok != 0;
}

/* -- Arbitrary-length command signing (revocation CLI) ---------------
 * SHA-256 collapses the domain-separated command to the fixed 32-byte
 * message the Ed25519 primitive above expects.  The hex digest is decoded
 * by the same hex_to_32() used for keys/seeds, so there is one canonical
 * byte encoding.  Domain separation stays in the policy layer. */
static bool command_digest(const char *command,
                           uint8_t out[WUBU_PKG_SIGN_DIGEST_LEN]) {
    char hex[2 * WUBU_PKG_SIGN_DIGEST_LEN + 1];
    if (!command || !out) return false;
    wubu_sha256_digest(command, strlen(command), hex, sizeof(hex));
    return hex_to_32(hex, out) == 0;
}

bool wubu_pkg_sign_command(const char *command, const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!command_digest(command, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash command for signing");
        return false;
    }
    return wubu_pkg_sign_ed25519(digest, seed_hex, out_sig, err, errsz);
}

bool wubu_pkg_verify_command(const char *command, const char *pubkey_hex,
                             const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                             char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!command_digest(command, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash command for verification");
        return false;
    }
    return wubu_pkg_sign_verify_ed25519(digest, pubkey_hex, sig, err, errsz);
}

/* -- Domain-separated hashing of arbitrary byte buffers (feed import) ----
 * The domain and the exact byte length are folded into the SHA-256 preimage
 * so a feed signature is bound to this protocol AND to the exact file bytes.
 * Only the resulting 32-byte digest reaches the Ed25519 primitive above. */
static bool bytes_digest(const uint8_t *data, size_t len, const char *domain,
                         uint8_t out[WUBU_PKG_SIGN_DIGEST_LEN]) {
    if (!out || !domain || !domain[0] || (!data && len)) return false;
    char hdr[128];
    int hn = snprintf(hdr, sizeof(hdr), "%s\n%zu\n", domain, len);
    if (hn < 0 || (size_t)hn >= sizeof(hdr)) return false;
    wubu_SHA256_CTX ctx;
    wubu_sha256_init(&ctx);
    wubu_sha256_update(&ctx, hdr, (size_t)hn);
    if (len) wubu_sha256_update(&ctx, data, len);
    wubu_sha256_final(&ctx, out);
    return true;
}

bool wubu_pkg_sign_bytes(const uint8_t *data, size_t len, const char *domain,
                         const char *seed_hex,
                         uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                         char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!bytes_digest(data, len, domain, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash bytes for signing");
        return false;
    }
    return wubu_pkg_sign_ed25519(digest, seed_hex, out_sig, err, errsz);
}

bool wubu_pkg_verify_bytes(const uint8_t *data, size_t len, const char *domain,
                           const char *pubkey_hex,
                           const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!bytes_digest(data, len, domain, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash bytes for verification");
        return false;
    }
    return wubu_pkg_sign_verify_ed25519(digest, pubkey_hex, sig, err, errsz);
}

#ifdef WUBU_EDDSA_CT_PROBE
/* Evidence hook: run the REAL production ladder on a raw little-endian
 * scalar and return the number of constant-selection calls it made.  The
 * evidence script asserts this count is identical for every scalar class. */
int wubu_pkg_ed25519_ct_ladder_run(const char *scalar_hex, char *err, size_t errsz) {
    uint8_t s[32];
    mbedtls_mpi n;
    ge p, r;
    int rc = 0;
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!constants_init()) { wubu_pkg_sign_set_err(err, errsz, "constants init failed"); return -1; }
    if (hex_to_32(scalar_hex, s) != 0) { wubu_pkg_sign_set_err(err, errsz, "bad scalar hex"); return -1; }
    mbedtls_mpi_init(&n); ge_init(&p); ge_init(&r);
    TRY(scalar_from_le(&n, s, 32));
    TRY(ge_base(&p));
    wubu_ct_cswap_calls = 0;
    TRY(ge_mul(&r, &p, &n));
cleanup:
    mbedtls_mpi_free(&n); ge_free(&p); ge_free(&r);
    if (rc != 0) wubu_pkg_sign_set_err(err, errsz, "ladder probe failed");
    return rc;
}
#endif /* WUBU_EDDSA_CT_PROBE */

#endif /* WUBU_EDDSA_FIXED_LADDER full bignum-free path / legacy bignum backend */

#elif defined(WUBU_HAVE_OPENSSL_EDDSA)
/* -- Native-only backend: OpenSSL EVP + bignum-free policy ---------------
 * Compiled when OpenSSL 3 is available but NO mbedTLS dev root is installed.
 * No mbedTLS header is included and no bignum code appears in this branch.
 * The policy runs before the primitive on every verification, exactly as in
 * the mbedTLS build above, so the two builds reach identical accept/reject
 * decisions on the same digest, key and signature.
 */
#include <openssl/evp.h>
#include "../runtime/wubu_eddsa_policy.h"

static unsigned wubu_openssl_native_ops = 0;

unsigned wubu_pkg_ed25519_native_ops(void) { return wubu_openssl_native_ops; }

/* 1 = signed/verified natively, 0 = native ran and rejected, -1 = unavailable. */
static int wubu_openssl_ed25519_sign(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                     const uint8_t seed[32],
                                     uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN]) {
#ifdef WUBU_EDDSA_FORCE_LADDER
    (void)digest; (void)seed; (void)out_sig;
    return -1;  /* simulate an unavailable native backend: fail closed */
#else
    EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, seed, 32);
    if (!pkey) return -1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { EVP_PKEY_free(pkey); return -1; }
    size_t siglen = WUBU_PKG_SIGNATURE_LEN;  /* one-shot: input = capacity */
    int result = -1;
    if (EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey) == 1 &&
        EVP_DigestSign(ctx, out_sig, &siglen, digest, WUBU_PKG_SIGN_DIGEST_LEN) == 1 &&
        siglen == WUBU_PKG_SIGNATURE_LEN) {
        result = 1;
        ++wubu_openssl_native_ops;
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return result;
#endif
}

static int wubu_openssl_ed25519_verify(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                       const uint8_t pub[32],
                                       const uint8_t sig[WUBU_PKG_SIGNATURE_LEN]) {
#ifdef WUBU_EDDSA_FORCE_LADDER
    (void)digest; (void)pub; (void)sig;
    return -1;
#else
    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub, 32);
    if (!pkey) return -1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { EVP_PKEY_free(pkey); return -1; }
    int result = 0;  /* 0 = native verifier ran and rejected */
    if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey) == 1 &&
        EVP_DigestVerify(ctx, sig, WUBU_PKG_SIGNATURE_LEN,
                         digest, WUBU_PKG_SIGN_DIGEST_LEN) == 1) {
        result = 1;
        ++wubu_openssl_native_ops;
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return result;
#endif
}

static int hex_to_32(const char *hex, uint8_t out[32]) {
    static const char digits[] = "0123456789abcdef";
    if (!hex || !out || strlen(hex) != 64) return -1;
    for (size_t i = 0; i < 32; i++) {
        char c = hex[2 * i], d = hex[2 * i + 1];
        const char *p1 = strchr(digits, c), *p2 = strchr(digits, d);
        if (!p1 || !p2) return -1;
        out[i] = (uint8_t)(((p1 - digits) << 4) | (p2 - digits));
    }
    return 0;
}

bool wubu_pkg_sign_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                           const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    uint8_t seed[32];
    if (!digest || !out_sig || hex_to_32(seed_hex, seed) != 0) {
        wubu_pkg_sign_set_err(err, errsz, "Ed25519 sign needs a 32-byte seed hex (64 chars)");
        return false;
    }
    int r = wubu_openssl_ed25519_sign(digest, seed, out_sig);
    if (r < 0) {
        wubu_pkg_sign_set_err(err, errsz, "OpenSSL Ed25519 backend unavailable: refusing to sign");
        return false;
    }
    return r == 1;
}

bool wubu_pkg_sign_verify_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                  const char *pubkey_hex,
                                  const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                                  char *err, size_t errsz) {
    uint8_t pub[32];
    if (!digest || !sig || hex_to_32(pubkey_hex, pub) != 0) {
        wubu_pkg_sign_set_err(err, errsz, "Ed25519 verify needs a 32-byte public key hex (64 chars)");
        return false;
    }
    /* Policy is never delegated to OpenSSL: non-canonical S and small-order
     * A are refused here, before the primitive is called. */
    if (!wubu_eddsa_policy_check(pub, sig, err, errsz)) return false;
    int nv = wubu_openssl_ed25519_verify(digest, pub, sig);
    if (nv < 0) {
        wubu_pkg_sign_set_err(err, errsz, "OpenSSL Ed25519 backend unavailable: refusing to verify");
        return false;
    }
    if (!nv) wubu_pkg_sign_set_err(err, errsz, "Ed25519 signature does not verify");
    return nv == 1;
}

/* -- Arbitrary-length command signing (revocation CLI) ---------------- */
static bool command_digest(const char *command,
                           uint8_t out[WUBU_PKG_SIGN_DIGEST_LEN]) {
    char hex[2 * WUBU_PKG_SIGN_DIGEST_LEN + 1];
    if (!command || !out) return false;
    wubu_sha256_digest(command, strlen(command), hex, sizeof(hex));
    return hex_to_32(hex, out) == 0;
}

bool wubu_pkg_sign_command(const char *command, const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!command_digest(command, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash command for signing");
        return false;
    }
    return wubu_pkg_sign_ed25519(digest, seed_hex, out_sig, err, errsz);
}

bool wubu_pkg_verify_command(const char *command, const char *pubkey_hex,
                             const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                             char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!command_digest(command, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash command for verification");
        return false;
    }
    return wubu_pkg_sign_verify_ed25519(digest, pubkey_hex, sig, err, errsz);
}

/* -- Domain-separated hashing of arbitrary byte buffers (feed import) -- */
static bool bytes_digest(const uint8_t *data, size_t len, const char *domain,
                         uint8_t out[WUBU_PKG_SIGN_DIGEST_LEN]) {
    if (!out || !domain || !domain[0] || (!data && len)) return false;
    char hdr[128];
    int hn = snprintf(hdr, sizeof(hdr), "%s\n%zu\n", domain, len);
    if (hn < 0 || (size_t)hn >= sizeof(hdr)) return false;
    wubu_SHA256_CTX ctx;
    wubu_sha256_init(&ctx);
    wubu_sha256_update(&ctx, hdr, (size_t)hn);
    if (len) wubu_sha256_update(&ctx, data, len);
    wubu_sha256_final(&ctx, out);
    return true;
}

bool wubu_pkg_sign_bytes(const uint8_t *data, size_t len, const char *domain,
                         const char *seed_hex,
                         uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                         char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!bytes_digest(data, len, domain, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash bytes for signing");
        return false;
    }
    return wubu_pkg_sign_ed25519(digest, seed_hex, out_sig, err, errsz);
}

bool wubu_pkg_verify_bytes(const uint8_t *data, size_t len, const char *domain,
                           const char *pubkey_hex,
                           const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    wubu_pkg_sign_set_err(err, errsz, "");
    if (!bytes_digest(data, len, domain, digest)) {
        wubu_pkg_sign_set_err(err, errsz, "cannot hash bytes for verification");
        return false;
    }
    return wubu_pkg_sign_verify_ed25519(digest, pubkey_hex, sig, err, errsz);
}

#else
/* Neither backend is compiled in: wubu_pkg_sign.c supplies the fail-closed
 * stubs for the public entry points.  No crypto code is emitted here. */
#endif /* backend selection */
