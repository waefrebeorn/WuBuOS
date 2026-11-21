/*
 * eddsa_dudect_probe.c -- dudect-style, NON-FORMAL statistical leakage gate
 * for the Ed25519 scalar multiplication used by the shipped signing path.
 *
 * Built ONLY by tools/eddsa_dudect_gate.sh.  The implementation source is
 * included directly so the probe times the shipped Montgomery ladder and the
 * verbatim pre-change double-and-add in ONE process, on one clock, over one
 * scalar set -- the same-run baseline a dudect-style test needs.  Samples for
 * the two secret classes (low vs high Hamming weight, fixed 256-bit length)
 * are interleaved with a deterministic shuffle so monotonic drift cannot
 * manufacture a signal.
 *
 * Verdict uses Welch's t statistic with FIXED thresholds and NO retries:
 *   shipped ladder   |t| <= DUDECT_T_MAX (10)  -> no significant leakage
 *   legacy negative  |t| >  DUDECT_T_MIN (10)  -> control MUST show leakage
 * A single measurement pass produces a single verdict.  This is empirical
 * evidence, NOT a formal constant-time proof (see the CT evidence doc).
 */
#define WUBU_ED25519_TIMING_PROBE
#include "../src/gui/wubu_pkg_ed25519.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* The included implementation calls this shared helper; keep the probe
 * self-contained instead of dragging in the whole package-signing stack. */
void wubu_pkg_sign_set_err(char *err, size_t errsz, const char *msg) {
    if (err && errsz) {
        strncpy(err, msg, errsz - 1);
        err[errsz - 1] = '\0';
    }
}

/* Fixed thresholds -- no adaptation, no re-run on a near miss. */
#define DUDECT_T_MAX 10.0
#define DUDECT_T_MIN 10.0

enum { NSAMP = 100, REPS = 64, TRIALS = 3, EQ_N = 24 };
#define PC_LOW 8
#define PC_HIGH 255

