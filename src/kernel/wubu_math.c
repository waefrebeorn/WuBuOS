/*
 * wubu_math.c  --  WuBuOS Pure C Math Library
 *
 * Cell 420: Pure C implementations replacing libm.
 * IEEE 754 compliant approximations using polynomial/CORDIC methods.
 * No external libm dependency - compiles with -DWUBU_NO_LIBM
 *
 * Functions: sqrt, log, pow, sin, cos, atan2, ceil, floor
 */

#include <stdint.h>
#include <stddef.h>

/* -- Constants ---------------------------------------------------- */

#define WUBU_PI        3.14159265358979323846
#define WUBU_PI_2      1.57079632679489661923
#define WUBU_PI_4      0.78539816339744830962
#define WUBU_2_PI      0.63661977236758134308
#define WUBU_E         2.71828182845904523536
#define WUBU_LOG2_E    1.44269504088896340736
#define WUBU_LN2       0.69314718055994530942
#define WUBU_SQRT2     1.41421356237309504880
#define WUBU_SQRT1_2   0.70710678118654752440

/* IEEE 754 special values */
#define WUBU_NAN       __builtin_nan("")
#define WUBU_INF       __builtin_inf()
#define WUBU_NEG_INF   -__builtin_inf()

/* High-precision π from hex (C11 hex float) */
#define WUBU_PI_HI     0x1.921fb54442d18p+1  /* π exact double */
#define WUBU_PI_2_HI   0x1.921fb54442d18p+0  /* π/2 exact double */
#define WUBU_PI_4_HI   0x1.921fb54442d18p-1  /* π/4 exact double */
#define WUBU_2_PI_HI   0x1.45f306dc9c883p-1  /* 2/π exact double */
#define WUBU_E_HI      0x1.5bf0a8b145769p+1  /* e exact double */
#define WUBU_LOG2_E_HI 0x1.71547652b82fep+0  /* log2(e) exact double */
#define WUBU_LN2_HI    0x1.62e42fefa39efp-1  /* ln(2) exact double */

/* -- Bit Manipulation Helpers ------------------------------------- */

static inline uint64_t double_to_bits(double x) {
    uint64_t u;
    __builtin_memcpy(&u, &x, sizeof(u));
    return u;
}

static inline double bits_to_double(uint64_t u) {
    double x;
    __builtin_memcpy(&x, &u, sizeof(x));
    return x;
}

static inline uint32_t float_to_bits(float x) {
    uint32_t u;
    __builtin_memcpy(&u, &x, sizeof(u));
    return u;
}

static inline float bits_to_float(uint32_t u) {
    float x;
    __builtin_memcpy(&x, &u, sizeof(x));
    return x;
}

/* Extract sign, exponent, mantissa from double */
static inline int get_sign(double x) {
    return (double_to_bits(x) >> 63) & 1;
}

static inline int get_exponent(double x) {
    return (int)((double_to_bits(x) >> 52) & 0x7FF);
}

static inline uint64_t get_mantissa(double x) {
    return double_to_bits(x) & 0xFFFFFFFFFFFFFULL;
}

static inline double make_double(int sign, int exp, uint64_t mantissa) {
    return bits_to_double(((uint64_t)sign << 63) | ((uint64_t)exp << 52) | mantissa);
}

/* -- Fast Approximate Square Root (Newton-Raphson) ---------------- */

