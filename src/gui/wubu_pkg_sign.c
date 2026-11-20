/*
 * wubu_pkg_sign.c -- .wubu package digest + Ed25519 (RFC 8032) owner.
 * See wubu_pkg_sign.h for the canonical signed-message definition.
 *
 * The default (no WUBU_HAVE_MBEDTLS) build compiles the digest helpers only:
 * signing and verification return false.  There is deliberately no fallback
 * hash-as-signature path.
 */
#include "wubu_pkg_sign.h"
#include "../runtime/wubu_crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WUBU_HAVE_MBEDTLS
#include <mbedtls/sha512.h>
#include <mbedtls/bignum.h>
#endif

static void set_err(char *err, size_t errsz, const char *msg) {
    if (err && errsz) {
        strncpy(err, msg, errsz - 1);
        err[errsz - 1] = '\0';
    }
}

/* -- Canonical package digest -------------------------------------- */

static void digest_zero_span(wubu_SHA256_CTX *ctx, size_t n) {
    static const uint8_t zeros[64] = {0};
    while (n) {
        size_t take = n < sizeof(zeros) ? n : sizeof(zeros);
        wubu_sha256_update(ctx, zeros, take);
        n -= take;
    }
}

bool wubu_pkg_sign_digest(const uint8_t *header, size_t header_len,
                          size_t sig_offset, size_t sig_len,
                          const uint8_t *manifest, size_t manifest_len,
                          const char *payload_path,
                          uint8_t out_digest[WUBU_PKG_SIGN_DIGEST_LEN]) {
    if (!header || !out_digest || !payload_path ||
        sig_offset > header_len || sig_len > header_len - sig_offset ||
        !manifest) return false;

    wubu_SHA256_CTX ctx;
    wubu_sha256_init(&ctx);
    wubu_sha256_update(&ctx, header, sig_offset);
    digest_zero_span(&ctx, sig_len);
    wubu_sha256_update(&ctx, header + sig_offset + sig_len,
                       header_len - sig_offset - sig_len);
    if (manifest_len) wubu_sha256_update(&ctx, manifest, manifest_len);

    FILE *f = fopen(payload_path, "rb");
    if (!f) return false;
    uint8_t buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        wubu_sha256_update(&ctx, buf, n);
    bool bad = ferror(f) != 0;
    fclose(f);
    if (bad) return false;

    wubu_sha256_final(&ctx, out_digest);
    return true;
}

bool wubu_pkg_sign_digest_file(const char *pkg_path, size_t header_len,
                               size_t sig_offset, size_t sig_len,
                               size_t manifest_len,
                               uint8_t out_digest[WUBU_PKG_SIGN_DIGEST_LEN]) {
    if (!pkg_path || !out_digest || sig_offset > header_len ||
        sig_len > header_len - sig_offset)
        return false;

    FILE *f = fopen(pkg_path, "rb");
    if (!f) return false;
    uint8_t *header = (uint8_t *)malloc(header_len ? header_len : 1);
    if (!header) { fclose(f); return false; }
    bool ok = fread(header, 1, header_len, f) == header_len;

    wubu_SHA256_CTX ctx;
    wubu_sha256_init(&ctx);
    if (ok) {
        wubu_sha256_update(&ctx, header, sig_offset);
        digest_zero_span(&ctx, sig_len);
        wubu_sha256_update(&ctx, header + sig_offset + sig_len,
                           header_len - sig_offset - sig_len);
    }

    /* Manifest sits immediately after the header. */
    uint64_t want = (uint64_t)header_len + (uint64_t)manifest_len;
    uint8_t buf[8192];
    size_t left = manifest_len;
    while (ok && left) {
        size_t take = left < sizeof(buf) ? left : sizeof(buf);
        size_t rd = fread(buf, 1, take, f);
        if (rd != take) { ok = false; break; }
        wubu_sha256_update(&ctx, buf, rd);
        left -= rd;
    }

    /* Everything after header+manifest is payload. */
    while (ok) {
        size_t rd = fread(buf, 1, sizeof(buf), f);
        if (rd) wubu_sha256_update(&ctx, buf, rd);
        if (rd < sizeof(buf)) {
            if (ferror(f)) ok = false;
            break;
        }
    }
    if (ok) {
        long got = ftell(f);
        /* Reject truncated containers before a digest can be trusted. */
        if (got < 0 || (uint64_t)got < want) ok = false;
    }
    fclose(f);
    free(header);
    if (!ok) return false;

    wubu_sha256_final(&ctx, out_digest);
    return true;
}

/* -- Ed25519 (RFC 8032) over mbedTLS SHA-512 + bignum -------------- */

#ifdef WUBU_HAVE_MBEDTLS

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

