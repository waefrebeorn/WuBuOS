/*
 * ed25519_timing_probe.c -- differential timing probe for the Ed25519 scalar
 * multiplication in src/gui/wubu_pkg_ed25519.c.  Built ONLY by
 * tools/ed25519_timing_gate.sh.
 *
 * The implementation source is included directly so this probe can time the
 * hardened Montgomery ladder (ge_mul_ladder) and the verbatim pre-change
 * double-and-add loop (ge_mul_doubleadd) in the same process, with identical
 * hardware/compiler conditions, then exercise the public sign/verify API over
 * many distinct secret scalars.
 *
 * This is a statistical differential gate, NOT a formal constant-time proof.
 * It bounds the observable correlation between a controlled secret's Hamming
 * weight and wall-clock time; mbedTLS bignum internals and compiler codegen
 * are out of scope and are called out as remaining gaps.
 */
#define WUBU_ED25519_TIMING_PROBE
#include "../src/gui/wubu_pkg_ed25519.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Deterministic xorshift64* -- every run uses the same controlled scalars. */
static uint64_t rng_s = 0x9e3779b97f4a7c15ULL;
static uint32_t rnd32(void) {
    rng_s ^= rng_s >> 12;
    rng_s ^= rng_s << 25;
    rng_s ^= rng_s >> 27;
    return (uint32_t)((rng_s * 0x2545F4914F6CDD1DULL) >> 32);
}