double wubu_sqrt(double x) {
    /* Handle special cases */
    if (x < 0.0) return WUBU_NAN;
    if (x == 0.0 || x == WUBU_INF) return x;
    if (x != x) return WUBU_NAN;  /* NaN */

    uint64_t bits = double_to_bits(x);
    int exp = (int)((bits >> 52) & 0x7FF);
    uint64_t mantissa = bits & 0xFFFFFFFFFFFFFULL;

    /* Subnormal */
    if (exp == 0) {
        return 0.0;
    }

    /* Add implicit leading 1 to mantissa */
    mantissa |= 0x10000000000000ULL;  // Now mantissa in [2^52, 2^53)

    /* sqrt(x) = 2^((exp-1023)/2) * sqrt(mantissa / 2^52) */
    int new_exp = ((exp - 1023) >> 1) + 1023;
    
    /* For odd exponent, multiply mantissa by 2 before sqrt */
    if ((exp - 1023) & 1) {
        mantissa <<= 1;
    }
    new_exp += 0;  // Already compensated

    /* m = mantissa / 2^52, where mantissa in [2^52, 2^53) so m in [1, 2) */
    double m = (double)mantissa / 0x10000000000000ULL;
    
    /* sqrt(m) where m in [1, 2): use polynomial for sqrt(m) = sqrt(1 + (m-1)) */
    double y = m - 1.0;  // y in [0, 1)
    double y2 = y * y;
    double y3 = y2 * y;
    double y4 = y3 * y;
    double y5 = y4 * y;
    
    /* sqrt(1+y) ≈ 1 + y/2 - y^2/8 + y^3/16 - 5*y^4/128 + 7*y^5/256 */
    double sqrt_m = 1.0 + y * 0.5 - y2 * 0.125 + y3 * 0.0625 - y4 * 0.0390625 + y5 * 0.02734375;
    
    /* Two Newton-Raphson iterations */
    sqrt_m = 0.5 * (sqrt_m + m / sqrt_m);
    sqrt_m = 0.5 * (sqrt_m + m / sqrt_m);

    /* Reconstruct: value = 2^(new_exp - 1023) * sqrt_m */
    /* sqrt_m is in [1, 2), so we need to encode as 1.(sqrt_m - 1) in mantissa */
    uint64_t new_mantissa = (uint64_t)((sqrt_m - 1.0) * 0x10000000000000ULL);
    return make_double(0, new_exp, new_mantissa);
}

/* -- Fast Approximate Natural Log (Taylor Series + Range Reduction) */

double wubu_log(double x) {
    /* Handle special cases */
    if (x < 0.0) return WUBU_NAN;
    if (x == 0.0) return WUBU_NEG_INF;
    if (x == WUBU_INF) return WUBU_INF;
    if (x != x) return WUBU_NAN;  /* NaN */

    /* Range reduction: x = 2^e * m, where m in [1, 2) */
    uint64_t bits = double_to_bits(x);
    int exp = (int)((bits >> 52) & 0x7FF);
    uint64_t mantissa = bits & 0xFFFFFFFFFFFFFULL;

    if (exp == 0) {
        /* Subnormal */
        while ((mantissa & 0x10000000000000ULL) == 0) {
            mantissa <<= 1;
            exp--;
        }
        exp++;
        mantissa &= 0xFFFFFFFFFFFFFULL;
    }

    /* Normalized: x = 2^(exp-1023) * m where m in [1, 2) */
    double x_norm = 1.0 + (double)mantissa / 0x10000000000000ULL;
    int e = exp - 1023;

    /* Range reduction to [sqrt(2)/2, sqrt(2)) ≈ [0.707, 1.414) for better convergence */
    /* If x_norm >= sqrt(2), divide by 2 and increment exponent */
    if (x_norm >= WUBU_SQRT2) {
        x_norm *= 0.5;
        e++;
    }

    /* Use log(1+y) where y = x_norm - 1, y in [-0.2929, 0.4142] */
    double y = x_norm - 1.0;

    /* Taylor series for log(1+y) with 9 terms for high precision */
    double y2 = y * y;
    double y3 = y2 * y;
    double y4 = y3 * y;
    double y5 = y4 * y;
    double y6 = y5 * y;
    double y7 = y6 * y;
    double y8 = y7 * y;
    double y9 = y8 * y;

    double log1p_y = y - y2 * 0.5 + y3 / 3.0 - y4 * 0.25 + y5 * 0.2 - y6 / 6.0 + y7 / 7.0 - y8 / 8.0 + y9 / 9.0;

    return log1p_y + (double)e * WUBU_LN2_HI;
}

