/*
 * bear_cudnn.c  --  cuBLAS/cuDNN Wrapper Implementations
 * 
 * HolyC-callable cuBLAS/cuDNN operations
 * Compiled with: nvcc -c bear_cudnn.c -o bear_cudnn.o -lcublas -lcudnn
 * 
 * If CUDA/cuDNN not available, provides stub implementations.
 */

#include "bear_cudnn.h"

#include <stdlib.h>
#include <string.h>

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
#include <cuda_runtime.h>
#include <cublas_v2.h>
#endif

#if BEAR_HAVE_CUDNN
#include <cudnn.h>
#endif

/* ===================================================================
 * cuBLAS Handle
 * =================================================================== */

struct BearCublasHandle {
#if BEAR_HAVE_CUDA
    cublasHandle_t handle;
#else
    void* handle;  /* stub */
#endif
};

BearCublasHandle hc_builtin_cublas_create(void) {
#if BEAR_HAVE_CUDA
    BearCublasHandle h = malloc(sizeof(struct BearCublasHandle));
    if (!h) return NULL;
    
    cublasStatus_t status = cublasCreate(&h->handle);
    if (status != CUBLAS_STATUS_SUCCESS) {
        free(h);
        return NULL;
    }
    
    /* Set math mode to use tensor cores for >= 16x16x16 */
    cublasSetMathMode(h->handle, CUBLAS_TENSOR_OP_MATH);
    
    return h;
#else
    /* Stub implementation */
    return malloc(sizeof(struct BearCublasHandle));
#endif
}

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
    (void)transa; (void)transb; (void)m; (void)n; (void)k;
    (void)alpha; (void)A; (void)lda; (void)B; (void)ldb;
    (void)beta; (void)C; (void)ldc;
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
    (void)n; (void)alpha; (void)x; (void)incx; (void)y; (void)incy;
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
    (void)n; (void)x; (void)incx; (void)y; (void)incy;
    return 0.0f;
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
    (void)n; (void)x; (void)incx;
    return 0.0f;
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
    (void)n; (void)alpha; (void)x; (void)incx;
#endif
}

/* ===================================================================
 * cuDNN Handle
 * =================================================================== */

struct BearCudnnHandle {
#if BEAR_HAVE_CUDNN
    cudnnHandle_t handle;
#else
    void* handle;  /* stub */
#endif
};

BearCudnnHandle hc_builtin_cudnn_create(void) {
#if BEAR_HAVE_CUDNN
    BearCudnnHandle h = malloc(sizeof(struct BearCudnnHandle));
    if (!h) return NULL;
    
    cudnnStatus_t status = cudnnCreate(&h->handle);
    if (status != CUDNN_STATUS_SUCCESS) {
        free(h);
        return NULL;
    }
    return h;
#else
    return malloc(sizeof(struct BearCudnnHandle));
#endif
}

void hc_builtin_cudnn_destroy(BearCudnnHandle handle) {
    if (!handle) return;
#if BEAR_HAVE_CUDNN
    cudnnDestroy(handle->handle);
#endif
    free(handle);
}

/* ===================================================================
 * cuDNN Tensor Descriptors
 * =================================================================== */

struct BearCudnnTensorDesc {
#if BEAR_HAVE_CUDNN
    cudnnTensorDescriptor_t desc;
#else
    void* desc;
#endif
};

BearCudnnTensorDesc hc_builtin_cudnn_create_tensor_desc(int n, int c, int h, int w) {
#if BEAR_HAVE_CUDNN
    BearCudnnTensorDesc d = malloc(sizeof(struct BearCudnnTensorDesc));
    if (!d) return NULL;
    
    cudnnStatus_t status = cudnnCreateTensorDescriptor(&d->desc);
    if (status != CUDNN_STATUS_SUCCESS) {
        free(d);
        return NULL;
    }
    
    status = cudnnSetTensor4dDescriptor(d->desc, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, n, c, h, w);
    if (status != CUDNN_STATUS_SUCCESS) {
        cudnnDestroyTensorDescriptor(d->desc);
        free(d);
        return NULL;
    }
    
    return d;
#else
    (void)n; (void)c; (void)h; (void)w;
    return malloc(sizeof(struct BearCudnnTensorDesc));
#endif
}

void hc_builtin_cudnn_destroy_tensor_desc(BearCudnnTensorDesc desc) {
    if (!desc) return;
#if BEAR_HAVE_CUDNN
    cudnnDestroyTensorDescriptor(desc->desc);
#endif
    free(desc);
}

