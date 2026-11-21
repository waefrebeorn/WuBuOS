/*
 * wubu_ed25519_fixed.c -- self-contained fixed-width Ed25519 scalar ladder.
 *
 * NO mbedTLS bignum (mbedtls_mpi) appears anywhere in this file.  Field
 * arithmetic is four unsigned 64-bit little-endian limbs modulo the RFC 8032
 * field prime q = 2^255 - 19.  Every secret-dependent selection is a mask
 * (all-ones / all-zeros) AND/OR, never a branch or an index.  The scalar
 * ladder runs exactly WUBU_ED25519_FIXED_LADDER_BITS (256) iterations and
 * performs the same group operations in the same order for every scalar.
 *
 * This is deliberately a SEPARATE leaf from the shipped mbedTLS MPI path so
 * that the constant-time property can be proved over code we own end to end;
 * the bignum value-independence gate cannot be closed for the mbedTLS callees
 * (headers + .a only -- see docs/EDDSA_BIGNUM_VALUE_CHECK.md).
 */
#include "wubu_ed25519_fixed.h"

#include <string.h>

typedef unsigned __int128 u128;

/* Compile-time-fixed ladder length.  Also declared in the public header; the
 * identical definition here lets the source gate read this module alone. */
#define WUBU_ED25519_FIXED_LADDER_BITS 256u

/* value in [0, q), four little-endian 64-bit limbs mod q = 2^255 - 19 */
typedef struct { uint64_t l[4]; } fe;
/* extended twisted Edwards point (a = -1): X:Y:Z:T with x=X/Z, y=Y/Z, xy=T/Z */
typedef struct { fe X, Y, Z, T; } ge;

#define LIMB_MASK63 0x7fffffffffffffffULL

static const fe FE_ZERO = {{0ULL, 0ULL, 0ULL, 0ULL}};
static const fe FE_ONE  = {{1ULL, 0ULL, 0ULL, 0ULL}};
/* d = -121665/121666 mod q, RFC 8032 public constant */
static const fe FE_D = {{0x75eb4dca135978a3ULL, 0x00700a4d4141d8abULL,
                         0x8cc740797779e898ULL, 0x52036cee2b6ffe73ULL}};
/* sqrt(-1) mod q */
static const fe FE_SQRTM1 = {{0xc4ee1b274a0ea0b0ULL, 0x2f431806ad2fe478ULL,
                              0x2b4d00993dfbd7a7ULL, 0x2b8324804fc1df0bULL}};
/* RFC 8032 base point x, little-endian bytes */
static const uint8_t BASE_X_BYTES[32] = {
    0x1a,0xd5,0x25,0x8f,0x60,0x2d,0x56,0xc9,0xb2,0xa7,0x25,0x95,0x60,0xc7,0x2c,0x69,
    0x5c,0xdc,0xd6,0xfd,0x31,0xe2,0xa4,0xc0,0xfe,0x53,0x6e,0xcd,0xd3,0x36,0x69,0x21
};
static const uint8_t BASE_Y_BYTES[32] = {
    0x58,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,
    0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66
};
/* q-2 and (q+3)/8, little-endian public exponents for fixed-bit exponentiations */
static const uint8_t EXP_QMINUS2[32] = {
    0xeb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f
};
static const uint8_t EXP_QPLUS3_8[32] = {
    0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0f
};

/* q as four little-endian 64-bit limbs */
static const uint64_t Q[4] = {
    0xffffffffffffffedULL, 0xffffffffffffffffULL,
    0xffffffffffffffffULL, 0x7fffffffffffffffULL
};

/* RFC 8032 group order L = 2^252 + 27742317777372353535851937790883648493,
 * four little-endian 64-bit limbs (little-endian hex of L is
 * ed d3 f5 5c 1a 63 12 58 6c d7 f7 9c de f9 de 14 00*8 00 00 00 00 00 00 00 10). */
static const uint64_t LIMBS_L[4] = {
    0x5812631a5cf5d3edULL, 0x14def9dea2f79cd6ULL,
    0x0000000000000000ULL, 0x1000000000000000ULL
};