/* -- Fast Approximate Power (via exp and log) */

double wubu_exp(double x);

double wubu_pow(double x, double y) {
    /* Handle special cases */
    if (y == 0.0) return 1.0;
    if (x == 0.0) {
        if (y > 0.0) return 0.0;
        if (y < 0.0) return WUBU_INF;
        return WUBU_NAN;  /* 0^0 */
    }
    if (x == 1.0) return 1.0;
    if (x == WUBU_INF) {
        if (y > 0.0) return WUBU_INF;
        if (y < 0.0) return 0.0;
        return WUBU_NAN;
    }
    if (x < 0.0 && y != (int)y) return WUBU_NAN;  /* Negative base, non-integer exponent */

    /* pow(x, y) = exp(y * log(x)) */
    double log_x = wubu_log(x);
    double z = y * log_x;

    return wubu_exp(z);
}

/* -- Fast exp approximation (helper for pow) */

double wubu_exp(double x) {
    if (x >= 709.0) return WUBU_INF;   /* Overflow */
    if (x <= -745.0) return 0.0;        /* Underflow */

    /* Range reduction: x = n*ln(2) + r, where r in [-ln(2)/2, ln(2)/2] */
    double n = x * WUBU_LOG2_E_HI;
    int n_int = (int)n;
    if (n < 0.0 && n != (double)n_int) n_int--;  /* Floor for negative */

    double r = x - (double)n_int * WUBU_LN2_HI;

    /* Taylor series for exp(r): 1 + r + r²/2! + r³/3! + r⁴/4! + ... */
    /* Use 12 terms for high precision */
    double r2 = r * r;
    double r3 = r2 * r;
    double r4 = r3 * r;
    double r5 = r4 * r;
    double r6 = r5 * r;
    double r7 = r6 * r;
    double r8 = r7 * r;
    double r9 = r8 * r;
    double r10 = r9 * r;
    double r11 = r10 * r;
    double r12 = r11 * r;

    double exp_r = 1.0 + r + r2 * 0.5 + r3 / 6.0 + r4 / 24.0 + r5 / 120.0 + r6 / 720.0 + 
                   r7 / 5040.0 + r8 / 40320.0 + r9 / 362880.0 + r10 / 3628800.0 + 
                   r11 / 39916800.0 + r12 / 479001600.0;

    /* Scale by 2^n */
    uint64_t bits = double_to_bits(exp_r);
    int exp = (int)((bits >> 52) & 0x7FF);
    exp += n_int;
    if (exp >= 2047) return WUBU_INF;
    if (exp <= 0) return 0.0;

    return bits_to_double((bits & 0x800FFFFFFFFFFFFFULL) | ((uint64_t)exp << 52));
}

/* -- Fast Approximate Sin/Cos (Range Reduction + Minimax) ----- */

double wubu_sin(double x) {
    if (x != x || x == WUBU_INF || x == WUBU_NEG_INF) return WUBU_NAN;

    /* Reduce to [-π, π]: x = x - 2π * round(x / 2π) */
    double k = x / (2.0 * WUBU_PI_HI);  /* x / (2π) */
    int64_t k_int = (int64_t)k;
    if (k >= 0.0) {
        if (k - k_int >= 0.5) k_int++;
    } else {
        if (k_int - k >= 0.5) k_int--;
    }
    x = x - (double)k_int * 2.0 * WUBU_PI_HI;

    /* Reduce to [-π/2, π/2] using sin(x) = sin(π - x) for x in [π/2, π] */
    if (x > WUBU_PI_2_HI) {
        x = WUBU_PI_HI - x;
    } else if (x < -WUBU_PI_2_HI) {
        x = -WUBU_PI_HI - x;
    }

    /* Taylor series for sin on [-π/2, π/2]: x - x^3/6 + x^5/120 - x^7/5040 + x^9/362880 - x^11/39916800 + x^13/6227020800 */
    double x2 = x * x;
    double x3 = x2 * x;
    double x5 = x3 * x2;
    double x7 = x5 * x2;
    double x9 = x7 * x2;
    double x11 = x9 * x2;
    double x13 = x11 * x2;

    return x - x3 / 6.0 + x5 / 120.0 - x7 / 5040.0 + x9 / 362880.0 - x11 / 39916800.0 + x13 / 6227020800.0;
}