/* ===================================================================
 * cuDNN Convolution
 * =================================================================== */

void hc_builtin_cudnn_convolution_forward(
    BearCudnnHandle handle,
    const float* alpha,
    BearCudnnTensorDesc x_desc, const float* x,
    BearCudnnTensorDesc w_desc, const float* w,
    const int conv_desc[],  // pad_h, pad_w, stride_h, stride_w, dil_h, dil_w
    const float* beta,
    BearCudnnTensorDesc y_desc, float* y,
    int algo, void* workspace, size_t workspace_size)
{
    if (!handle || !x_desc || !w_desc || !y_desc) return;
#if BEAR_HAVE_CUDNN
    cudnnConvolutionDescriptor_t conv_desc_obj;
    cudnnCreateConvolutionDescriptor(&conv_desc_obj);
    
    int pad_h = conv_desc[0];
    int pad_w = conv_desc[1];
    int stride_h = conv_desc[2];
    int stride_w = conv_desc[3];
    int dil_h = conv_desc[4];
    int dil_w = conv_desc[5];
    
    cudnnSetConvolution2dDescriptor(
        conv_desc_obj, 
        conv_desc[0], conv_desc[1],  // pad_h, pad_w
        conv_desc[2], conv_desc[3],  // stride_h, stride_w
        conv_desc[4], conv_desc[5],  // dil_h, dil_w
        CUDNN_CROSS_CORRELATION, CUDNN_DATA_FLOAT);
    
    cudnnConvolutionForward(handle->handle, alpha, 
                            x_desc->desc, x,
                            w_desc->desc, w,
                            conv_desc_obj, (cudnnConvolutionFwdAlgo_t)algo,
                            workspace, workspace_size,
                            beta, y_desc->desc, y);
    
    cudnnDestroyConvolutionDescriptor(conv_desc_obj);
#else
    (void)alpha; (void)x_desc; (void)x; (void)w_desc; (void)w;
    (void)conv_desc; (void)beta; (void)y_desc; (void)y;
    (void)algo; (void)workspace; (void)workspace_size;
#endif
}

void hc_builtin_cudnn_convolution_backward_data(
    BearCudnnHandle handle,
    const float* alpha,
    BearCudnnTensorDesc w_desc, const float* w,
    BearCudnnTensorDesc dy_desc, const float* dy,
    const int conv_desc[],
    const float* beta,
    BearCudnnTensorDesc dx_desc, float* dx,
    int algo, void* workspace, size_t workspace_size)
{
    if (!handle) return;
#if BEAR_HAVE_CUDNN
    cudnnConvolutionDescriptor_t conv_desc_obj;
    cudnnCreateConvolutionDescriptor(&conv_desc_obj);
    
    cudnnSetConvolution2dDescriptor(
        conv_desc_obj, conv_desc[0], conv_desc[1], conv_desc[2], conv_desc[3],
        conv_desc[4], conv_desc[5], CUDNN_CROSS_CORRELATION, CUDNN_DATA_FLOAT);
    
    cudnnConvolutionBackwardData(handle->handle, alpha,
                                 w_desc->desc, w,
                                 c_desc->desc, dy,
                                 conv_desc_obj, (cudnnConvolutionBwdDataAlgo_t)algo,
                                 workspace, workspace_size,
                                 beta, dx_desc->desc, dx);
    
    cudnnDestroyConvolutionDescriptor(conv_desc_obj);
#else
    (void)alpha; (void)w_desc; (void)w; (void)dy_desc; (void)dy;
    (void)conv_desc; (void)beta; (void)dx_desc; (void)dx;
    (void)algo; (void)workspace; (void)workspace_size;
#endif
}

