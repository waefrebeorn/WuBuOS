/*
 * wubu_math.h  --  WuBuOS Pure C Math Library API
 *
 * IEEE 754 compliant math functions without libm dependency.
 * Compile with -DWUBU_NO_LIBM to use these instead of libm.
 */

#ifndef WUBU_MATH_H
#define WUBU_MATH_H

#include <stdint.h>

/* -- Constants ---------------------------------------------------- */

#define WUBU_M_PI        3.14159265358979323846
#define WUBU_M_PI_2      1.57079632679489661923
#define WUBU_M_PI_4      0.78539816339744830962
#define WUBU_M_2_PI      0.63661977236758134308
#define WUBU_M_E         2.71828182845904523536
#define WUBU_M_LOG2_E    1.44269504088896340736
#define WUBU_M_LN2       0.69314718055994530942
#define WUBU_M_SQRT2     1.41421356237309504880
#define WUBU_M_SQRT1_2   0.70710678118654752440

/* Aliases for backward compatibility */
#define WUBU_PI          WUBU_M_PI
#define WUBU_PI_2        WUBU_M_PI_2
#define WUBU_PI_4        WUBU_M_PI_4
#define WUBU_2_PI        WUBU_M_2_PI
#define WUBU_E           WUBU_M_E
#define WUBU_LOG2_E      WUBU_M_LOG2_E
#define WUBU_LN2         WUBU_M_LN2
#define WUBU_SQRT2       WUBU_M_SQRT2
#define WUBU_SQRT1_2     WUBU_M_SQRT1_2

/* -- Double Precision --------------------------------------------- */

double wubu_sqrt(double x);
double wubu_log(double x);
double wubu_pow(double x, double y);
double wubu_exp(double x);
double wubu_sin(double x);
double wubu_cos(double x);
double wubu_atan2(double y, double x);
double wubu_ceil(double x);
double wubu_floor(double x);
double wubu_fabs(double x);
double wubu_fmod(double x, double y);
double wubu_trunc(double x);
double wubu_round(double x);
void wubu_sincos(double x, double *sin, double *cos);

/* -- Single Precision --------------------------------------------- */

float wubu_sqrtf(float x);
float wubu_logf(float x);
float wubu_powf(float x, float y);
float wubu_expf(float x);
float wubu_sinf(float x);
float wubu_cosf(float x);
float wubu_atan2f(float y, float x);
float wubu_ceilf(float x);
float wubu_floorf(float x);
float wubu_fabsf(float x);
float wubu_fmodf(float x, float y);
float wubu_truncf(float x);
float wubu_roundf(float x);

/* -- Compatibility macros (when WUBU_NO_LIBM defined) ------------ */

#ifdef WUBU_NO_LIBM
#define sqrt      wubu_sqrt
#define log       wubu_log
#define pow       wubu_pow
#define exp       wubu_exp
#define sin       wubu_sin
#define cos       wubu_cos
#define atan2     wubu_atan2
#define ceil      wubu_ceil
#define floor     wubu_floor
#define fabs      wubu_fabs
#define fmod      wubu_fmod
#define trunc     wubu_trunc
#define round     wubu_round
#define sqrtf     wubu_sqrtf
#define logf      wubu_logf
#define powf      wubu_powf
#define expf      wubu_expf
#define sinf      wubu_sinf
#define cosf      wubu_cosf
#define atan2f    wubu_atan2f
#define ceilf     wubu_ceilf
#define floorf    wubu_floorf
#define fabsf     wubu_fabsf
#define fmodf     wubu_fmodf
#define truncf    wubu_truncf
#define roundf    wubu_roundf
#define M_PI      WUBU_M_PI
#define M_PI_2    WUBU_M_PI_2
#define M_PI_4    WUBU_M_PI_4
#define M_2_PI    WUBU_M_2_PI
#define M_E       WUBU_M_E
#define M_LOG2_E  WUBU_M_LOG2_E
#define M_LN2     WUBU_M_LN2
#define M_SQRT2   WUBU_M_SQRT2
#define M_SQRT1_2 WUBU_M_SQRT1_2
#endif

#endif /* WUBU_MATH_H */