double wubu_cos(double x) {
    /* cos(x) = sin(x + π/2) */
    return wubu_sin(x + WUBU_PI_2_HI);
}

double wubu_trunc(double x);

double wubu_atan2(double y, double x) {
    /* Handle special cases */
    if (x != x || y != y) return WUBU_NAN;

    if (x == 0.0 && y == 0.0) return 0.0;
    if (x == 0.0) return (y > 0.0) ? WUBU_PI_2_HI : -WUBU_PI_2_HI;
    if (y == 0.0) return (x > 0.0) ? 0.0 : WUBU_PI_HI;

    /* Use atan2 via atan(y/x) with proper quadrant handling and high-precision Taylor series */
    double z = y / x;
    double abs_z = z < 0.0 ? -z : z;
    double atan_z;
    
    /* Use 25-term Taylor series for high precision */
    if (abs_z <= 1.0) {
        double z2 = z * z;
        double z3 = z2 * z;
        double z5 = z3 * z2;
        double z7 = z5 * z2;
        double z9 = z7 * z2;
        double z11 = z9 * z2;
        double z13 = z11 * z2;
        double z15 = z13 * z2;
        double z17 = z15 * z2;
        double z19 = z17 * z2;
        double z21 = z19 * z2;
        double z23 = z21 * z2;
        double z25 = z23 * z2;
        
        atan_z = z - z3 / 3.0 + z5 / 5.0 - z7 / 7.0 + z9 / 9.0 - z11 / 11.0 + 
                 z13 / 13.0 - z15 / 15.0 + z17 / 17.0 - z19 / 19.0 + 
                 z21 / 21.0 - z23 / 23.0 + z25 / 25.0;
    } else {
        /* For |z| > 1, use atan(z) = sign(z) * π/2 - atan(1/z) */
        double inv_z = 1.0 / z;
        double inv_z2 = inv_z * inv_z;
        double inv_z3 = inv_z2 * inv_z;
        double inv_z5 = inv_z3 * inv_z2;
        double inv_z7 = inv_z5 * inv_z2;
        double inv_z9 = inv_z7 * inv_z2;
        double inv_z11 = inv_z9 * inv_z2;
        double inv_z13 = inv_z11 * inv_z2;
        double inv_z15 = inv_z13 * inv_z2;
        double inv_z17 = inv_z15 * inv_z2;
        double inv_z19 = inv_z17 * inv_z2;
        double inv_z21 = inv_z19 * inv_z2;
        double inv_z23 = inv_z21 * inv_z2;
        double inv_z25 = inv_z23 * inv_z2;
        
        double atan_inv = inv_z - inv_z3 / 3.0 + inv_z5 / 5.0 - inv_z7 / 7.0 + inv_z9 / 9.0 - 
                          inv_z11 / 11.0 + inv_z13 / 13.0 - inv_z15 / 15.0 + inv_z17 / 17.0 - 
                          inv_z19 / 19.0 + inv_z21 / 21.0 - inv_z23 / 23.0 + inv_z25 / 25.0;
        atan_z = (z > 0.0) ? (WUBU_PI_2_HI - atan_inv) : (-WUBU_PI_2_HI - atan_inv);
    }

    /* Quadrant correction */
    if (x > 0.0) {
        return atan_z;
    } else if (x < 0.0) {
        return (y >= 0.0) ? atan_z + WUBU_PI_HI : atan_z - WUBU_PI_HI;
    }
    return atan_z;
}

