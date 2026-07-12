/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "bear_cudnn.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void hc_builtin_cublas_destroy(BearCublasHandle handle) {
    if (!handle) return;
#if BEAR_HAVE_CUDA
    cublasDestroy(handle->handle);
#endif
    free(handle);
}

/* ===================================================================
 * cuBLAS Operations
 * =================================================================== */

void hc_builtin_cublas_sgemm(
    BearCublasHandle handle,
    int transa, int transb,
    int m, int n, int k,
    const float* alpha,
    const float* A, int lda,
    const float* B, int ldb,
    const float* beta,
    float* C, int ldc)
{
    if (!handle) return;
#if BEAR_HAVE_CUDA
    cublasOperation_t opa = (cublasOperation_t)transa;
    cublasOperation_t opb = (cublasOperation_t)transb;

    cublasSgemm(handle->handle, opa, opb, m, n, k,
                alpha, A, lda, B, ldb, beta, C, ldc);
#else
    /* CPU fallback: C = alpha * op(A) * op(B) + beta * C
     * A is m x k (or k x m if transa), B is k x n (or n x k if transb), C is m x n
     * Row-major: lda = k if !transa else m, ldb = n if !transb else k, ldc = n
     * We support transa=0 (CUBLAS_OP_N) and transb=0 (CUBLAS_OP_N) for now.
     * Note: cuBLAS is column-major, but our data is row-major.
     * For row-major: C = alpha * A * B + beta * C means C^T = alpha * B^T * A^T + beta * C^T
     * So we can call row-major GEMM with swapped A/B and transposed operations.
     * For transa=0, transb=0 (no transpose), A=[m,k], B=[k,n], C=[m,n]
     * Row-major: C[i][j] = sum_k A[i][k] * B[k][j]
     * Our bear_gemm expects A=[M,K], B=[N,K] (B is transposed), C=[M,N]
     * So for C = A * B with A=[m,k], B=[k,n], we need B_transposed = B^T = [n,k]
     * Then C = A * B_transposed^T = A * B
     */
    if (transa == 0 && transb == 0) {
        /* C = alpha * A * B + beta * C, A=[m,k], B=[k,n], C=[m,n] */
        float a_val = alpha ? *alpha : 1.0f;
        float b_val = beta ? *beta : 0.0f;
        
        if (b_val != 0.0f) {
            /* C = beta * C first */
            for (int i = 0; i < m * n; ++i) {
                C[i] *= b_val;
            }
        } else {
            /* C = 0 */
            memset(C, 0, m * n * sizeof(float));
        }
        
        /* Allocate transposed B: [n, k] */
        float* B_T = (float*)malloc(n * k * sizeof(float));
        if (B_T) {
            for (int j = 0; j < n; ++j) {
                for (int i = 0; i < k; ++i) {
                    B_T[j * k + i] = B[i * ldb + j];
                }
            }
            
            /* bear_gemm expects A=[M,K], B=[N,K], C=[M,N] -> C = A * B^T
             * Here A=[m,k], B_T=[n,k], so C = A * B_T^T = A * B */
            bear_gemm(A, B_T, C, m, n, k);
            
            /* Scale by alpha */
            if (a_val != 1.0f) {
                for (int i = 0; i < m * n; ++i) {
                    C[i] *= a_val;
                }
            }
            
            free(B_T);
        }
    } else {
        /* For other transpose modes, fall back to simple triple loop */
        float a_val = alpha ? *alpha : 1.0f;
        float b_val = beta ? *beta : 0.0f;
        
        if (b_val != 0.0f) {
            for (int i = 0; i < m * n; ++i) C[i] *= b_val;
        } else {
            memset(C, 0, m * n * sizeof(float));
        }
        
        for (int i = 0; i < m; ++i) {
            for (int j = 0; j < n; ++j) {
                float sum = 0.0f;
                for (int l = 0; l < k; ++l) {
                    float a_val_local, b_val_local;
                    if (transa == 0) a_val_local = A[i * lda + l];
                    else a_val_local = A[l * lda + i];
                    if (transb == 0) b_val_local = B[l * ldb + j];
                    else b_val_local = B[j * ldb + l];
                    sum += a_val_local * b_val_local;
                }
                C[i * ldc + j] += a_val * sum;
            }
        }
    }
#endif
}

void hc_builtin_cublas_saxpy(
    BearCublasHandle handle,
    int n,
    const float* alpha,
    const float* x, int incx,
    float* y, int incy)
{
    if (!handle) return;
#if BEAR_HAVE_CUDA
    cublasSaxpy(handle->handle, n, alpha, x, incx, y, incy);
#else
    /* CPU fallback: y = alpha * x + y */
    float a_val = alpha ? *alpha : 1.0f;
    if (incx == 1 && incy == 1) {
        for (int i = 0; i < n; ++i) {
            y[i] += a_val * x[i];
        }
    } else {
        for (int i = 0, ix = 0, iy = 0; i < n; ++i, ix += incx, iy += incy) {
            y[iy] += a_val * x[ix];
        }
    }
#endif
}

float hc_builtin_cublas_sdot(
    BearCublasHandle handle,
    int n,
    const float* x, int incx,
    const float* y, int incy)
{
    if (!handle) return 0.0f;
#if BEAR_HAVE_CUDA
    float result = 0.0f;
    cublasSdot(handle->handle, n, x, incx, y, incy, &result);
    return result;
#else
    /* CPU fallback: dot product */
    float result = 0.0f;
    if (incx == 1 && incy == 1) {
        for (int i = 0; i < n; ++i) {
            result += x[i] * y[i];
        }
    } else {
        for (int i = 0, ix = 0, iy = 0; i < n; ++i, ix += incx, iy += incy) {
            result += x[ix] * y[iy];
        }
    }
    return result;
#endif
}

float hc_builtin_cublas_snrm2(
    BearCublasHandle handle,
    int n,
    const float* x, int incx)
{
    if (!handle) return 0.0f;
#if BEAR_HAVE_CUDA
    float result = 0.0f;
    cublasSnrm2(handle->handle, n, x, incx, &result);
    return result;
#else
    /* CPU fallback: Euclidean norm */
    float sum = 0.0f;
    if (incx == 1) {
        for (int i = 0; i < n; ++i) {
            sum += x[i] * x[i];
        }
    } else {
        for (int i = 0, ix = 0; i < n; ++i, ix += incx) {
            sum += x[ix] * x[ix];
        }
    }
    return sqrtf(sum);
#endif
}

void hc_builtin_cublas_sscal(
    BearCublasHandle handle,
    int n,
    const float* alpha,
    float* x, int incx)
{
    if (!handle) return;
#if BEAR_HAVE_CUDA
    cublasSscal(handle->handle, n, alpha, x, incx);
#else
    /* CPU fallback: x = alpha * x */
    float a_val = alpha ? *alpha : 1.0f;
    if (incx == 1) {
        for (int i = 0; i < n; ++i) {
            x[i] *= a_val;
        }
    } else {
        for (int i = 0, ix = 0; i < n; ++i, ix += incx) {
            x[ix] *= a_val;
        }
    }
#endif
}
