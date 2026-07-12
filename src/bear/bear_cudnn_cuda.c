/*
 * WuBuOS -- extracted module (auto-split, C11, opaque-safe)
 */

#include "bear_cudnn.h"
#include <stdlib.h>
#include <string.h>

void* hc_builtin_cuda_malloc(size_t bytes) {
#if BEAR_HAVE_CUDA
    void* ptr = NULL;
    cudaMalloc(&ptr, bytes);
    return ptr;
#else
    /* CPU fallback: Use regular malloc for host memory */
    return malloc(bytes);
#endif
}

void hc_builtin_cuda_free(void* ptr) {
#if BEAR_HAVE_CUDA
    if (ptr) cudaFree(ptr);
#else
    /* CPU fallback: Use regular free for host memory */
    free(ptr);
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