void hc_builtin_cudnn_convolution_backward_filter(
    BearCudnnHandle handle,
    const float* alpha,
    BearCudnnTensorDesc x_desc, const float* x,
    BearCudnnTensorDesc dy_desc, const float* dy,
    const int conv_desc[],
    const float* beta,
    BearCudnnTensorDesc dw_desc, float* dw,
    int algo, void* workspace, size_t workspace_size)
{
    if (!handle) return;
#if BEAR_HAVE_CUDNN
    cudnnConvolutionDescriptor_t conv_desc_obj;
    cudnnCreateConvolutionDescriptor(&conv_desc_obj);
    
    cudnnSetConvolution2dDescriptor(
        conv_desc_obj, conv_desc[0], conv_desc[1], conv_desc[2], conv_desc[3],
        conv_desc[4], conv_desc[5], CUDNN_CROSS_CORRELATION, CUDNN_DATA_FLOAT);
    
    cudnnConvolutionBackwardFilter(handle->handle, alpha,
                                   x_desc->desc, x, dy_desc->desc, dy,
                                   conv_desc_obj, (cudnnConvolutionBwdFilterAlgo_t)algo,
                                   workspace, workspace_size,
                                   beta, dw_desc->desc, dw);
    
    cudnnDestroyConvolutionDescriptor(conv_desc_obj);
#else
    (void)alpha; (void)x_desc; (void)x; (void)dy_desc; (void)dy;
    (void)conv_desc; (void)beta; (void)dw_desc; (void)dw;
    (void)algo; (void)workspace; (void)workspace_size;
#endif
}

/* ===================================================================
 * cuDNN Activation
 * =================================================================== */

void hc_builtin_cudnn_activation_forward(
    BearCudnnHandle handle,
    int mode,  // 0=sigmoid, 1=relu, 2=tanh
    const float* alpha,
    BearCudnnTensorDesc x_desc, const float* x,
    const float* beta,
    BearCudnnTensorDesc y_desc, float* y)
{
    if (!handle || !x_desc || !y_desc) return;
#if BEAR_HAVE_CUDNN
    cudnnActivationDescriptor_t act_desc;
    cudnnCreateActivationDescriptor(&act_desc);
    
    cudnnActivationMode_t mode_t;
    switch (mode) {
        case 0: mode_t = CUDNN_ACTIVATION_SIGMOID; break;
        case 1: mode_t = CUDNN_ACTIVATION_RELU; break;
        case 2: mode_t = CUDNN_ACTIVATION_TANH; break;
        default: mode_t = CUDNN_ACTIVATION_RELU; break;
    }
    
    cudnnSetActivationDescriptor(act_desc, mode_t, CUDNN_NOT_PROPAGATE_NAN, 0.0);
    
    cudnnActivationForward(handle->handle, act_desc,
                           alpha, x_desc->desc, x,
                           beta, y_desc->desc, y);
    
    cudnnDestroyActivationDescriptor(act_desc);
#else
    (void)mode; (void)alpha; (void)x_desc; (void)x; (void)beta; (void)y_desc; (void)y;
#endif
}

/* ===================================================================
 * cuDNN Pooling
 * =================================================================== */

void hc_builtin_cudnn_pooling_forward(
    BearCudnnHandle handle,
    int mode,  // 0=max, 1=average
    BearCudnnTensorDesc x_desc, const float* x,
    const int pool_desc[],  // window_h, window_w, pad_h, pad_w, stride_h, stride_w
    const float* beta,
    BearCudnnTensorDesc y_desc, float* y)
{
    if (!handle || !x_desc || !y_desc) return;
#if BEAR_HAVE_CUDNN
    cudnnPoolingDescriptor_t pool_desc_obj;
    cudnnCreatePoolingDescriptor(&pool_desc_obj);
    
    cudnnPoolingMode_t mode_t = (mode == 0) ? CUDNN_POOLING_MAX : CUDNN_POOLING_AVERAGE_COUNT_INCLUDE_PADDING;
    
    cudnnSetPooling2dDescriptor(pool_desc_obj, mode_t,
                                CUDNN_NOT_PROPAGATE_NAN,
                                pool_desc[0], pool_desc[1],  // window_h, window_w
                                pool_desc[2], pool_desc[3],  // pad_h, pad_w
                                pool_desc[4], pool_desc[5]); // stride_h, stride_w
    
    cudnnPoolingForward(handle->handle, pool_desc_obj,
                        alpha, x_desc->desc, x,
                        beta, y_desc->desc, y);
    
    cudnnDestroyPoolingDescriptor(pool_desc_obj);
#else
    (void)mode; (void)x_desc; (void)x; (void)pool_desc; (void)beta; (void)y_desc; (void)y;
#endif
}

/* ===================================================================
 * cuDNN Softmax
 * =================================================================== */

