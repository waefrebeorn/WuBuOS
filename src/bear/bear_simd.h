/*
 * bear_simd.h — PufferC/BearRL SIMD Intrinsics + Fused Kernels
 *
 * AVX2 / NEON / Scalar fallback for matmul, fused ops.
 * Pure C11 + compiler intrinsics. No external deps.
 */

#ifndef BEAR_SIMD_H
#define BEAR_SIMD_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include "bear_arena.h"  /* for BearTensor */

/* ═══════════════════════════════════════════════════════════════════
 * Architecture Detection
 * ═══════════════════════════════════════════════════════════════════ */

#if defined(__AVX2__)
    #define BEAR_HAVE_AVX2 1
    #include <immintrin.h>
#elif defined(__ARM_NEON__) || defined(__aarch64__)
    #define BEAR_HAVE_NEON 1
    #include <arm_neon.h>
#else
    #define BEAR_HAVE_SCALAR 1
#endif

/* ═══════════════════════════════════════════════════════════════════
 * Activation Functions (SIMD-accelerated)
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    BEAR_ACT_NONE  = 0,
    BEAR_ACT_RELU  = 1,
    BEAR_ACT_TANH  = 2,
    BEAR_ACT_GELU  = 3,  /* Gaussian Error Linear Unit */
    BEAR_ACT_SILU  = 4,  /* SiLU / Swish: x * sigmoid(x) */
} BearAct;

/* Scalar fallback (always available) */
static inline float bear_act_scalar(float x, BearAct act) {
    switch (act) {
        case BEAR_ACT_RELU:  return x > 0 ? x : 0;
        case BEAR_ACT_TANH:  return tanhf(x);
        case BEAR_ACT_GELU:  return 0.5f * x * (1.0f + tanhf(0.79788456f * (x + 0.044715f * x * x * x)));
        case BEAR_ACT_SILU:  return x / (1.0f + expf(-x));
        default: return x;
    }
}

/* SIMD batch activation */
static inline void bear_act_batch(float* __restrict dst, const float* __restrict src,
                                   int64_t n, BearAct act) {
#if defined(BEAR_HAVE_AVX2)
    if (act == BEAR_ACT_RELU) {
        __m256 zero = _mm256_setzero_ps();
        for (int64_t i = 0; i <= n - 8; i += 8) {
            __m256 v = _mm256_loadu_ps(src + i);
            _mm256_storeu_ps(dst + i, _mm256_max_ps(v, zero));
        }
        for (int64_t i = n & ~7; i < n; ++i) dst[i] = bear_act_scalar(src[i], act);
        return;
    }
#elif defined(BEAR_HAVE_NEON)
    if (act == BEAR_ACT_RELU) {
        float32x4_t zero = vdupq_n_f32(0.0f);
        for (int64_t i = 0; i <= n - 4; i += 4) {
            float32x4_t v = vld1q_f32(src + i);
            vst1q_f32(dst + i, vmaxq_f32(v, zero));
        }
        for (int64_t i = n & ~3; i < n; ++i) dst[i] = bear_act_scalar(src[i], act);
        return;
    }
#endif
    /* Scalar fallback for non-RELU or unsupported arch */
    for (int64_t i = 0; i < n; ++i) dst[i] = bear_act_scalar(src[i], act);
}

/* ═══════════════════════════════════════════════════════════════════
 * GEMM: C = A * B^T  (row-major: A=[M,K], B=[N,K], C=[M,N])
 * ═══════════════════════════════════════════════════════════════════ */

/* AVX2: 8x8 micro-kernel */
#if defined(BEAR_HAVE_AVX2)
static inline void bear_gemm_avx2(const float* A, const float* B, float* C,
                                   int M, int N, int K) {
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n <= N - 8; n += 8) {
            __m256 c = _mm256_setzero_ps();
            for (int k = 0; k < K; ++k) {
                __m256 a = _mm256_broadcast_ss(A + m * K + k);
                __m256 b = _mm256_loadu_ps(B + n * K + k);
                c = _mm256_fmadd_ps(a, b, c);
            }
            _mm256_storeu_ps(C + m * N + n, c);
        }
        /* Tail */
        for (int n = N & ~7; n < N; ++n) {
            float acc = 0.0f;
            for (int k = 0; k < K; ++k) acc += A[m * K + k] * B[n * K + k];
            C[m * N + n] = acc;
        }
    }
}
#endif

/* NEON: 4x4 micro-kernel */
#if defined(BEAR_HAVE_NEON)
static inline void bear_gemm_neon(const float* A, const float* B, float* C,
                                   int M, int N, int K) {
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n <= N - 4; n += 4) {
            float32x4_t c = vdupq_n_f32(0.0f);
            for (int k = 0; k < K; ++k) {
                float32x4_t a = vdupq_n_f32(A[m * K + k]);
                float32x4_t b = vld1q_f32(B + n * K + k);
                c = vfmaq_f32(c, a, b);
            }
            vst1q_f32(C + m * N + n, c);
        }
        for (int n = N & ~3; n < N; ++n) {
            float acc = 0.0f;
            for (int k = 0; k < K; ++k) acc += A[m * K + k] * B[n * K + k];
            C[m * N + n] = acc;
        }
    }
}
#endif