static void add256(uint64_t r[4], const uint64_t a[4], const uint64_t b[4]) {
    u128 c = 0;
    for (int i = 0; i < 4; i++) {
        u128 t = (u128)a[i] + (u128)b[i] + c;
        r[i] = (uint64_t)t;
        c = t >> 64;
    }
}

/* returns 1 iff a < b (borrow out of bit 256) */
static int sub256(uint64_t r[4], const uint64_t a[4], const uint64_t b[4]) {
    u128 borrow = 0;
    for (int i = 0; i < 4; i++) {
        u128 t = (u128)a[i] - (u128)b[i] - borrow;
        r[i] = (uint64_t)t;
        borrow = (t >> 64) & 1;
    }
    return (int)borrow;
}

static void add_u128_at(uint64_t *acc, int words, u128 v, int off) {
    u128 carry = v;
    for (int w = off; w < words && carry; w++) {
        u128 t = (u128)acc[w] + (uint64_t)carry;
        carry = (carry >> 64) + (t >> 64);
        acc[w] = (uint64_t)t;
    }
}

/* Canonicalise a 256-bit value to [0, q).  Fold bit 255 (2^255 == 19), then
 * two mask-driven conditional subtractions of q; no data-dependent branch. */
static void fe_reduce(fe *r) {
    uint64_t x[4];
    memcpy(x, r->l, sizeof(x));
    uint64_t h = x[3] >> 63;
    x[3] &= LIMB_MASK63;
    add_u128_at((uint64_t *)x, 4, (u128)h * 19u, 0);
    for (int k = 0; k < 2; k++) {
        uint64_t y[4];
        int borrow = sub256(y, x, Q);
        uint64_t mask = (uint64_t)0 - (uint64_t)(1 - borrow); /* ~0 iff x >= q */
        for (int i = 0; i < 4; i++)
            x[i] = (x[i] & ~mask) | (y[i] & mask);
    }
    memcpy(r->l, x, sizeof(x));
}

static void fe_from_bytes(fe *r, const uint8_t b[32]) {
    for (int i = 0; i < 4; i++) {
        uint64_t v = 0;
        for (int j = 0; j < 8; j++) v |= (uint64_t)b[8 * i + j] << (8 * j);
        r->l[i] = v;
    }
    r->l[3] &= LIMB_MASK63;
    fe_reduce(r);
}

static void fe_to_bytes(uint8_t b[32], const fe *a) {
    fe t = *a;
    fe_reduce(&t);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 8; j++)
            b[8 * i + j] = (uint8_t)(t.l[i] >> (8 * j));
}

static void fe_add(fe *r, const fe *a, const fe *b) {
    uint64_t x[4];
    add256(x, a->l, b->l);
    memcpy(r->l, x, sizeof(x));
    fe_reduce(r);
}

static void fe_sub(fe *r, const fe *a, const fe *b) {
    uint64_t qmb[4], x[4];
    sub256(qmb, Q, b->l);   /* b is canonical, so q-b is exact */
    add256(x, a->l, qmb);
    memcpy(r->l, x, sizeof(x));
    fe_reduce(r);
}

static void fe_neg(fe *r, const fe *a) { fe_sub(r, &FE_ZERO, a); }

static void fe_mul(fe *r, const fe *a, const fe *b) {
    uint64_t p[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
        u128 carry = 0;
        for (int j = 0; j < 4; j++) {
            u128 t = (u128)a->l[i] * (u128)b->l[j] + (u128)p[i + j] + carry;
            p[i + j] = (uint64_t)t;
            carry = t >> 64;
        }
        p[i + 4] = (uint64_t)carry;
    }
    /* 2^256 == 38 (mod q): fold p[4..7] into a 6-limb accumulator.
     * p[4+k] sits at 2^(256+64k), i.e. 38*2^(64k) mod q. */
    uint64_t acc[6] = {p[0], p[1], p[2], p[3], 0, 0};
    for (int k = 4; k < 8; k++)
        add_u128_at(acc, 6, (u128)p[k] * 38u, k - 4);
    /* Capture the (small) limbs above 2^256 BEFORE modifying them, and put the
     * low window in canonical form first: a canonical limb 3 is < 2^63, so
     * adding the small fold below cannot carry into bit 256. */
    u128 hi = (u128)acc[4] * 38u + (u128)acc[5] * 2432u;
    fe low;
    memcpy(low.l, acc, 32);
    fe_reduce(&low);
    memcpy(acc, low.l, 32);
    acc[4] = 0; acc[5] = 0;
    add_u128_at(acc, 6, hi, 0);
    memcpy(r->l, acc, 32);
    fe_reduce(r);
}

