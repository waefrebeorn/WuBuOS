/*
 * bear_cudnn.h  --  cuBLAS/cuDNN Wrappers for HolyC FFI
 * 
 * HolyC-callable cuBLAS/cuDNN operations via extern "C"
 * 
 * HolyC usage:
 *   extern "C" void hc_builtin_cublas_sgemm(
 *       cublasHandle_t handle, cublasOperation_t transa, cublasOperation_t transb,
 *       int m, int n, int k,
 *       const float* alpha,
 *       const float* A, int lda,
 *       const float* B, int ldb,
 *       const float* beta,
 *       float* C, int ldc
 *   );
 */

#ifndef BEAR_CUDNN_H
#define BEAR_CUDNN_H

/* Feature detection */
#if defined(__CUDA_RUNTIME_H__) || defined(__CUDACC__)
#define BEAR_HAVE_CUDA 1
#else
#define BEAR_HAVE_CUDA 0
#endif

#if defined(__CUDNN_H__) || defined(CUDNN_VERSION)
#define BEAR_HAVE_CUDNN 1
#else
#define BEAR_HAVE_CUDNN 0
#endif

#if BEAR_HAVE_CUDA
#include <cublas_v2.h>
#endif

#if BEAR_HAVE_CUDNN
#include <cudnn.h>
#endif

#include <stddef.h>  /* for size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * cuBLAS Context Management
 * =================================================================== */

typedef struct BearCublasHandle* BearCublasHandle;

BearCublasHandle hc_builtin_cublas_create(void);
void hc_builtin_cublas_destroy(BearCublasHandle handle);

/* ===================================================================
 * cuBLAS Level-1/2/3 Operations (SGEMM, SAXPY, SDOT, etc.)
 * =================================================================== */

/* SGEMM: C = alpha * op(A) * op(B) + beta * C
 * op(A) is m x k, op(B) is k x n, C is m x n
 * lda: leading dimension of A, etc.
 */
void hc_builtin_cublas_sgemm(
    BearCublasHandle handle,
    int transa, int transb,    // cublasOperation_t: 0=no_trans, 1=trans
    int m, int n, int k,
    const float* alpha,
    const float* A, int lda,
    const float* B, int ldb,
    const float* beta,
    float* C, int ldc);

/* SAXPY: y = alpha * x + y */
void hc_builtin_cublas_saxpy(
    BearCublasHandle handle,
    int n,
    const float* alpha,
    const float* x, int incx,
    float* y, int incy);

/* SDOT: dot product */
float hc_builtin_cublas_sdot(
    BearCublasHandle handle,
    int n,
    const float* x, int incx,
    const float* y, int incy);

/* SNRM2: Euclidean norm */
float hc_builtin_cublas_snrm2(
    BearCublasHandle handle,
    int n,
    const float* x, int incx);

/* SSCAL: x = alpha * x */
void hc_builtin_cublas_sscal(
    BearCublasHandle handle,
    int n,
    const float* alpha,
    float* x, int incx);

/* ===================================================================
 * cuDNN Context Management
 * =================================================================== */

typedef struct BearCudnnHandle* BearCudnnHandle;

BearCudnnHandle hc_builtin_cudnn_create(void);
void hc_builtin_cudnn_destroy(BearCudnnHandle handle);

/* ===================================================================
 * cuDNN Tensor Descriptors & Operations
 * =================================================================== */

typedef struct BearCudnnTensorDesc* BearCudnnTensorDesc;

BearCudnnTensorDesc hc_builtin_cudnn_create_tensor_desc(
    int n, int c, int h, int w);  // NCHW format
void hc_builtin_cudnn_destroy_tensor_desc(BearCudnnTensorDesc desc);