void hc_builtin_cudnn_softmax_forward(
    BearCudnnHandle handle,
    int algo, int mode,
    const float* alpha,
    BearCudnnTensorDesc x_desc, const float* x,
    const float* beta,
    BearCudnnTensorDesc y_desc, float* y)
{
    if (!handle || !x_desc || !y_desc) return;
#if BEAR_HAVE_CUDNN
    cudnnSoftmaxForward(handle->handle, (cudnnSoftmaxAlgorithm_t)algo,
                       (cudnnSoftmaxMode_t)mode,
                       alpha, x_desc->desc, x,
                       beta, y_desc->desc, y);
#else
    (void)algo; (void)mode; (void)alpha; (void)x_desc; (void)x; (void)beta; (void)y_desc; (void)y;
#endif
}

/* ===================================================================
 * cuDNN Workspace Size Queries
 * =================================================================== */

size_t hc_builtin_cudnn_get_convolution_forward_workspace_size(
    BearCudnnHandle handle,
    BearCudnnTensorDesc x_desc,
    BearCudnnTensorDesc w_desc,
    int conv_desc[],
    BearCudnnTensorDesc y_desc,
    int algo)
{
    if (!handle) return 0;
#if BEAR_HAVE_CUDNN
    cudnnConvolutionDescriptor_t conv_desc_obj;
    cudnnCreateConvolutionDescriptor(&conv_desc_obj);
    
    cudnnSetConvolution2dDescriptor(
        conv_desc_obj, conv_desc[0], conv_desc[1], conv_desc[2], conv_desc[3],
        conv_desc[4], conv_desc[5], CUDNN_CROSS_CORRELATION, CUDNN_DATA_FLOAT);
    
    size_t size = 0;
    cudnnGetConvolutionForwardWorkspaceSize(handle->handle,
                                            x_desc->desc, w_desc->desc,
                                            conv_desc_obj, y_desc->desc,
                                            (cudnnConvolutionFwdAlgo_t)algo,
                                            CUDNN_DATA_FLOAT, &size);
    
    cudnnDestroyConvolutionDescriptor(conv_desc_obj);
    return size;
#else
    (void)x_desc; (void)w_desc; (void)conv_desc; (void)y_desc; (void)algo;
    return 0;
#endif
}

size_t hc_builtin_cudnn_get_convolution_backward_data_workspace_size(
    BearCudnnHandle handle,
    BearCudnnTensorDesc w_desc,
    BearCudnnTensorDesc dy_desc,
    int conv_desc[],
    BearCudnnTensorDesc dx_desc,
    int algo)
{
    if (!handle) return 0;
#if BEAR_HAVE_CUDNN
    cudnnConvolutionDescriptor_t conv_desc_obj;
    cudnnCreateConvolutionDescriptor(&conv_desc_obj);
    
    cudnnSetConvolution2dDescriptor(
        conv_desc_obj, conv_desc[0], conv_desc[1], conv_desc[2], conv_desc[3],
        conv_desc[4], conv_desc[5], CUDNN_CROSS_CORRELATION, CUDNN_DATA_FLOAT);
    
    size_t size = 0;
    cudnnGetConvolutionBackwardDataWorkspaceSize(handle->handle,
                                                 w_desc->desc, dy_desc->desc,
                                                 conv_desc_obj, dx_desc->desc,
                                                 (cudnnConvolutionBwdDataAlgo_t)algo,
                                                 CUDNN_DATA_FLOAT, &size);
    
    cudnnDestroyConvolutionDescriptor(conv_desc_obj);
    return size;
#else
    (void)w_desc; (void)dy_desc; (void)conv_desc; (void)dx_desc; (void)algo;
    return 0;
#endif
}

size_t hc_builtin_cudnn_get_convolution_backward_filter_workspace_size(
    BearCudnnHandle handle,
    BearCudnnTensorDesc x_desc,
    BearCudnnTensorDesc dy_desc,
    int conv_desc[],
    BearCudnnTensorDesc dw_desc,
    int algo)
{
    if (!handle) return 0;
#if BEAR_HAVE_CUDNN
    cudnnConvolutionDescriptor_t conv_desc_obj;
    cudnnCreateConvolutionDescriptor(&conv_desc_obj);
    
    cudnnSetConvolution2dDescriptor(
        conv_desc_obj, conv_desc[0], conv_desc[1], conv_desc[2], conv_desc[3],
        conv_desc[4], conv_desc[5], CUDNN_CROSS_CORRELATION, CUDNN_DATA_FLOAT);
    
    size_t size = 0;
    cudnnGetConvolutionBackwardFilterWorkspaceSize(handle->handle,
                                                   x_desc->desc, dy_desc->desc,
                                                   conv_desc_obj, dw_desc->desc,
                                                   (cudnnConvolutionBwdFilterAlgo_t)algo,
                                                   CUDNN_DATA_FLOAT, &size);
    
    cudnnDestroyConvolutionDescriptor(conv_desc_obj);
    return size;
#else
    (void)x_desc; (void)dy_desc; (void)conv_desc; (void)dw_desc; (void)algo;
    return 0;
#endif
}