static void fe_sq(fe *r, const fe *a) { fe_mul(r, a, a); }

static int fe_iszero(const fe *a) {
    uint64_t v = a->l[0] | a->l[1] | a->l[2] | a->l[3];
    return 1 - (int)((v | ((uint64_t)0 - v)) >> 63);
}

/* Constant-time mask select: r = (bit ? a : b) */
static void fe_select(fe *r, const fe *a, const fe *b, unsigned char bit) {
    uint64_t m = (uint64_t)0 - (uint64_t)bit;
    for (int i = 0; i < 4; i++)
        r->l[i] = (a->l[i] & m) | (b->l[i] & ~m);
}

/* Fixed-exponentiation with a public exponent: no secret-dependent branch. */
static void fe_exp(fe *r, const fe *a, const uint8_t exp[32]) {
    fe acc = FE_ONE;
    for (int i = 254; i >= 0; i--) {
        fe t;
        fe_sq(&acc, &acc);
        fe_mul(&t, &acc, a);
        fe_select(&acc, &t, &acc, (unsigned char)((exp[i >> 3] >> (i & 7)) & 1u));
    }
    *r = acc;
}

static void fe_inv(fe *r, const fe *a) { fe_exp(r, a, EXP_QMINUS2); }

/* -- projective point operations (complete formulas, a = -1) ------------ */
static void ge_identity(ge *p) {
    p->X = FE_ZERO; p->Y = FE_ONE; p->Z = FE_ONE; p->T = FE_ZERO;
}

static void ge_base(ge *p) {
    fe_from_bytes(&p->X, BASE_X_BYTES);
    fe_from_bytes(&p->Y, BASE_Y_BYTES);
    p->Z = FE_ONE;
    fe_mul(&p->T, &p->X, &p->Y);
}

/* add-2008-hwcd: works for p == q (doubling) too */
static void ge_add(ge *r, const ge *p, const ge *q) {
    fe t1, t2, A, B, C, D, E, F, G, H;
    fe_sub(&t1, &p->Y, &p->X);
    fe_sub(&t2, &q->Y, &q->X);
    fe_mul(&A, &t1, &t2);
    fe_add(&t1, &p->Y, &p->X);
    fe_add(&t2, &q->Y, &q->X);
    fe_mul(&B, &t1, &t2);
    fe_mul(&C, &p->T, &q->T);
    fe_mul(&C, &C, &FE_D);
    fe_add(&C, &C, &C);          /* 2*d*T1*T2 */
    fe_mul(&D, &p->Z, &q->Z);
    fe_add(&D, &D, &D);          /* 2*Z1*Z2 */
    fe_sub(&E, &B, &A);
    fe_sub(&F, &D, &C);
    fe_add(&G, &D, &C);
    fe_add(&H, &B, &A);
    fe_mul(&r->X, &E, &F);
    fe_mul(&r->Y, &G, &H);
    fe_mul(&r->T, &E, &H);
    fe_mul(&r->Z, &F, &G);
}

/* Mask-only conditional swap of all four coordinates (16 limbs).
 * `swap` must be 0 or 1, but even an out-of-range byte only widens the mask
 * and never introduces a data-dependent branch or memory index. */
static void wubu_ed25519_fixed_cswap(ge *a, ge *b, unsigned char swap) {
    uint64_t m = (uint64_t)0 - (uint64_t)(swap & 1u);
    fe *pairs[4][2] = {
        {&a->X, &b->X}, {&a->Y, &b->Y},
        {&a->Z, &b->Z}, {&a->T, &b->T}
    };
    for (int k = 0; k < 4; k++) {
        for (int i = 0; i < 4; i++) {
            uint64_t x = pairs[k][0]->l[i];
            uint64_t y = pairs[k][1]->l[i];
            pairs[k][0]->l[i] = (x & ~m) | (y & m);
            pairs[k][1]->l[i] = (y & ~m) | (x & m);
        }
    }
}