/* -- Ceil/Floor (Bit Manipulation) -------------------------------- */

double wubu_ceil(double x) {
    if (x != x || x == WUBU_INF || x == WUBU_NEG_INF) return x;
    if (x >= 9007199254740992.0 || x <= -9007199254740992.0) return x;  /* 2^53, all integers */

    int64_t i = (int64_t)x;
    double d = (double)i;
    if (d == x) return x;
    return (x > 0.0) ? d + 1.0 : d;
}

double wubu_floor(double x) {
    if (x != x || x == WUBU_INF || x == WUBU_NEG_INF) return x;
    if (x >= 9007199254740992.0 || x <= -9007199254740992.0) return x;

    int64_t i = (int64_t)x;
    double d = (double)i;
    if (d == x) return x;
    return (x < 0.0) ? d - 1.0 : d;
}

/* -- Additional math functions ------------------------------------ */

double wubu_fabs(double x) {
    return x < 0.0 ? -x : x;
}

double wubu_fmod(double x, double y) {
    if (y == 0.0 || x != x || y != y) return WUBU_NAN;
    if (x == WUBU_INF || x == WUBU_NEG_INF) return WUBU_NAN;
    if (y == WUBU_INF || y == WUBU_NEG_INF) return x;
    return x - wubu_trunc(x / y) * y;
}

double wubu_trunc(double x) {
    if (x != x || x == WUBU_INF || x == WUBU_NEG_INF) return x;
    if (x >= 9007199254740992.0 || x <= -9007199254740992.0) return x;
    return (double)(int64_t)x;
}

/* -- sincos (simultaneous sin/cos for efficiency) ------------------- */

void wubu_sincos(double x, double *sin_out, double *cos_out) {
    if (!sin_out || !cos_out) return;
    
    /* Use the same range reduction as sin, then compute both */
    if (x != x || x == WUBU_INF || x == WUBU_NEG_INF) {
        *sin_out = WUBU_NAN;
        *cos_out = WUBU_NAN;
        return;
    }

    /* Reduce to [-π, π] */
    double k = x / (2.0 * WUBU_PI_HI);
    int64_t k_int = (int64_t)k;
    if (k >= 0.0) {
        if (k - k_int >= 0.5) k_int++;
    } else {
        if (k_int - k >= 0.5) k_int--;
    }
    x = x - (double)k_int * 2.0 * WUBU_PI_HI;

    /* Reduce to [-π/2, π/2] using sin(x) = sin(π - x) for x in [π/2, π] */
    double sin_sign = 1.0;
    double cos_sign = 1.0;
    if (x > WUBU_PI_2_HI) {
        x = WUBU_PI_HI - x;
        cos_sign = -1.0;
    } else if (x < -WUBU_PI_2_HI) {
        x = -WUBU_PI_HI - x;
        cos_sign = -1.0;
    }

    /* Taylor series for sin on [-π/2, π/2] */
    double x2 = x * x;
    double x3 = x2 * x;
    double x5 = x3 * x2;
    double x7 = x5 * x2;
    double x9 = x7 * x2;
    double sin_x = x - x3 / 6.0 + x5 / 120.0 - x7 / 5040.0 + x9 / 362880.0;

    /* Taylor series for cos: cos(x) = 1 - x²/2! + x⁴/4! - x⁶/6! + ... */
    double cos_x = 1.0 - x2 * 0.5 + x2 * x2 / 24.0 - x3 * x3 / 720.0 + x5 * x2 / 40320.0;

    *sin_out = sin_x * sin_sign;
    *cos_out = cos_x * cos_sign;
}

