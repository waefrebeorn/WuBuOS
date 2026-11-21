/*
 * ed25519_fixed_timing_probe.c -- dudect-style, NON-FORMAL statistical
 * timing-leakage gate for the SHIPPED DEFAULT fixed-width Ed25519 path.
 *
 * The product signs with WUBU_EDDSA_FIXED_LADDER (see mk/vars.mk), whose
 * arithmetic leaf is src/gui/wubu_ed25519_fixed.c -- bignum-free, 256-iteration
 * constant-trip-count ladder with mask-only conditional swaps.  This probe
 * #includes that exact leaf so it times the code that actually ships, with no
 * mbedTLS bignum and no network fetch.
 *
 * ONE process, ONE clock, ONE scalar set: for each secret class (fixed 256-bit
 * length, low vs high Hamming weight) the probe interleaves measurements of
 *   (a) the shipped ladder  -- wubu_ed25519_fixed_base_scalarmult()
 *   (b) a deliberately SECRET-DEPENDENT control -- an in-probe double-and-add
 *       with `if (bit) ge_add(...)`, i.e. a real variable-time path.
 * A single pass computes Welch's t for each.  Fixed thresholds, NO retries:
 *   |t_shipped| <= 10  -> shipped path shows no significant class-dependent leak
 *   |t_control| >  10  -> the control MUST be flagged (gate is non-vacuous)
 *
 * The control proves the detector can see leakage when it exists.  If the
 * control is not flagged the gate FAILS (vacuous detector).  This is empirical
 * evidence, NOT a formal constant-time proof: one host, one toolchain, process
 * CPU-time clock, and the mbedTLS SHA-512 used elsewhere in signing is out of
 * scope here (this module is the bignum-free arithmetic leaf only).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Real shipped arithmetic leaf.  All of its helpers (ge, ge_add, ge_base,
 * ge_identity, ge_tobytes, ...) are static and therefore visible below. */
#include "../src/gui/wubu_ed25519_fixed.c"

/* Fixed decision thresholds -- no adaptation, no re-run on a near miss. */
#define T_MAX_SHIPPED 10.0
#define T_MIN_CONTROL 10.0

enum { NSAMP = 64, REPS = 24, TRIALS = 3, EQ_N = 24 };
enum { PC_LOW = 8, PC_HIGH = 255 };

static uint64_t rng_s = 0x9e3779b97f4a7c15ULL;
static uint32_t rnd32(void) {
    rng_s ^= rng_s >> 12;
    rng_s ^= rng_s << 25;
    rng_s ^= rng_s >> 27;
    return (uint32_t)((rng_s * 0x2545F4914F6CDD1DULL) >> 32);
}

/* Process CPU time: scheduling noise from other tenants must not inflate the
 * spread.  The secret-dependent work is entirely CPU-bound. */
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

/* Fixed 256-bit length, exactly `pc` bits set: bit 255 always, then low bits.
 * Timing varies with Hamming weight only, never with bit length. */
static void scalar_with_popcount(uint8_t s[32], int pc) {
    memset(s, 0, 32);
    s[31] |= 0x80;                 /* bit 255 */
    for (int b = 0; b < pc - 1; ++b)
        s[(size_t)b >> 3] |= (uint8_t)(1u << (b & 7));
}

static void random_scalar(uint8_t s[32]) {
    for (int i = 0; i < 32; ++i) s[i] = (uint8_t)rnd32();
}

/* Welch's t for H0: mean(class A) == mean(class B), unequal variances. */
static double welch_t(const double *a, const double *b, int n) {
    double sa = 0, sb = 0;
    for (int i = 0; i < n; ++i) { sa += a[i]; sb += b[i]; }
    double ma = sa / n, mb = sb / n;
    double va = 0, vb = 0;
    for (int i = 0; i < n; ++i) {
        double da = a[i] - ma, db = b[i] - mb;
        va += da * da; vb += db * db;
    }
    va /= (n - 1); vb /= (n - 1);
    double se2 = va / n + vb / n;
    return se2 > 0 ? (ma - mb) / sqrt(se2) : 0.0;
}

/* (a) The shipped path: the exact API the signer calls to compute [secret]B. */
static double measure_shipped(const uint8_t s[32]) {
    uint8_t out[32];
    double t[TRIALS];
    for (int k = 0; k < TRIALS; ++k) {
        double st = now_s();
        for (int j = 0; j < REPS; ++j)
            if (wubu_ed25519_fixed_base_scalarmult(out, s) != 0) return -1.0;
        t[k] = (now_s() - st) * 1e9 / REPS;
    }
    return median3(t[0], t[1], t[2]);
}

/* (b) DELIBERATELY SECRET-DEPENDENT control: left-to-right double-and-add.
 *     One doubling every bit; one extra point add only when the secret bit
 *     is 1.  Work (and thus CPU time) tracks Hamming weight.  Uses the SAME
 *     field/point primitives from the included shipped leaf.  Never part of
 *     any product build -- it exists only inside this probe. */
static void variable_control(ge *r, const uint8_t s[32], const ge *p) {
    ge acc, q, t;
    ge_identity(&acc);
    q = *p;
    for (int i = 255; i >= 0; --i) {
        ge_add(&t, &acc, &acc);         /* unconditional doubling */
        acc = t;
        if ((s[(size_t)i >> 3] >> (i & 7)) & 1u) { /* SECRET-DEPENDENT BRANCH */
            ge_add(&t, &acc, &q);       /* extra work on secret 1 */
            acc = t;
        }
    }
    *r = acc;
}