static double now_s(void) {
    struct timespec ts;
    /* Process CPU time, not wall clock: a scheduler deschedule or another
     * tenant on the box must not inflate the hardened ladder's spread.
     * The secret-dependent work is all CPU-bound, so this still measures
     * the scalar-multiply cost it is meant to bound. */
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int dbl_cmp(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static double median_of(double *v, int n) {
    qsort(v, (size_t)n, sizeof(v[0]), dbl_cmp);
    return v[n / 2];
}

/* Scalar with a FIXED bit length (256) and an exactly controlled Hamming
 * weight: bit 255 is always set, then the low (pc-1) bits are set.  Timing
 * therefore varies with popcount only, never with bit length. */
static void scalar_with_popcount(mbedtls_mpi *n, int pc) {
    mbedtls_mpi_lset(n, 0);
    mbedtls_mpi_set_bit(n, 255, 1);
    for (int b = 0; b < pc - 1; ++b)
        mbedtls_mpi_set_bit(n, (size_t)b, 1);
}

static void random_scalar_lt_L(mbedtls_mpi *n) {
    uint8_t b[32];
    for (int i = 0; i < 32; ++i) b[i] = (uint8_t)rnd32();
    mbedtls_mpi_read_binary_le(n, b, 32);
    mbedtls_mpi_mod_mpi(n, n, &L);
}

static int popcount_mpi(const mbedtls_mpi *n) {
    int c = 0;
    size_t bits = mbedtls_mpi_bitlen(n);
    for (size_t i = 0; i < bits; ++i) c += mbedtls_mpi_get_bit(n, i);
    return c;
}

static double pearson(const double *x, const double *y, int n) {
    double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
    for (int i = 0; i < n; ++i) {
        sx += x[i]; sy += y[i]; sxx += x[i] * x[i];
        syy += y[i] * y[i]; sxy += x[i] * y[i];
    }
    double dn = (double)n;
    double num = sxy - sx * sy / dn;
    double den = sqrt((sxx - sx * sx / dn) * (syy - sy * sy / dn));
    return den > 0 ? num / den : 0.0;
}

/* Two-tailed t statistic for H0: rho == 0, df = n-2. */
static double pearson_t(double r, int n) {
    if (r >= 1.0 || r <= -1.0) return HUGE_VAL;
    return r * sqrt((double)(n - 2) / (1.0 - r * r));
}

enum { M = 24, REPS = 96, TRIALS = 7, EQ_N = 48, API_N = 48 };

/* Median nanoseconds per scalar multiplication for `n`. */
static double batch_ns(int ladder, const ge *B, const mbedtls_mpi *n) {
    double t[TRIALS];
    ge out;
    ge_init(&out);
    for (int k = 0; k < TRIALS; ++k) {
        double s = now_s();
        for (int j = 0; j < REPS; ++j) {
            int rc = ladder ? ge_mul_ladder(&out, B, n)
                            : ge_mul_doubleadd(&out, B, n);
            if (rc) { ge_free(&out); return -1; }
        }
        t[k] = (now_s() - s) * 1e9 / REPS;
    }
    ge_free(&out);
    return median_of(t, TRIALS);
}

static void bytes_to_hex(const uint8_t *b, char *hex) {
    static const char d[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        hex[2 * i]     = d[b[i] >> 4];
        hex[2 * i + 1] = d[b[i] & 15];
    }
    hex[64] = 0;
}

int main(void) {
    if (!constants_init()) { fprintf(stderr, "constants_init failed\n"); return 2; }
    ge B;
    ge_init(&B);
    if (ge_base(&B)) { fprintf(stderr, "ge_base failed\n"); return 2; }

    /* 1) Functional equivalence: hardened ladder must equal the known-correct
     *    pre-change multiplication on many distinct random scalars. */
    int mismatch = 0;
    for (int i = 0; i < EQ_N; ++i) {
        mbedtls_mpi n;
        mbedtls_mpi_init(&n);
        random_scalar_lt_L(&n);
        ge a, b;
        ge_init(&a); ge_init(&b);
        if (ge_mul_ladder(&a, &B, &n) || ge_mul_doubleadd(&b, &B, &n) ||
            !ge_equal(&a, &b))
            mismatch++;
        ge_free(&a); ge_free(&b); mbedtls_mpi_free(&n);
    }
    printf("equiv_mismatches=%d/%d\n", mismatch, EQ_N);

    /* 2) Timing sweep over controlled secret scalars, in a randomized order
     *    so that any monotonic clock/thermal drift cannot masquerade as a
     *    correlation with popcount (which is monotonically indexed). */
    double pcs[M], old[M], neu[M];
    int order[M];
    for (int i = 0; i < M; ++i) order[i] = i;
    for (int i = M - 1; i > 0; --i) {
        int j = (int)(rnd32() % (uint32_t)(i + 1));
        int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
    }
    for (int k = 0; k < M; ++k) {
        int i = order[k];
        int pc = 8 + (247 * i) / (M - 1);   /* 8 .. 255 inclusive */
        mbedtls_mpi n;
        mbedtls_mpi_init(&n);
        scalar_with_popcount(&n, pc);
        pcs[k] = (double)popcount_mpi(&n);
        old[k] = batch_ns(0, &B, &n);
        neu[k] = batch_ns(1, &B, &n);
        mbedtls_mpi_free(&n);
    }
    double r_old = pearson(pcs, old, M);
    double r_new = pearson(pcs, neu, M);
    double pmin = pcs[0], pmax = pcs[0];
    double omin = old[0], omax = old[0], nmin = neu[0], nmax = neu[0];
    for (int i = 1; i < M; ++i) {
        if (pcs[i] < pmin) pmin = pcs[i];
        if (pcs[i] > pmax) pmax = pcs[i];
        if (old[i] < omin) omin = old[i];
        if (old[i] > omax) omax = old[i];
        if (neu[i] < nmin) nmin = neu[i];
        if (neu[i] > nmax) nmax = neu[i];
    }
    double t_old = pearson_t(r_old, M), t_new = pearson_t(r_new, M);
    printf("popcount_range=%.0f..%.0f (bitlen fixed at 256), n=%d reps=%d trials=%d, order randomized\n",
           pmin, pmax, M, REPS, TRIALS);
    printf("old_ns_per_mul=%.0f..%.0f ratio=%.2f r=%+.4f t=%+.2f\n",
           omin, omax, omax / omin, r_old, t_old);
    printf("new_ns_per_mul=%.0f..%.0f ratio=%.2f r=%+.4f t=%+.2f\n",
           nmin, nmax, nmax / nmin, r_new, t_new);

    /* 3) High-level public API over many distinct secret scalars/keys.
     *    `ge_mul` here is whichever implementation the build selected:
     *    ladder by default, double-and-add under WUBU_ED25519_LEGACY_MUL. */
    int sign_fail = 0, verify_fail = 0, tamper_pass = 0;
    uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN];
    memset(digest, 0x5a, sizeof(digest));
    for (int i = 0; i < API_N; ++i) {
        char seed_hex[65];
        for (int j = 0; j < 32; ++j) {
            uint8_t v = (uint8_t)rnd32();
            seed_hex[2 * j]     = "0123456789abcdef"[v >> 4];
            seed_hex[2 * j + 1] = "0123456789abcdef"[v & 15];
        }
        seed_hex[64] = 0;

        /* Derive the public key exactly as the signer does. */
        uint8_t seed[32], h[64], aenc[32];
        mbedtls_mpi a;
        ge A;
        mbedtls_mpi_init(&a);
        ge_init(&A);
        int ok = 0;
        if (hex_to_32(seed_hex, seed) == 0) {
            sha512_one(seed, 32, h);
            h[0] &= 248; h[31] &= 63; h[31] |= 64;
            if (scalar_from_le(&a, h, 32) == 0 &&
                ge_mul(&A, &B, &a) == 0 &&
                ge_tobytes(aenc, &A) == 0)
                ok = 1;
        }
        if (!ok) {
            sign_fail++;
            mbedtls_mpi_free(&a); ge_free(&A);
            continue;
        }
        char pub_hex[65];
        bytes_to_hex(aenc, pub_hex);

        uint8_t sig[WUBU_PKG_SIGNATURE_LEN];
        char err[256];
        if (!wubu_pkg_sign_ed25519(digest, seed_hex, sig, err, sizeof(err))) {
            sign_fail++;
        } else if (!wubu_pkg_sign_verify_ed25519(digest, pub_hex, sig, err,
                                                  sizeof(err))) {
            verify_fail++;
        } else {
            uint8_t bad[WUBU_PKG_SIGNATURE_LEN];
            memcpy(bad, sig, sizeof(bad));
            bad[7] ^= 1;
            if (wubu_pkg_sign_verify_ed25519(digest, pub_hex, bad, err,
                                              sizeof(err)))
                tamper_pass++;
        }
        mbedtls_mpi_free(&a); ge_free(&A);
    }
    printf("api_sign_fail=%d api_verify_fail=%d tamper_accepted=%d of %d\n",
           sign_fail, verify_fail, tamper_pass, API_N);

    ge_free(&B);

    /* Verdict.  OLD must show a strong, significant positive correlation
     * (its ge_add count IS the Hamming weight).  NEW is judged RELATIVE to
     * that baseline from the same run: hardening must remove a large part
     * of the leak.  An absolute r_new threshold is fragile under frequency
     * drift, which can move a flat ladder's measured r without reintrodu-
     * cing a secret-dependent operation count.  Functional equivalence and
     * the public API differential remain absolute. */
    int fail = 0;
    if (mismatch) {
        printf("FAIL: ladder differs from reference scalar multiplication\n");
        fail = 1;
    }
    if (r_old < 0.80 || t_old < 2.074 || omax / omin < 1.50) {
        printf("FAIL: negative control did not leak (r=%.4f t=%.2f ratio=%.2f)\n",
               r_old, t_old, omax / omin);
        fail = 1;
    }
    double delta_r = r_old - r_new;
    printf("delta_r=%.4f (old-minus-new)\n", delta_r);
    if (delta_r < 0.25 || t_new > t_old * 0.05 || nmax / nmin >= 1.25) {
        printf("FAIL: hardened ladder did not remove the baseline leak "
               "(delta_r=%.4f t_new=%.2f t_old=%.2f ratio=%.2f)\n",
               delta_r, t_new, t_old, nmax / nmin);
        fail = 1;
    }
    if (sign_fail || verify_fail || tamper_pass) {
        printf("FAIL: public API differential\n");
        fail = 1;
    }
    printf("%s\n", fail ? "TIMING_GATE FAILED" : "TIMING_GATE PASSED");
    return fail ? 1 : 0;
}