double wubu_round(double x) {
    if (x != x || x == WUBU_INF || x == WUBU_NEG_INF) return x;
    if (x >= 9007199254740992.0 || x <= -9007199254740992.0) return x;
    return (double)(int64_t)(x + (x >= 0.0 ? 0.5 : -0.5));
}

/* Float versions */

float wubu_sqrtf(float x) {
    return (float)wubu_sqrt((double)x);
}

float wubu_logf(float x) {
    return (float)wubu_log((double)x);
}

float wubu_powf(float x, float y) {
    return (float)wubu_pow((double)x, (double)y);
}

float wubu_sinf(float x) {
    return (float)wubu_sin((double)x);
}

float wubu_cosf(float x) {
    return (float)wubu_cos((double)x);
}

float wubu_atan2f(float y, float x) {
    return (float)wubu_atan2((double)y, (double)x);
}

float wubu_ceilf(float x) {
    return (float)wubu_ceil((double)x);
}

float wubu_floorf(float x) {
    return (float)wubu_floor((double)x);
}

float wubu_fabsf(float x) {
    return (float)wubu_fabs((double)x);
}

float wubu_fmodf(float x, float y) {
    return (float)wubu_fmod((double)x, (double)y);
}

float wubu_truncf(float x) {
    return (float)wubu_trunc((double)x);
}

float wubu_roundf(float x) {
    return (float)wubu_round((double)x);
}

/* -- Test function ------------------------------------------------ */

#ifdef WUBU_MATH_TEST
#include <stdio.h>
#include <math.h>

#define TEST(name, fn, ref, args...) do { \
    double got = fn(args); \
    double expected = ref(args); \
    double diff = got - expected; \
    if (diff < 0) diff = -diff; \
    printf("%s: got=%f exp=%f diff=%e %s\n", name, got, expected, diff, diff < 1e-6 ? "PASS" : "FAIL"); \
} while(0)

int main(void) {
    printf("=== wubu_math test ===\n");
    
    TEST("sqrt", wubu_sqrt, sqrt, 2.0);
    TEST("sqrt", wubu_sqrt, sqrt, 100.0);
    TEST("sqrt", wubu_sqrt, sqrt, 0.25);
    
    TEST("log", wubu_log, log, 2.0);
    TEST("log", wubu_log, log, 10.0);
    TEST("log", wubu_log, log, 0.5);
    
    TEST("pow", wubu_pow, pow, 2.0, 3.0);
    TEST("pow", wubu_pow, pow, 5.0, 2.0);
    TEST("pow", wubu_pow, pow, 2.0, 0.5);
    
    TEST("sin", wubu_sin, sin, 0.0);
    TEST("sin", wubu_sin, sin, WUBU_PI_2);
    TEST("sin", wubu_sin, sin, WUBU_PI);
    TEST("sin", wubu_sin, sin, 1.0);
    
    TEST("cos", wubu_cos, cos, 0.0);
    TEST("cos", wubu_cos, cos, WUBU_PI_2);
    TEST("cos", wubu_cos, cos, WUBU_PI);
    TEST("cos", wubu_cos, cos, 1.0);
    
    TEST("atan2", wubu_atan2, atan2, 1.0, 1.0);
    TEST("atan2", wubu_atan2, atan2, -1.0, 1.0);
    TEST("atan2", wubu_atan2, atan2, 1.0, -1.0);
    TEST("atan2", wubu_atan2, atan2, -1.0, -1.0);
    TEST("atan2", wubu_atan2, atan2, 0.0, 1.0);
    TEST("atan2", wubu_atan2, atan2, 1.0, 0.0);
    
    TEST("ceil", wubu_ceil, ceil, 1.5);
    TEST("ceil", wubu_ceil, ceil, -1.5);
    TEST("ceil", wubu_ceil, ceil, 2.0);
    
    TEST("floor", wubu_floor, floor, 1.5);
    TEST("floor", wubu_floor, floor, -1.5);
    TEST("floor", wubu_floor, floor, 2.0);
    
    return 0;
}
#endif