static int ge_mul(ge *r, const ge *p, const mbedtls_mpi *n) {
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

bool wubu_pkg_sign_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                           const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    if (!constants_init()) { set_err(err, errsz, "mbedTLS constants init failed"); return false; }
    uint8_t seed[32];
    if (hex_to_32(seed_hex, seed) != 0 || !digest || !out_sig) {
        set_err(err, errsz, "Ed25519 sign needs a 32-byte seed hex (64 chars)"); return false;
    }

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
    if (rc != 0) { set_err(err, errsz, "Ed25519 sign failed (mbedTLS bignum error)"); return false; }
    return true;
}

bool wubu_pkg_sign_verify_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                  const char *pubkey_hex,
                                  const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                                  char *err, size_t errsz) {
    if (!constants_init()) { set_err(err, errsz, "mbedTLS constants init failed"); return false; }
    uint8_t pub[32];
    if (hex_to_32(pubkey_hex, pub) != 0 || !digest || !sig) {
        set_err(err, errsz, "Ed25519 verify needs a 32-byte public key hex (64 chars)");
        return false;
    }

    mbedtls_mpi S, k;
    ge B, R, A, kA, lhs, rhs;
    int rc = 0, ok = 0;
    uint8_t kh[64];

    mbedtls_mpi_init(&S); mbedtls_mpi_init(&k);
    ge_init(&B); ge_init(&R); ge_init(&A);
    ge_init(&kA); ge_init(&lhs); ge_init(&rhs);

    TRY(mbedtls_mpi_read_binary_le(&S, sig + 32, 32));
    if (mbedtls_mpi_cmp_mpi(&S, &L) >= 0) { set_err(err, errsz, "non-canonical Ed25519 S"); goto cleanup; }
    TRY(ge_frombytes(&R, sig));
    TRY(ge_frombytes(&A, pub));
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
    if (!ok) set_err(err, errsz, "Ed25519 signature does not verify");

cleanup:
    mbedtls_mpi_free(&S); mbedtls_mpi_free(&k);
    ge_free(&B); ge_free(&R); ge_free(&A);
    ge_free(&kA); ge_free(&lhs); ge_free(&rhs);
    if (rc != 0) {
        set_err(err, errsz, "Ed25519 verify failed (invalid point or bignum error)");
        return false;
    }
    return ok != 0;
}

#else /* !WUBU_HAVE_MBEDTLS */

bool wubu_pkg_sign_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                           const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz) {
    (void)digest; (void)seed_hex; (void)out_sig;
    set_err(err, errsz, "mbedTLS not compiled in: Ed25519 signing unavailable");
    return false;
}
bool wubu_pkg_sign_verify_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                  const char *pubkey_hex,
                                  const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                                  char *err, size_t errsz) {
    (void)digest; (void)pubkey_hex; (void)sig;
    set_err(err, errsz, "mbedTLS not compiled in: Ed25519 verification unavailable");
    return false;
}

#endif /* WUBU_HAVE_MBEDTLS */

bool wubu_pkg_sign_verify_package(const char *pkg_path, size_t header_len,
                                  size_t sig_offset, size_t sig_len,
                                  size_t manifest_len, const char *pubkey_hex,
                                  char *err, size_t errsz) {
    set_err(err, errsz, "");
#ifndef WUBU_HAVE_MBEDTLS
    (void)pkg_path; (void)header_len; (void)sig_offset; (void)sig_len;
    (void)manifest_len;
    set_err(err, errsz, "mbedTLS not compiled in: refusing to verify");
    return false;
#else
    if (!pkg_path || !pubkey_hex || !pubkey_hex[0] ||
        sig_len != WUBU_PKG_SIGNATURE_LEN) {
        set_err(err, errsz, "no trust anchor supplied for Ed25519 verification");
        return false;
    }
    FILE *f = fopen(pkg_path, "rb");
    if (!f) { set_err(err, errsz, "cannot open package"); return false; }
    uint8_t *header = (uint8_t *)malloc(header_len ? header_len : 1);
    if (!header) { fclose(f); set_err(err, errsz, "out of memory"); return false; }
    bool ok = fread(header, 1, header_len, f) == header_len;
    fclose(f);
    if (!ok) { free(header); set_err(err, errsz, "truncated package header"); return false; }

    const uint8_t *sig = header + sig_offset;
    bool absent = true;
    for (size_t i = 0; i < sig_len; i++)
        if (sig[i]) { absent = false; break; }
    uint8_t *sigcopy = (uint8_t *)malloc(sig_len);
    if (sigcopy) memcpy(sigcopy, sig, sig_len);
    free(header);
    if (!sigcopy) { set_err(err, errsz, "out of memory"); return false; }
    if (absent) {
        free(sigcopy);
        set_err(err, errsz, "package is unsigned (empty Ed25519 signature)");
        return false;
    }

    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    bool good = wubu_pkg_sign_digest_file(pkg_path, header_len, sig_offset,
                                          sig_len, manifest_len, digest);
    if (!good) {
        free(sigcopy);
        set_err(err, errsz, "cannot digest package contents");
        return false;
    }
    good = wubu_pkg_sign_verify_ed25519(digest, pubkey_hex, sigcopy, err, errsz);
    free(sigcopy);
    return good;
#endif
}