/* Constant-trip-count Montgomery ladder.  At loop top r0=[x]P, r1=[x+1]P up
 * to the pending swap, exactly as the mbedTLS ladder in wubu_pkg_ed25519.c. */
static void wubu_ed25519_fixed_ladder(ge *r, const uint8_t scalar[32],
                                      const ge *p) {
    ge r0, r1, sum, dbl;
    ge_identity(&r0);
    r1 = *p;
    unsigned char swap = 0, bit;
    for (unsigned int i = WUBU_ED25519_FIXED_LADDER_BITS; i > 0; --i) {
        bit = (unsigned char)((scalar[(i - 1) >> 3] >> ((i - 1) & 7)) & 1u);
        swap ^= bit;
        wubu_ed25519_fixed_cswap(&r0, &r1, swap);
        ge_add(&sum, &r0, &r1);
        ge_add(&dbl, &r0, &r0);
        r1 = sum;
        r0 = dbl;
        swap = bit;
    }
    wubu_ed25519_fixed_cswap(&r0, &r1, swap);
    *r = r0;
}

static int ge_frombytes(ge *p, const uint8_t enc[32]) {
    uint8_t buf[32];
    memcpy(buf, enc, 32);
    unsigned char sign = (unsigned char)(buf[31] >> 7);
    buf[31] &= 0x7f;
    fe y, yy, num, den, inv, x2, x, chk;
    fe_from_bytes(&y, buf);
    fe_sq(&yy, &y);
    fe_sub(&num, &yy, &FE_ONE);
    fe_mul(&den, &FE_D, &yy);       /* d*y^2 */
    fe_add(&den, &den, &FE_ONE);     /* d*y^2 + 1 */
    fe_inv(&inv, &den);
    fe_mul(&x2, &num, &inv);
    fe_exp(&x, &x2, EXP_QPLUS3_8);
    fe_sq(&chk, &x);
    fe_sub(&chk, &chk, &x2);
    if (!fe_iszero(&chk)) {
        fe_mul(&x, &x, &FE_SQRTM1);
        fe_sq(&chk, &x);
        fe_sub(&chk, &chk, &x2);
        if (!fe_iszero(&chk)) return -1;
    }
    unsigned char xodd = (unsigned char)(x.l[0] & 1u);
    fe nx;
    fe_neg(&nx, &x);
    fe_select(&x, &nx, &x, (unsigned char)(xodd ^ sign));
    /* reject y >= q (canonicality) and sign=1,x=0 */
    uint8_t yb[32];
    fe_to_bytes(yb, &y);
    if (memcmp(yb, buf, 32) != 0) return -1;
    if (sign && fe_iszero(&x)) return -1;
    p->X = x; p->Y = y; p->Z = FE_ONE;
    fe_mul(&p->T, &x, &y);
    return 0;
}

static void ge_tobytes(uint8_t enc[32], const ge *p) {
    fe inv, x, y;
    fe_inv(&inv, &p->Z);
    fe_mul(&x, &p->X, &inv);
    fe_mul(&y, &p->Y, &inv);
    fe_to_bytes(enc, &y);
    enc[31] &= 0x7f;
    enc[31] |= (uint8_t)((x.l[0] & 1u) << 7);
}

int wubu_ed25519_fixed_scalarmult(uint8_t out[32],
                                  const uint8_t scalar[32],
                                  const uint8_t point[32]) {
    if (!out || !scalar || !point) return -1;
    ge p, r;
    if (ge_frombytes(&p, point) != 0) return -1;
    wubu_ed25519_fixed_ladder(&r, scalar, &p);
    ge_tobytes(out, &r);
    return 0;
}

int wubu_ed25519_fixed_base_scalarmult(uint8_t out[32],
                                       const uint8_t scalar[32]) {
    if (!out || !scalar) return -1;
    ge p, r;
    ge_base(&p);
    wubu_ed25519_fixed_ladder(&r, scalar, &p);
    ge_tobytes(out, &r);
    return 0;
}

/* -- Fixed-width scalar arithmetic modulo L ------------------------------ *
 * These helpers replace the bignum mod-L calls on the default signing path.
 * Every loop has a compile-time-fixed trip count and every conditional is a
 * mask select, so no secret bit governs a branch or a memory index.
 */