/* Convolution Forward */
void hc_builtin_cudnn_convolution_forward(
    BearCudnnHandle handle,
    const float* alpha,
    BearCudnnTensorDesc x_desc, const float* x,
    BearCudnnTensorDesc w_desc, const float* w,
    const int conv_desc[],  // padding_h, padding_w, stride_h, stride_w, dilation_h, dilation_w
    const float* beta,
    BearCudnnTensorDesc y_desc, float* y,
    int algo,  // cudnnConvolutionFwdAlgo_t
    void* workspace, size_t workspace_size);

/* Convolution Backward Data */
void hc_builtin_cudnn_convolution_backward_data(
    BearCudnnHandle handle,
    const float* alpha,
    BearCudnnTensorDesc w_desc, const float* w,
    BearCudnnTensorDesc dy_desc, const float* dy,
    const int conv_desc[],
    const float* beta,
    BearCudnnTensorDesc dx_desc, float* dx,
    int algo, void* workspace, size_t workspace_size);

/* Convolution Backward Filter */
void hc_builtin_cudnn_convolution_backward_filter(
    BearCudnnHandle handle,
    const float* alpha,
    BearCudnnTensorDesc x_desc, const float* x,
    BearCudnnTensorDesc dy_desc, const float* dy,
    const int conv_desc[],
    const float* beta,
    BearCudnnTensorDesc dw_desc, float* dw,
    int algo, void* workspace, size_t workspace_size);

/* Activation Forward */
void hc_builtin_cudnn_activation_forward(
    BearCudnnHandle handle,
    int mode,  // cudnnActivationMode_t: 0=sigmoid, 1=relu, 2=tanh
    const float* alpha,
    BearCudnnTensorDesc x_desc, const float* x,
    const float* beta,
    BearCudnnTensorDesc y_desc, float* y);

/* Pooling Forward */
void hc_builtin_cudnn_pooling_forward(
    BearCudnnHandle handle,
    int mode,  // cudnnPoolingMode_t: 0=max, 1=average
    BearCudnnTensorDesc x_desc, const float* x,
    const int pool_desc[],  // window_h, window_w, pad_h, pad_w, stride_h, stride_w
    const float* beta,
    BearCudnnTensorDesc y_desc, float* y);

/* Softmax Forward */
void hc_builtin_cudnn_softmax_forward(
    BearCudnnHandle handle,
    int algo, int mode,  // cudnnSoftmaxAlgorithm_t, cudnnSoftmaxMode_t
    const float* alpha,
    BearCudnnTensorDesc x_desc, const float* x,
    const float* beta,
    BearCudnnTensorDesc y_desc, float* y);

/* Get Workspace Size for Convolution */
#if BEAR_HAVE_CUDNN
size_t hc_builtin_cudnn_get_convolution_forward_workspace_size(
    BearCudnnHandle handle,
    BearCudnnTensorDesc x_desc,
    BearCudnnTensorDesc w_desc,
    int conv_desc[],
    BearCudnnTensorDesc y_desc,
    int algo);

size_t hc_builtin_cudnn_get_convolution_backward_data_workspace_size(
    BearCudnnHandle handle,
    BearCudnnTensorDesc w_desc,
    BearCudnnTensorDesc dy_desc,
    int conv_desc[],
    BearCudnnTensorDesc dx_desc,
    int algo);

size_t hc_builtin_cudnn_get_convolution_backward_filter_workspace_size(
    BearCudnnHandle handle,
    BearCudnnTensorDesc x_desc,
    BearCudnnTensorDesc dy_desc,
    int conv_desc[],
    BearCudnnTensorDesc dw_desc,
    int algo);
#endif

/* ===================================================================
 * Memory Management (Unified with CUDA arena)
 * =================================================================== */

void* hc_builtin_cuda_malloc(size_t bytes);
void hc_builtin_cuda_free(void* ptr);

/* ===================================================================
 * Error Handling
 * =================================================================== */

const char* hc_builtin_cublas_get_error_string(int status);
const char* hc_builtin_cudnn_get_error_string(int status);

#ifdef __cplusplus
}
#endif

#endif /* BEAR_CUDNN_H */