/* ===================================================================
 * CUDA Memory Management
 * =================================================================== */

void* hc_builtin_cuda_malloc(size_t bytes) {
#if BEAR_HAVE_CUDA
    void* ptr = NULL;
    cudaMalloc(&ptr, bytes);
    return ptr;
#else
    (void)bytes;
    return NULL;
#endif
}

void hc_builtin_cuda_free(void* ptr) {
#if BEAR_HAVE_CUDA
    if (ptr) cudaFree(ptr);
#else
    (void)ptr;
#endif
}

/* ===================================================================
 * Error Strings
 * =================================================================== */

const char* hc_builtin_cublas_get_error_string(int status) {
#if BEAR_HAVE_CUDA
    switch (status) {
        case CUBLAS_STATUS_SUCCESS: return "CUBLAS_STATUS_SUCCESS";
        case CUBLAS_STATUS_NOT_INITIALIZED: return "CUBLAS_STATUS_NOT_INITIALIZED";
        case CUBLAS_STATUS_ALLOC_FAILED: return "CUBLAS_STATUS_ALLOC_FAILED";
        case CUBLAS_STATUS_INVALID_VALUE: return "CUBLAS_STATUS_INVALID_VALUE";
        case CUBLAS_STATUS_ARCH_MISMATCH: return "CUBLAS_STATUS_ARCH_MISMATCH";
        case CUBLAS_STATUS_MAPPING_ERROR: return "CUBLAS_STATUS_MAPPING_ERROR";
        case CUBLAS_STATUS_EXECUTION_FAILED: return "CUBLAS_STATUS_EXECUTION_FAILED";
        case CUBLAS_STATUS_INTERNAL_ERROR: return "CUBLAS_STATUS_INTERNAL_ERROR";
        case CUBLAS_STATUS_NOT_SUPPORTED: return "CUBLAS_STATUS_NOT_SUPPORTED";
        case CUBLAS_STATUS_LICENSE_ERROR: return "CUBLAS_STATUS_LICENSE_ERROR";
        default: return "CUBLAS_STATUS_UNKNOWN";
    }
#else
    (void)status;
    return "CUBLAS_STATUS_NO_CUDA";
#endif
}

const char* hc_builtin_cudnn_get_error_string(int status) {
#if BEAR_HAVE_CUDNN
    switch (status) {
        case CUDNN_STATUS_SUCCESS: return "CUDNN_STATUS_SUCCESS";
        case CUDNN_STATUS_NOT_INITIALIZED: return "CUDNN_STATUS_NOT_INITIALIZED";
        case CUDNN_STATUS_ALLOC_FAILED: return "CUDNN_STATUS_ALLOC_FAILED";
        case CUDNN_STATUS_BAD_PARAM: return "CUDNN_STATUS_BAD_PARAM";
        case CUDNN_STATUS_INTERNAL_ERROR: return "CUDNN_STATUS_INTERNAL_ERROR";
        case CUDNN_STATUS_INVALID_VALUE: return "CUDNN_STATUS_INVALID_VALUE";
        case CUDNN_STATUS_ARCH_MISMATCH: return "CUDNN_STATUS_ARCH_MISMATCH";
        case CUDNN_STATUS_MAPPING_ERROR: return "CUDNN_STATUS_MAPPING_ERROR";
        case CUDNN_STATUS_EXECUTION_FAILED: return "CUDNN_STATUS_EXECUTION_FAILED";
        case CUDNN_STATUS_NOT_SUPPORTED: return "CUDNN_STATUS_NOT_SUPPORTED";
        case CUDNN_STATUS_LICENSE_ERROR: return "CUDNN_STATUS_LICENSE_ERROR";
        case CUDNN_STATUS_RUNTIME_PREREQUISITE_MISSING: return "CUDNN_STATUS_RUNTIME_PREREQUISITE_MISSING";
        default: return "CUDNN_STATUS_UNKNOWN";
    }
#else
    (void)status;
    return "CUDNN_STATUS_NO_CUDA";
#endif
}