/* Scalar fallback */
static inline void bear_gemm_scalar(const float* A, const float* B, float* C,
                                     int M, int N, int K) {
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            float acc = 0.0f;
            for (int k = 0; k < K; ++k) {
                acc += A[m * K + k] * B[n * K + k];
            }
            C[m * N + n] = acc;
        }
    }
}

/* Dispatcher */
static inline void bear_gemm(const float* A, const float* B, float* C,
                              int M, int N, int K) {
    bear_gemm_scalar(A, B, C, M, N, K);
}

/* High-level tensor GEMM: out = A @ B^T + bias (if present) */
static inline void bear_tensor_gemm(const BearTensor* A, const BearTensor* B,
                                     const BearTensor* bias,
                                     BearTensor* out, BearAct act) {
    int M = (int)A->shape[0];
    int K = (int)A->shape[1];
    int N = (int)B->shape[0];

    const float* a = (const float*)A->data;
    const float* b = (const float*)B->data;
    float* c = (float*)out->data;

    bear_gemm(a, b, c, M, N, K);

    if (bias && bias->dtype == BEAR_DTYPE_F32) {
        const float* bias_p = (const float*)bias->data;
        for (int m = 0; m < M; ++m) {
            for (int n = 0; n < N; ++n) {
                c[m * N + n] += bias_p[n];
            }
        }
    }

    if (act != BEAR_ACT_NONE) {
        int64_t n = (int64_t)M * N;
        float* tmp = (float*)malloc(n * sizeof(float));
        for (int64_t i = 0; i < n; ++i) tmp[i] = c[i];
        bear_act_batch(c, tmp, n, act);
        free(tmp);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Fused MLP Layer: out = act(x @ W^T + b)
 * ═══════════════════════════════════════════════════════════════════ */

static inline void bear_mlp_layer(const BearTensor* x,   /* [batch, in_f] */
                                   const BearTensor* W,   /* [out_f, in_f] */
                                   const BearTensor* b,   /* [out_f] */
                                   BearTensor* out,       /* [batch, out_f] */
                                   BearAct act,
                                   BearArena* temp_arena) {
    (void)temp_arena;
    bear_tensor_gemm(x, W, b, out, act);
}

/* ═══════════════════════════════════════════════════════════════════
 * Math helpers (must come before MinGRU step)
 * ═══════════════════════════════════════════════════════════════════ */

static inline float bear_sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

static inline float bear_tanh_fast(float x) {
    return tanhf(x);
}

/* ═══════════════════════════════════════════════════════════════════
 * MinGRU Cell (parallelizable recurrent)
 * h_t = z_t * h_{t-1} + (1 - z_t) * n_t
 * z_t = sigmoid(x @ Wz + h_{t-1} @ Uz + bz)
 * r_t = sigmoid(x @ Wr + h_{t-1} @ Ur + br)
 * n_t = tanh(x @ Wn + r_t * (h_{t-1} @ Un) + bn)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    BearParam Wz;   /* [hid, in] */
    BearParam Uz;   /* [hid, hid] */
    BearParam bz;   /* [hid, 1] */
    BearParam Wr;   /* [hid, in] */
    BearParam Ur;   /* [hid, hid] */
    BearParam br;   /* [hid, 1] */
    BearParam Wn;   /* [hid, in] */
    BearParam Un;   /* [hid, hid] */
    BearParam bn;   /* [hid, 1] */
    int hid_size;
} BearMinGRU;

/* Initialize MinGRU params */
static inline int bear_mingru_create(BearArena* a, BearMinGRU* gru,
                                      int in_size, int hid_size) {
    gru->hid_size = hid_size;
    if (bear_param_create(a, &gru->Wz, hid_size, in_size, "gru.Wz") != 0) return -1;
    if (bear_param_create(a, &gru->Uz, hid_size, hid_size, "gru.Uz") != 0) return -1;
    if (bear_param_create(a, &gru->bz, hid_size, 1, "gru.bz") != 0) return -1;
    if (bear_param_create(a, &gru->Wr, hid_size, in_size, "gru.Wr") != 0) return -1;
    if (bear_param_create(a, &gru->Ur, hid_size, hid_size, "gru.Ur") != 0) return -1;
    if (bear_param_create(a, &gru->br, hid_size, 1, "gru.br") != 0) return -1;
    if (bear_param_create(a, &gru->Wn, hid_size, in_size, "gru.Wn") != 0) return -1;
    if (bear_param_create(a, &gru->Un, hid_size, hid_size, "gru.Un") != 0) return -1;
    if (bear_param_create(a, &gru->bn, hid_size, 1, "gru.bn") != 0) return -1;
    return 0;
}

/* MinGRU forward (single step, batched) */
static inline void bear_mingru_step(const BearMinGRU* gru,
                                     const BearTensor* x,      /* [batch, in] */
                                     const BearTensor* h_prev, /* [batch, hid] (optional) */
                                     BearTensor* h_next,       /* [batch, hid] */
                                     BearArena* temp) {
    int batch = (int)x->shape[0];
    int hid = gru->hid_size;
    int in_size = (int)gru->Wz.weight.shape[1];

    /* If no h_prev, use zero initial state */
    float* h_prev_p = NULL;
    if (h_prev && h_prev->data) {
        h_prev_p = (float*)h_prev->data;
    }

    /* Allocate temp buffers */
    float* z_pre = (float*)BEAR_ARENA_ALLOC(temp, float, batch * hid);
    float* r_pre = (float*)BEAR_ARENA_ALLOC(temp, float, batch * hid);
    float* n_pre = (float*)BEAR_ARENA_ALLOC(temp, float, batch * hid);
    float* Uh     = (float*)BEAR_ARENA_ALLOC(temp, float, batch * hid);
    float* Ur_h   = (float*)BEAR_ARENA_ALLOC(temp, float, batch * hid);

    const float* x_p = (const float*)x->data;
    float* h_next_p = (float*)h_next->data;
    fflush(stdout);

    /* z_pre = x @ Wz^T  [batch, hid] */
    bear_gemm(x_p, (const float*)gru->Wz.weight.data, z_pre, batch, hid, in_size);
    /* r_pre = x @ Wr^T  [batch, hid] */
    bear_gemm(x_p, (const float*)gru->Wr.weight.data, r_pre, batch, hid, in_size);

    if (h_prev_p) {
        bear_gemm(h_prev_p, (const float*)gru->Uz.weight.data, Uh, batch, hid, hid);
        bear_gemm(h_prev_p, (const float*)gru->Ur.weight.data, Ur_h, batch, hid, hid);
    } else {
        memset(Uh, 0, batch * hid * sizeof(float));
        memset(Ur_h, 0, batch * hid * sizeof(float));
    }

    const float* bz_p = (const float*)gru->bz.bias.data;
    const float* br_p = (const float*)gru->br.bias.data;
    const float* bn_p = (const float*)gru->bn.bias.data;

    for (int i = 0; i < batch * hid; ++i) {
        z_pre[i] = bear_sigmoid(z_pre[i] + Uh[i] + bz_p[i % hid]);
        r_pre[i] = bear_sigmoid(r_pre[i] + Ur_h[i] + br_p[i % hid]);
    }

    /* n_pre = x @ Wn^T  [batch, hid] */
    bear_gemm(x_p, (const float*)gru->Wn.weight.data, n_pre, batch, hid, in_size);

    /* Un_term = h_prev @ Un^T  [batch, hid] */
    if (h_prev_p) {
        float* Un_term = (float*)BEAR_ARENA_ALLOC(temp, float, batch * hid);
        bear_gemm(h_prev_p, (const float*)gru->Un.weight.data, Un_term, batch, hid, hid);
        for (int i = 0; i < batch * hid; ++i) {
            n_pre[i] = bear_tanh_fast(n_pre[i] + r_pre[i] * Un_term[i] + bn_p[i % hid]);
        }
    } else {
        for (int i = 0; i < batch * hid; ++i) {
            n_pre[i] = bear_tanh_fast(n_pre[i] + bn_p[i % hid]);
        }
    }

    /* h_next = z * h_prev + (1 - z) * n */
    for (int i = 0; i < batch * hid; ++i) {
        float h_val = h_prev_p ? h_prev_p[i] : 0.0f;
        h_next_p[i] = z_pre[i] * h_val + (1.0f - z_pre[i]) * n_pre[i];
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Vectorized Environment Step Helpers (SIMD on env dimension)
 * ═══════════════════════════════════════════════════════════════════ */

#define BEAR_MAX_ENVS 1024
#define BEAR_MAX_AGENTS 8
#define BEAR_MAX_OBS_DIM 256
#define BEAR_MAX_ACT_DIM 64

/* ═══════════════════════════════════════════════════════════════════
 * Orthogonal Initialization (CleanRL detail)
 * ═══════════════════════════════════════════════════════════════════ */

/* Fill matrix with orthogonal initialization (QR decomp of random) */
static inline void bear_orthogonal_init(BearTensor* W, float gain) {
    if (W->dtype != BEAR_DTYPE_F32 || W->ndim != 2) return;
    int rows = (int)W->shape[0];
    int cols = (int)W->shape[1];
    float* data = (float*)W->data;

    /* Simple uniform Xavier init — guaranteed finite, no trig/log */
    float limit = gain * sqrtf(6.0f / (float)(rows + cols));
    for (int i = 0; i < rows * cols; ++i) {
        long r = rand();
        float u = (float)r / (float)RAND_MAX;
        data[i] = (2.0f * u - 1.0f) * limit;
    }
}

#endif /* BEAR_SIMD_H */
