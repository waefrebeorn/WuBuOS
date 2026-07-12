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
#include <stdint.h>
#include <math.h>

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
    /* CPU fallback: store tensor dimensions for later use */
    BearCudnnTensorDesc d = malloc(sizeof(struct BearCudnnTensorDesc));
    if (!d) return NULL;
    
    /* Store shape info in the descriptor pointer itself (as integer packed) */
    /* We'll use the desc pointer to store shape info: 
     * bits 0-15: w, bits 16-31: h, bits 32-47: c, bits 48-63: n */
    uint64_t *shape_ptr = (uint64_t*)&d->desc;
    *shape_ptr = ((uint64_t)n << 48) | ((uint64_t)c << 32) | ((uint64_t)h << 16) | (uint64_t)w;
    
    return d;
#endif
}

/* ===================================================================
 * Helper functions for CPU fallback tensor descriptors
 * =================================================================== */

static inline void bear_cudnn_get_tensor_shape(BearCudnnTensorDesc desc,
                                                int* n, int* c, int* h, int* w) {
#if BEAR_HAVE_CUDNN
    if (n) *n = 0; if (c) *c = 0; if (h) *h = 0; if (w) *w = 0;
#else
    uint64_t shape = *(uint64_t*)&desc->desc;
    if (n) *n = (int)(shape >> 48);
    if (c) *c = (int)((shape >> 32) & 0xFFFF);
    if (h) *h = (int)((shape >> 16) & 0xFFFF);
    if (w) *w = (int)(shape & 0xFFFF);
#endif
}

/* =================================================================== */

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
    /* CPU fallback: Direct convolution NCHW */
    (void)algo; (void)workspace; (void)workspace_size;
    
    int N, C, H, W;
    bear_cudnn_get_tensor_shape(x_desc, &N, &C, &H, &W);
    
    int K, C_w, R, S;
    bear_cudnn_get_tensor_shape(w_desc, &K, &C_w, &R, &S);
    
    int out_N, out_C, out_H, out_W;
    bear_cudnn_get_tensor_shape(y_desc, &out_N, &out_C, &out_H, &out_W);
    
    int pad_h = conv_desc[0];
    int pad_w = conv_desc[1];
    int stride_h = conv_desc[2];
    int stride_w = conv_desc[3];
    int dil_h = conv_desc[4];
    int dil_w = conv_desc[5];
    
    float a_val = alpha ? *alpha : 1.0f;
    float b_val = beta ? *beta : 0.0f;
    
    /* Initialize output */
    if (b_val != 0.0f) {
        for (int i = 0; i < out_N * out_C * out_H * out_W; ++i) {
            y[i] *= b_val;
        }
    } else {
        memset(y, 0, out_N * out_C * out_H * out_W * sizeof(float));
    }
    
    /* Direct convolution: y[n,k,oh,ow] = sum_c sum_r sum_s x[n,c,ih,iw] * w[k,c,r,s]
     * where ih = oh * stride_h - pad_h + r * dil_h
     *       iw = ow * stride_w - pad_w + s * dil_w
     */
    for (int n = 0; n < N; ++n) {
        for (int k = 0; k < K; ++k) {
            for (int oh = 0; oh < out_H; ++oh) {
                for (int ow = 0; ow < out_W; ++ow) {
                    float acc = 0.0f;
                    for (int c = 0; c < C; ++c) {
                        for (int r = 0; r < R; ++r) {
                            for (int s = 0; s < S; ++s) {
                                int ih = oh * stride_h - pad_h + r * dil_h;
                                int iw = ow * stride_w - pad_w + s * dil_w;
                                if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                    int x_idx = ((n * C + c) * H + ih) * W + iw;
                                    int w_idx = ((k * C + c) * R + r) * S + s;
                                    acc += x[x_idx] * w[w_idx];
                                }
                            }
                        }
                    }
                    int y_idx = ((n * K + k) * out_H + oh) * out_W + ow;
                    y[y_idx] += a_val * acc;
                }
            }
        }
    }
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
                                 dy_desc->desc, dy,
                                 conv_desc_obj, (cudnnConvolutionBwdDataAlgo_t)algo,
                                 workspace, workspace_size,
                                 beta, dx_desc->desc, dx);
    
    cudnnDestroyConvolutionDescriptor(conv_desc_obj);