static double measure_control(const uint8_t s[32]) {
    uint8_t out[32];
    ge p, r;
    ge_base(&p);
    double t[TRIALS];
    for (int k = 0; k < TRIALS; ++k) {
        double st = now_s();
        for (int j = 0; j < REPS; ++j) {
            variable_control(&r, s, &p);
            ge_tobytes(out, &r);       /* same encode work as shipped path */
        }
        t[k] = (now_s() - st) * 1e9 / REPS;
    }
    return median3(t[0], t[1], t[2]);
}

static void range(const double *v, int n, double *lo, double *hi) {
    *lo = *hi = v[0];
    for (int i = 1; i < n; ++i) {
        if (v[i] < *lo) *lo = v[i];
        if (v[i] > *hi) *hi = v[i];
    }
}

int main(int argc, char **argv) {
    int negctl = (argc > 1 && strcmp(argv[1], "--negctl") == 0);

    /* Functional equivalence: the variable control must compute the SAME
     * group element as the shipped ladder (so it is a fair baseline, not a
     * garbage path that differs for trivial reasons). */
    int mismatch = 0;
    for (int i = 0; i < EQ_N; ++i) {
        uint8_t s[32], a[32], b[32];
        random_scalar(s);
        ge p, r;
        ge_base(&p);
        if (wubu_ed25519_fixed_base_scalarmult(a, s) != 0) { mismatch++; continue; }
        variable_control(&r, s, &p);
        ge_tobytes(b, &r);
        if (memcmp(a, b, 32) != 0) mismatch++;
    }
    printf("equiv_mismatches=%d/%d\n", mismatch, EQ_N);

    /* Same-run, interleaved, deterministic-shuffled sampling of both paths
     * and both secret classes, so monotonic clock drift cannot manufacture a
     * Hamming-weight signal. */
    double sh_lo[NSAMP], sh_hi[NSAMP], vc_lo[NSAMP], vc_hi[NSAMP];
    int valid = 1;
    uint8_t slo[32], shi[32];
    scalar_with_popcount(slo, PC_LOW);
    scalar_with_popcount(shi, PC_HIGH);
    for (int i = 0; i < NSAMP; ++i) {
        int order[4] = { 0, 1, 2, 3 };
        for (int k = 3; k > 0; --k) {
            int j = (int)(rnd32() % (uint32_t)(k + 1));
            int tmp = order[k]; order[k] = order[j]; order[j] = tmp;
        }
        for (int k = 0; k < 4; ++k) {
            double v;
            switch (order[k]) {
                case 0: v = measure_shipped(slo); sh_lo[i] = v; break;
                case 1: v = measure_shipped(shi); sh_hi[i] = v; break;
                case 2: v = measure_control(slo);  vc_lo[i] = v; break;
                default:v = measure_control(shi);  vc_hi[i] = v; break;
            }
            if (v <= 0) valid = 0;
        }
    }

    double t_sh = welch_t(sh_lo, sh_hi, NSAMP);
    double t_vc = welch_t(vc_lo, vc_hi, NSAMP);
    double sh_ll, sh_lh, sh_hl, sh_hh, vc_ll, vc_lh, vc_hl, vc_hh;
    range(sh_lo, NSAMP, &sh_ll, &sh_lh);
    range(sh_hi, NSAMP, &sh_hl, &sh_hh);
    range(vc_lo, NSAMP, &vc_ll, &vc_lh);
    range(vc_hi, NSAMP, &vc_hl, &vc_hh);

    printf("fixed_timing_nsamps=%d reps=%d trials=%d pc=%d/%d interleaved=1\n",
           NSAMP, REPS, TRIALS, PC_LOW, PC_HIGH);
    printf("fixed_timing_shipped_t=%+.3f low=%.0f..%.0f high=%.0f..%.0f\n",
           t_sh, sh_ll, sh_lh, sh_hl, sh_hh);
    printf("fixed_timing_control_t=%+.3f low=%.0f..%.0f high=%.0f..%.0f\n",
           t_vc, vc_ll, vc_lh, vc_hl, vc_hh);
    printf("fixed_timing_thresholds: |t_shipped|<=%.1f |t_control|>%.1f (fixed, no retries)\n",
           T_MAX_SHIPPED, T_MIN_CONTROL);

    int fail = 0;
    if (mismatch) { printf("FAIL: variable control is not equivalent to shipped ladder\n"); fail = 1; }
    if (!valid)   { printf("FAIL: a timing sample was invalid\n"); fail = 1; }

    if (negctl) {
        /* Standalone negative control: require ONLY that the detector flags
         * the deliberately secret-dependent path. */
        if (fabs(t_vc) <= T_MIN_CONTROL) {
            printf("FAIL: negative control not flagged -- detector is vacuous (|t|=%.3f <= %.1f)\n",
                   fabs(t_vc), T_MIN_CONTROL);
            fail = 1;
        }
        printf("%s\n", fail ? "FIXED_TIMING_NEGCTL FAILED" : "FIXED_TIMING_NEGCTL PASSED");
        return fail ? 1 : 0;
    }

    if (fabs(t_sh) > T_MAX_SHIPPED) {
        printf("FAIL: shipped fixed path leaks (|t|=%.3f > %.1f)\n",
               fabs(t_sh), T_MAX_SHIPPED); fail = 1;
    }
    if (fabs(t_vc) <= T_MIN_CONTROL) {
        printf("FAIL: negative control not flagged -- gate is vacuous (|t|=%.3f <= %.1f)\n",
               fabs(t_vc), T_MIN_CONTROL); fail = 1;
    }
    printf("%s\n", fail ? "FIXED_TIMING_GATE FAILED" : "FIXED_TIMING_GATE PASSED");
    return fail ? 1 : 0;
}