static void load_le4(uint64_t w[4], const uint8_t b[32]) {
    for (int i = 0; i < 4; i++) {
        uint64_t v = 0;
        for (int j = 0; j < 8; j++) v |= (uint64_t)b[8 * i + j] << (8 * j);
        w[i] = v;
    }
}

static void store_le4(uint8_t b[32], const uint64_t w[4]) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 8; j++) b[8 * i + j] = (uint8_t)(w[i] >> (8 * j));
}

/* r = r mod L, by fixed bit-at-a-time long division.  r stays below 2^254,
 * hence four 64-bit limbs suffice throughout. */
static void scalar_reduce_le4(uint64_t r[4], const uint8_t in[64], size_t n) {
    for (int i = 0; i < 4; i++) r[i] = 0;
    for (size_t k = n * 8; k > 0; --k) {
        size_t q = k - 1;
        uint64_t carry = (in[q >> 3] >> (q & 7)) & 1u;
        for (int j = 0; j < 4; j++) {
            uint64_t next = r[j] >> 63;
            r[j] = (r[j] << 1) | carry;
            carry = next;
        }
        uint64_t t[4];
        int borrow = sub256(t, r, LIMBS_L);
        uint64_t mask = (uint64_t)0 - (uint64_t)(1 - borrow); /* ~0 iff r >= L */
        for (int j = 0; j < 4; j++) r[j] = (r[j] & ~mask) | (t[j] & mask);
    }
}

int wubu_ed25519_fixed_scalar_reduce(uint8_t out[32], const uint8_t in[64],
                                     size_t n) {
    if (!out || !in || (n != 32 && n != 64)) return -1;
    uint64_t r[4];
    scalar_reduce_le4(r, in, n);
    store_le4(out, r);
    return 0;
}

/* out = a * b as an exact eight-limb (512-bit) little-endian product. */
static void mul_256x256(uint64_t out[8], const uint64_t a[4], const uint64_t b[4]) {
    for (int i = 0; i < 8; i++) out[i] = 0;
    for (int i = 0; i < 4; i++) {
        u128 carry = 0;
        for (int j = 0; j < 4; j++) {
            u128 t = (u128)a[i] * (u128)b[j] + (u128)out[i + j] + carry;
            out[i + j] = (uint64_t)t;
            carry = t >> 64;
        }
        out[i + 4] = (uint64_t)carry;
    }
}

int wubu_ed25519_fixed_scalar_mul_add(uint8_t out[32], const uint8_t k[32],
                                      const uint8_t a[32], const uint8_t r[32]) {
    if (!out || !k || !a || !r) return -1;
    uint64_t kw[4], aw[4], rw[4], prod[8];
    load_le4(kw, k);
    load_le4(aw, a);
    load_le4(rw, r);
    mul_256x256(prod, kw, aw);
    u128 carry = 0;
    for (int i = 0; i < 4; i++) {
        u128 t = (u128)prod[i] + (u128)rw[i] + carry;
        prod[i] = (uint64_t)t;
        carry = t >> 64;
    }
    for (int i = 4; i < 8 && carry; i++) {
        u128 t = (u128)prod[i] + carry;
        prod[i] = (uint64_t)t;
        carry = t >> 64;
    }
    uint8_t buf[64];
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) buf[8 * i + j] = (uint8_t)(prod[i] >> (8 * j));
    return wubu_ed25519_fixed_scalar_reduce(out, buf, sizeof(buf));
}

int wubu_ed25519_fixed_scalar_ltL(const uint8_t s[32]) {
    if (!s) return 0;
    uint64_t w[4];
    load_le4(w, s);
    uint64_t t[4];
    return sub256(t, w, LIMBS_L) == 1;
}

/* -- Encoded-point addition ---------------------------------------------- */

int wubu_ed25519_fixed_add(uint8_t out[32], const uint8_t a[32],
                           const uint8_t b[32]) {
    if (!out || !a || !b) return -1;
    ge p, q, r;
    if (ge_frombytes(&p, a) != 0) return -1;
    if (ge_frombytes(&q, b) != 0) return -1;
    ge_add(&r, &p, &q);
    ge_tobytes(out, &r);
    return 0;
}