#else
    /* CPU fallback: Convolution backward data (input gradient) */
    (void)algo; (void)workspace; (void)workspace_size;
    
    int K, C, R, S;
    bear_cudnn_get_tensor_shape(w_desc, &K, &C, &R, &S);
    
    int N, out_C, out_H, out_W;
    bear_cudnn_get_tensor_shape(dy_desc, &N, &out_C, &out_H, &out_W);
    
    int in_N, in_C, in_H, in_W;
    bear_cudnn_get_tensor_shape(dx_desc, &in_N, &in_C, &in_H, &in_W);
    
    int pad_h = conv_desc[0];
    int pad_w = conv_desc[1];
    int stride_h = conv_desc[2];
    int stride_w = conv_desc[3];
    int dil_h = conv_desc[4];
    int dil_w = conv_desc[5];
    
    float a_val = alpha ? *alpha : 1.0f;
    float b_val = beta ? *beta : 0.0f;
    
    /* Initialize dx */
    if (b_val != 0.0f) {
        for (int i = 0; i < in_N * in_C * in_H * in_W; ++i) dx[i] *= b_val;
    } else {
        memset(dx, 0, in_N * in_C * in_H * in_W * sizeof(float));
    }
    
    /* dx[n,c,ih,iw] = sum_k sum_oh sum_ow dy[n,k,oh,ow] * w[k,c,r,s]
     * where oh = (ih + pad_h - r * dil_h) / stride_h (must be integer)
     *       ow = (iw + pad_w - s * dil_w) / stride_w (must be integer)
     */
    for (int n = 0; n < N; ++n) {
        for (int c = 0; c < C; ++c) {
            for (int ih = 0; ih < in_H; ++ih) {
                for (int iw = 0; iw < in_W; ++iw) {
                    float acc = 0.0f;
                    for (int k = 0; k < K; ++k) {
                        for (int r = 0; r < R; ++r) {
                            for (int s = 0; s < S; ++s) {
                                int ih_padded = ih + pad_h - r * dil_h;
                                int iw_padded = iw + pad_w - s * dil_w;
                                if (ih_padded % stride_h == 0 && iw_padded % stride_w == 0) {
                                    int oh = ih_padded / stride_h;
                                    int ow = iw_padded / stride_w;
                                    if (oh >= 0 && oh < out_H && ow >= 0 && ow < out_W) {
                                        int dy_idx = ((n * K + k) * out_H + oh) * out_W + ow;
                                        int w_idx = ((k * C + c) * R + r) * S + s;
                                        acc += dy[dy_idx] * w[w_idx];
                                    }
                                }
                            }
                        }
                    }
                    int dx_idx = ((n * C + c) * in_H + ih) * in_W + iw;
                    dx[dx_idx] += a_val * acc;
                }
            }
        }
    }
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
    /* CPU fallback: Convolution backward filter (weight gradient) */
    (void)algo; (void)workspace; (void)workspace_size;
    
    int N, C, H, W;
    bear_cudnn_get_tensor_shape(x_desc, &N, &C, &H, &W);
    
    int dy_N, K, out_H, out_W;
    bear_cudnn_get_tensor_shape(dy_desc, &dy_N, &K, &out_H, &out_W);
    
    int dw_K, dw_C, R, S;
    bear_cudnn_get_tensor_shape(dw_desc, &dw_K, &dw_C, &R, &S);
    
    int pad_h = conv_desc[0];
    int pad_w = conv_desc[1];
    int stride_h = conv_desc[2];
    int stride_w = conv_desc[3];
    int dil_h = conv_desc[4];
    int dil_w = conv_desc[5];
    
    float a_val = alpha ? *alpha : 1.0f;
    float b_val = beta ? *beta : 0.0f;
    
    /* Initialize dw */
    if (b_val != 0.0f) {
        for (int i = 0; i < dw_K * dw_C * R * S; ++i) dw[i] *= b_val;
    } else {
        memset(dw, 0, dw_K * dw_C * R * S * sizeof(float));
    }
    
    /* dw[k,c,r,s] = sum_n sum_oh sum_ow x[n,c,ih,iw] * dy[n,k,oh,ow]
     * where ih = oh * stride_h - pad_h + r * dil_h
     *       iw = ow * stride_w - pad_w + s * dil_w
     */
    for (int k = 0; k < K; ++k) {
        for (int c = 0; c < C; ++c) {
            for (int r = 0; r < R; ++r) {
                for (int s = 0; s < S; ++s) {
                    float acc = 0.0f;
                    for (int n = 0; n < N; ++n) {
                        for (int oh = 0; oh < out_H; ++oh) {
                            for (int ow = 0; ow < out_W; ++ow) {
                                int ih = oh * stride_h - pad_h + r * dil_h;
                                int iw = ow * stride_w - pad_w + s * dil_w;
                                if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                    int x_idx = ((n * C + c) * H + ih) * W + iw;
                                    int dy_idx = ((n * K + k) * out_H + oh) * out_W + ow;
                                    acc += x[x_idx] * dy[dy_idx];
                                }
                            }
                        }
                    }
                    int dw_idx = ((k * C + c) * R + r) * S + s;
                    dw[dw_idx] += a_val * acc;
                }
            }
        }
    }
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
    /* CPU fallback: Element-wise activation */
    int N, C, H, W;
    bear_cudnn_get_tensor_shape(x_desc, &N, &C, &H, &W);
    int total = N * C * H * W;
    
    float a_val = alpha ? *alpha : 1.0f;
    float b_val = beta ? *beta : 0.0f;
    
    for (int i = 0; i < total; ++i) {
        float val = x[i];
        float result;
        switch (mode) {
            case 0: /* sigmoid */
                result = 1.0f / (1.0f + expf(-val));
                break;
            case 1: /* relu */
                result = val > 0 ? val : 0;
                break;
            case 2: /* tanh */
                result = tanhf(val);
                break;
            default:
                result = val > 0 ? val : 0;
                break;
        }
        y[i] = a_val * result + b_val * y[i];
    }
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
    /* CPU fallback: Max/Average pooling */
    int N, C, H, W;
    bear_cudnn_get_tensor_shape(x_desc, &N, &C, &H, &W);
    
    int out_N, out_C, out_H, out_W;
    bear_cudnn_get_tensor_shape(y_desc, &out_N, &out_C, &out_H, &out_W);
    
    int window_h = pool_desc[0];
    int window_w = pool_desc[1];
    int pad_h = pool_desc[2];
    int pad_w = pool_desc[3];
    int stride_h = pool_desc[4];
    int stride_w = pool_desc[5];
    
    float a_val = 1.0f; /* alpha is always 1.0 for pooling */
    float b_val = beta ? *beta : 0.0f;
    
    /* Initialize output */
    if (b_val != 0.0f) {
        for (int i = 0; i < out_N * out_C * out_H * out_W; ++i) y[i] *= b_val;
    } else {
        memset(y, 0, out_N * out_C * out_H * out_W * sizeof(float));
    }
    
    for (int n = 0; n < N; ++n) {
        for (int c = 0; c < C; ++c) {
            for (int oh = 0; oh < out_H; ++oh) {
                for (int ow = 0; ow < out_W; ++ow) {
                    int h_start = oh * stride_h - pad_h;
                    int w_start = ow * stride_w - pad_w;
                    int h_end = h_start + window_h;
                    int w_end = w_start + window_w;
                    
                    if (mode == 0) { /* max pooling */
                        float max_val = -INFINITY;
                        for (int kh = h_start; kh < h_end; ++kh) {
                            for (int kw = w_start; kw < w_end; ++kw) {
                                if (kh >= 0 && kh < H && kw >= 0 && kw < W) {
                                    int idx = ((n * C + c) * H + kh) * W + kw;
                                    if (x[idx] > max_val) max_val = x[idx];
                                }
                            }
                        }
                        int y_idx = ((n * C + c) * out_H + oh) * out_W + ow;
                        y[y_idx] = a_val * max_val;
                    } else { /* average pooling (count include padding) */
                        float sum = 0.0f;
                        int count = 0;
                        for (int kh = h_start; kh < h_end; ++kh) {
                            for (int kw = w_start; kw < w_end; ++kw) {
                                if (kh >= 0 && kh < H && kw >= 0 && kw < W) {
                                    int idx = ((n * C + c) * H + kh) * W + kw;
                                    sum += x[idx];
                                }
                                count++;
                            }
                        }
                        int y_idx = ((n * C + c) * out_H + oh) * out_W + ow;
                        y[y_idx] = a_val * (count > 0 ? sum / count : 0.0f);
                    }
                }
            }
        }
    }
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
    /* CPU fallback: Softmax */
    int N, C, H, W;
    bear_cudnn_get_tensor_shape(x_desc, &N, &C, &H, &W);
    
    float a_val = alpha ? *alpha : 1.0f;
    float b_val = beta ? *beta : 0.0f;
    
    if (mode == 0) { /* CUDNN_SOFTMAX_MODE_INSTANCE: per-instance (spatial) softmax */
        /* Softmax over C dimension for each spatial location */
        for (int n = 0; n < N; ++n) {
            for (int h = 0; h < H; ++h) {
                for (int w = 0; w < W; ++w) {
                    /* Find max for numerical stability */
                    float max_val = -INFINITY;
                    for (int c = 0; c < C; ++c) {
                        int idx = ((n * C + c) * H + h) * W + w;
                        if (x[idx] > max_val) max_val = x[idx];
                    }
                    /* Compute exp and sum */
                    float sum = 0.0f;
                    for (int c = 0; c < C; ++c) {
                        int idx = ((n * C + c) * H + h) * W + w;
                        sum += expf(x[idx] - max_val);
                    }
                    /* Compute softmax */
                    for (int c = 0; c < C; ++c) {
                        int idx = ((n * C + c) * H + h) * W + w;
                        float result = expf(x[idx] - max_val) / sum;
                        y[idx] = a_val * result + b_val * y[idx];
                    }
                }
            }
        }
    } else { /* CUDNN_SOFTMAX_MODE_CHANNEL: per-channel (global) softmax */
        /* Softmax over all elements in each channel */
        for (int n = 0; n < N; ++n) {
            for (int c = 0; c < C; ++c) {
                float max_val = -INFINITY;
                for (int h = 0; h < H; ++h) {
                    for (int w = 0; w < W; ++w) {
                        int idx = ((n * C + c) * H + h) * W + w;
                        if (x[idx] > max_val) max_val = x[idx];
                    }
                }
                float sum = 0.0f;
                for (int h = 0; h < H; ++h) {
                    for (int w = 0; w < W; ++w) {
                        int idx = ((n * C + c) * H + h) * W + w;
                        sum += expf(x[idx] - max_val);
                    }
                }
                for (int h = 0; h < H; ++h) {
                    for (int w = 0; w < W; ++w) {
                        int idx = ((n * C + c) * H + h) * W + w;
                        float result = expf(x[idx] - max_val) / sum;
                        y[idx] = a_val * result + b_val * y[idx];
                    }
                }
            }
        }
    }
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
    /* CPU fallback: Estimate workspace size for direct convolution
     * Direct convolution doesn't need workspace, but we return a reasonable
     * estimate based on im2col buffer size for potential im2col+GEMM approach */
    (void)algo;
    int N, C, H, W;
    bear_cudnn_get_tensor_shape(x_desc, &N, &C, &H, &W);
    
    int K, C_w, R, S;
    bear_cudnn_get_tensor_shape(w_desc, &K, &C_w, &R, &S);
    
    int out_N, out_C, out_H, out_W;
    bear_cudnn_get_tensor_shape(y_desc, &out_N, &out_C, &out_H, &out_W);
    
    /* im2col buffer: [N, C*R*S, out_H*out_W] */
    size_t im2col_size = (size_t)N * C * R * S * out_H * out_W * sizeof(float);
    /* Weight matrix: [K, C*R*S] */
    size_t weight_size = (size_t)K * C * R * S * sizeof(float);
    /* Output matrix: [K, out_H*out_W] */
    size_t output_size = (size_t)K * out_H * out_W * sizeof(float);
    
    return im2col_size + weight_size + output_size;
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
    /* CPU fallback: Estimate workspace size for backward data (input gradient) */
    (void)algo;
    int K, C, R, S;
    bear_cudnn_get_tensor_shape(w_desc, &K, &C, &R, &S);
    
    int N, out_C, out_H, out_W;
    bear_cudnn_get_tensor_shape(dy_desc, &N, &out_C, &out_H, &out_W);
    
    int in_N, in_C, in_H, in_W;
    bear_cudnn_get_tensor_shape(dx_desc, &in_N, &in_C, &in_H, &in_W);
    
    /* im2col-like buffer for backward data */
    size_t col_size = (size_t)N * C * R * S * out_H * out_W * sizeof(float);
    size_t weight_size = (size_t)K * C * R * S * sizeof(float);
    size_t output_size = (size_t)N * C * in_H * in_W * sizeof(float);
    
    return col_size + weight_size + output_size;
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
    /* CPU fallback: Estimate workspace size for backward filter (weight gradient) */
    (void)algo;
    int N, C, H, W;
    bear_cudnn_get_tensor_shape(x_desc, &N, &C, &H, &W);
    
    int out_N, out_C, out_H, out_W;
    bear_cudnn_get_tensor_shape(dy_desc, &out_N, &out_C, &out_H, &out_W);
    
    int K, C_w, R, S;
    bear_cudnn_get_tensor_shape(dw_desc, &K, &C_w, &R, &S);
    
    /* im2col buffer for x: [N, C*R*S, out_H*out_W] */
    size_t col_size = (size_t)N * C * R * S * out_H * out_W * sizeof(float);
    /* dy matrix: [K, out_H*out_W] */
    size_t dy_size = (size_t)K * out_H * out_W * sizeof(float);
    /* dw matrix: [K, C*R*S*R*S] */
    size_t dw_size = (size_t)K * C * R * S * sizeof(float);
    
    return col_size + dy_size + dw_size;
#endif
}

/* ===================================================================
 * CUDA Memory Management
 * =================================================================== */