static uint64_t rng_s = 0x9e3779b97f4a7c15ULL;
static uint32_t rnd32(void) {
    rng_s ^= rng_s >> 12;
    rng_s ^= rng_s << 25;
    rng_s ^= rng_s >> 27;
    return (uint32_t)((rng_s * 0x2545F4914F6CDD1DULL) >> 32);
}

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int dbl_cmp(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static double median3(double a, double b, double c) {
    double v[3] = { a, b, c };
    qsort(v, 3, sizeof(v[0]), dbl_cmp);
    return v[1];
}

/* Fixed 256-bit length, exactly `pc` bits set.  Only Hamming weight varies. */
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

/* Median ns per scalar multiplication for this implementation and class. */
static double measure(int ladder, const ge *B, int pc) {
    mbedtls_mpi n;
    mbedtls_mpi_init(&n);
    scalar_with_popcount(&n, pc);
    ge out;
    ge_init(&out);
    double t[TRIALS];
    for (int k = 0; k < TRIALS; ++k) {
        double s = now_s();
        for (int j = 0; j < REPS; ++j) {
            int rc = ladder ? ge_mul_ladder(&out, B, &n)
                            : ge_mul_doubleadd(&out, B, &n);
            if (rc) { mbedtls_mpi_free(&n); ge_free(&out); return -1.0; }
        }
        t[k] = (now_s() - s) * 1e9 / REPS;
    }
    ge_free(&out);
    mbedtls_mpi_free(&n);
    return median3(t[0], t[1], t[2]);
}

/* Welch's t for H0: mean(class A) == mean(class B), unequal variances. */
static double welch_t(const double *a, int na, const double *b, int nb) {
    double sa = 0, sb = 0;
    for (int i = 0; i < na; ++i) sa += a[i];
    for (int i = 0; i < nb; ++i) sb += b[i];
    double ma = sa / na, mb = sb / nb;
    double va = 0, vb = 0;
    for (int i = 0; i < na; ++i) { double d = a[i] - ma; va += d * d; }
    for (int i = 0; i < nb; ++i) { double d = b[i] - mb; vb += d * d; }
    va /= (na - 1); vb /= (nb - 1);
    double se2 = va / na + vb / nb;
    return se2 > 0 ? (ma - mb) / sqrt(se2) : 0.0;
}

int main(void) {
    if (!constants_init()) { fprintf(stderr, "constants_init failed\n"); return 2; }
    ge B;
    ge_init(&B);
    if (ge_base(&B)) { fprintf(stderr, "ge_base failed\n"); return 2; }

    /* Functional equivalence: both paths compute the same group operation. */
    int mismatch = 0;
    for (int i = 0; i < EQ_N; ++i) {
        mbedtls_mpi n;
        mbedtls_mpi_init(&n);
        random_scalar_lt_L(&n);
        ge a, b;
        ge_init(&a); ge_init(&b);
        if (ge_mul_ladder(&a, &B, &n) || ge_mul_doubleadd(&b, &B, &n) ||
            !ge_equal(&a, &b)) mismatch++;
        ge_free(&a); ge_free(&b); mbedtls_mpi_free(&n);
    }
    printf("equiv_mismatches=%d/%d\n", mismatch, EQ_N);

    /* Same-run interleaved sampling of both implementations and classes. */
    double low[NSAMP], high[NSAMP], oldlow[NSAMP], oldhigh[NSAMP];
    int valid = 1;
    for (int i = 0; i < NSAMP; ++i) {
        if (rnd32() & 1u) {
            low[i]    = measure(1, &B, PC_LOW);
            high[i]   = measure(1, &B, PC_HIGH);
            oldlow[i] = measure(0, &B, PC_LOW);
            oldhigh[i]= measure(0, &B, PC_HIGH);
        } else {
            high[i]   = measure(1, &B, PC_HIGH);
            low[i]    = measure(1, &B, PC_LOW);
            oldhigh[i]= measure(0, &B, PC_HIGH);
            oldlow[i] = measure(0, &B, PC_LOW);
        }
        if (low[i] <= 0 || high[i] <= 0 || oldlow[i] <= 0 || oldhigh[i] <= 0)
            valid = 0;
    }
    double t_ladder = welch_t(low, NSAMP, high, NSAMP);
    double t_legacy = welch_t(oldlow, NSAMP, oldhigh, NSAMP);
    double lo_l = low[0], hi_l = low[0], lo_h = high[0], hi_h = high[0];
    double lo_ol = oldlow[0], hi_ol = oldlow[0], lo_oh = oldhigh[0], hi_oh = oldhigh[0];
    for (int i = 1; i < NSAMP; ++i) {
        if (low[i] < lo_l) lo_l = low[i]; if (low[i] > hi_l) hi_l = low[i];
        if (high[i] < lo_h) lo_h = high[i]; if (high[i] > hi_h) hi_h = high[i];
        if (oldlow[i] < lo_ol) lo_ol = oldlow[i]; if (oldlow[i] > hi_ol) hi_ol = oldlow[i];
        if (oldhigh[i] < lo_oh) lo_oh = oldhigh[i]; if (oldhigh[i] > hi_oh) hi_oh = oldhigh[i];
    }
    printf("dudect_nsamps=%d reps=%d trials=%d pc=%d/%d interleaved=1\n",
           NSAMP, REPS, TRIALS, PC_LOW, PC_HIGH);
    printf("dudect_ladder_t=%+.3f low=%.0f..%.0f high=%.0f..%.0f\n",
           t_ladder, lo_l, hi_l, lo_h, hi_h);
    printf("dudect_legacy_t=%+.3f low=%.0f..%.0f high=%.0f..%.0f\n",
           t_legacy, lo_ol, hi_ol, lo_oh, hi_oh);
    printf("dudect_thresholds: |t_ladder|<=%.1f |t_legacy|>%.1f (fixed, no retries)\n",
           DUDECT_T_MAX, DUDECT_T_MIN);

    int fail = 0;
    if (mismatch) { printf("FAIL: ladder and double-and-add disagree\n"); fail = 1; }
    if (!valid) { printf("FAIL: a timing sample was invalid\n"); fail = 1; }
    if (fabs(t_ladder) > DUDECT_T_MAX) {
        printf("FAIL: shipped ladder leaks (|t|=%.3f > %.1f)\n",
               fabs(t_ladder), DUDECT_T_MAX); fail = 1;
    }
    if (fabs(t_legacy) <= DUDECT_T_MIN) {
        printf("FAIL: negative control did NOT leak -- gate is vacuous (|t|=%.3f <= %.1f)\n",
               fabs(t_legacy), DUDECT_T_MIN); fail = 1;
    }
    printf("%s\n", fail ? "DUDECT_GATE FAILED" : "DUDECT_GATE PASSED");
    ge_free(&B);
    return fail ? 1 : 0;
}
