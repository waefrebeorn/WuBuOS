/*
 * bear_cuda.c  --  BearRL CUDA Host Context Management
 *
 * CUDA runtime initialization, device management, memory pools, streams.
 * Pure C11 host code  --  kernels live in bear_kernels.cu
 */

#include "bear_cuda.h"
#include "bear_arena.h"
#include <cuda_runtime.h>
#include <cuda.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Local copy of cudaDeviceProp fields we need (for compatibility) */
typedef struct {
    char name[256];
    size_t totalGlobalMem;
    size_t sharedMemPerBlock;
    int regsPerBlock;
    int warpSize;
    size_t memPitch;
    int maxThreadsPerBlock;
    int maxThreadsDim[3];
    int maxGridSize[3];
    int clockRate;
    size_t totalConstMem;
    int major;
    int minor;
    size_t textureAlignment;
    size_t texturePitchAlignment;
    int deviceOverlap;
    int multiProcessorCount;
    int kernelExecTimeoutEnabled;
    int integrated;
    int canMapHostMemory;
    int computeMode;
    int maxTexture1D;
    int maxTexture1DMipmap;
    int maxTexture1DLayered;
    int maxTexture2D[2];
    int maxTexture2DMipmap[2];
    int maxTexture2DLayered[3];
    int maxTexture3D[3];
    int maxTexture3DAlt[3];
    int maxTextureCubemap;
    int maxTextureCubemapLayered[2];
    int maxSurface1D;
    int maxSurface2D[2];
    int maxSurface3D[3];
    int maxSurfaceCubemap;
    int maxSurface1DLayered[2];
    int maxSurface2DLayered[3];
    int maxSurfaceCubemapLayered[2];
    size_t surfaceAlignment;
    int concurrentKernels;
    int ECCEnabled;
    int pciBusID;
    int pciDeviceID;
    int pciDomainID;
    int tccDriver;
    int asyncEngineCount;
    int unifiedAddressing;
    int memoryClockRate;
    int memoryBusWidth;
    int l2CacheSize;
    int maxThreadsPerMultiProcessor;
    int streamPrioritiesSupported;
    int globalL1CacheSupported;
    int localL1CacheSupported;
    size_t sharedMemPerMultiprocessor;
    int regsPerMultiprocessor;
    int managedMemory;
    int isMultiGpuBoard;
    int multiGpuBoardGroupID;
    int hostNativeAtomicSupported;
    int singleToDoublePrecisionPerfRatio;
    int pageableMemoryAccess;
    int concurrentManagedAccess;
    int computePreemptionSupported;
    int canUseHostPointerForRegisteredMem;
    int cooperativeLaunch;
    int cooperativeMultiDeviceLaunch;
    int maxSharedMemoryPerBlockOptin;
    int maxBlocksPerMultiprocessor;
    int maxBlocksPerMultiprocessorWithOption;
} CudaDeviceProp;

/* ==================================================================
 * Internal Context Structure
 * ================================================================== */

#define BEAR_CUDA_MAX_STREAMS 8
#define BEAR_CUDA_MAX_EVENTS 16

struct BearCudaContext {
    int device_id;
    CudaDeviceProp prop;
    int initialized;
    
    /* Streams for async execution */
    cudaStream_t streams[BEAR_CUDA_MAX_STREAMS];
    int num_streams;
    int next_stream;
    
    /* Events for timing/profiling */
    cudaEvent_t events[BEAR_CUDA_MAX_EVENTS];
    int num_events;
    
    /* Error tracking */
    cudaError_t last_error;
    char last_error_str[256];
    
    /* Profiling */
    int profiling_enabled;
    int num_profile_events;
    BearCudaProfileEvent profile_events[128];
    double profile_start_time;
};

/* ==================================================================
 * GPU Arena Implementation
 * ================================================================== */

struct BearGpuArena {
    BearCudaContext* ctx;
    size_t capacity;
    size_t offset;
    void* base_ptr;
    int owns_memory;
};

BearCudaStatus bear_cuda_query(void) {
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess || device_count == 0) {
        return BEAR_CUDA_UNAVAILABLE;
    }
    return BEAR_CUDA_AVAILABLE;
}

int bear_cuda_get_device_info(int device_id, BearCudaDeviceInfo* info) {
    if (!info) return -1;
    
    int count = 0;
    cudaGetDeviceCount(&count);
    if (device_id < 0 || device_id >= count) return -1;
    
    CudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, device_id);
    if (err != cudaSuccess) return -1;
    
    info->device_id = device_id;
    strncpy(info->name, prop.name, 255);
    info->name[255] = '\0';
    info->compute_capability_major = prop.major;
    info->compute_capability_minor = prop.minor;
    info->global_mem_bytes = prop.totalGlobalMem;
    info->shared_mem_per_block = prop.sharedMemPerBlock;
    info->max_threads_per_block = prop.maxThreadsPerBlock;
    info->max_blocks_per_sm = prop.maxBlocksPerMultiprocessor;
    info->num_sms = prop.multiProcessorCount;
    info->warp_size = prop.warpSize;
    info->clock_rate_khz = prop.clockRate;
    
    return 0;
}

/* ==================================================================
 * Context Management
 * ================================================================== */

static void cuda_check(BearCudaContext* ctx, cudaError_t err, const char* file, int line) {
    if (err != cudaSuccess) {
        ctx->last_error = err;
        snprintf(ctx->last_error_str, 256, "%s:%d: %s", file, line, cudaGetErrorString(err));
    }
}

#define CUDA_CHECK(ctx, expr) cuda_check(ctx, (expr), __FILE__, __LINE__)

BearCudaContext* bear_cuda_init(int device_id) {
    BearCudaStatus status = bear_cuda_query();
    if (status == BEAR_CUDA_UNAVAILABLE) {
        return NULL;
    }
    
    int count = 0;
    cudaGetDeviceCount(&count);
    if (device_id < 0) {
        /* Auto-select: pick device with most compute capability */
        int best = 0, best_cc = 0;
        for (int i = 0; i < count; ++i) {
            CudaDeviceProp p;
            cudaGetDeviceProperties(&p, i);
            int cc = p.major * 10 + p.minor;
            if (cc > best_cc) { best_cc = cc; best = i; }
        }
        device_id = best;
    }
    
    if (device_id >= count) return NULL;
    
    BearCudaContext* ctx = calloc(1, sizeof(BearCudaContext));
    if (!ctx) return NULL;
    
    CUDA_CHECK(ctx, cudaSetDevice(device_id));
    CUDA_CHECK(ctx, cudaGetDeviceProperties(&ctx->prop, device_id));
    ctx->device_id = device_id;
    
    /* Create streams */
    ctx->num_streams = BEAR_CUDA_MAX_STREAMS;
    for (int i = 0; i < ctx->num_streams; ++i) {
        CUDA_CHECK(ctx, cudaStreamCreate(&ctx->streams[i]));
    }
    ctx->next_stream = 0;
    
    /* Create events */
    ctx->num_events = BEAR_CUDA_MAX_EVENTS;
    for (int i = 0; i < ctx->num_events; ++i) {
        CUDA_CHECK(ctx, cudaEventCreate(&ctx->events[i]));
    }
    
    ctx->initialized = 1;
    ctx->last_error = cudaSuccess;
    ctx->profiling_enabled = 0;
    
    return ctx;
}

void bear_cuda_destroy(BearCudaContext* ctx) {
    if (!ctx) return;
    
    for (int i = 0; i < ctx->num_streams; ++i) {
        if (ctx->streams[i]) cudaStreamDestroy(ctx->streams[i]);
    }
    for (int i = 0; i < ctx->num_events; ++i) {
        if (ctx->events[i]) cudaEventDestroy(ctx->events[i]);
    }
    
    cudaDeviceReset();
    free(ctx);
}

void bear_cuda_sync(BearCudaContext* ctx) {
    if (!ctx) return;
    CUDA_CHECK(ctx, cudaDeviceSynchronize());
}

const char* bear_cuda_last_error(BearCudaContext* ctx) {
    if (!ctx) return "NULL context";
    if (ctx->last_error == cudaSuccess) return "No error";
    return ctx->last_error_str;
}

/* ==================================================================
 * GPU Arena
 * ================================================================== */

BearGpuArena* bear_gpu_arena_create(BearCudaContext* ctx, size_t capacity_bytes) {
    if (!ctx || !ctx->initialized) return NULL;
    
    BearGpuArena* arena = calloc(1, sizeof(BearGpuArena));
    if (!arena) return NULL;
    
    arena->ctx = ctx;
    arena->capacity = capacity_bytes;
    arena->offset = 0;
    arena->owns_memory = 1;
    
    CUDA_CHECK(ctx, cudaMalloc(&arena->base_ptr, capacity_bytes));
    if (ctx->last_error != cudaSuccess) {
        free(arena);
        return NULL;
    }
    
    return arena;
}

void bear_gpu_arena_reset(BearGpuArena* arena) {
    if (!arena) return;
    arena->offset = 0;
}

void bear_gpu_arena_destroy(BearGpuArena* arena) {
    if (!arena) return;
    if (arena->owns_memory && arena->base_ptr) {
        cudaFree(arena->base_ptr);
    }
    free(arena);
}

void* bear_gpu_arena_alloc(BearGpuArena* arena, size_t bytes) {
    if (!arena || !arena->ctx || !arena->base_ptr) return NULL;
    
    /* Align to 256 bytes for coalesced access */
    size_t aligned_offset = (arena->offset + 255) & ~255;
    if (aligned_offset + bytes > arena->capacity) {
        /* Out of space  --  could grow or return NULL */
        return NULL;
    }
    
    void* ptr = (char*)arena->base_ptr + aligned_offset;
    arena->offset = aligned_offset + bytes;
    return ptr;
}

/* ==================================================================
 * GPU Tensor Operations
 * ================================================================== */

static size_t dtype_size(int dtype) {
    switch (dtype) {
        case 0: return sizeof(float);  /* BEAR_DTYPE_F32 */
        case 1: return sizeof(double);
        case 2: return sizeof(int32_t); /* BEAR_DTYPE_INDICES */
        case 3: return sizeof(uint8_t);
        default: return sizeof(float);
    }
}

static int64_t tensor_numel(const int64_t* shape, int ndim) {
    int64_t n = 1;
    for (int i = 0; i < ndim; ++i) n *= shape[i];
    return n;
}

int bear_gpu_tensor_from_host(BearCudaContext* ctx, BearGpuArena* arena,
                               const BearTensor* host, BearGpuTensor* gpu) {
    if (!ctx || !host || !gpu) return -1;
    
    int64_t numel = bear_tensor_numel(host);
    size_t element_size = dtype_size(host->dtype);
    size_t bytes = numel * element_size;
    
    gpu->ndim = host->ndim;
    gpu->dtype = host->dtype;
    gpu->numel = numel;
    gpu->bytes = bytes;
    gpu->loc = BEAR_GPU_TENSOR_DEVICE;
    
    /* Copy shape to host array */
    gpu->shape = malloc(host->ndim * sizeof(int64_t));
    if (!gpu->shape) return -1;
    memcpy(gpu->shape, host->shape, host->ndim * sizeof(int64_t));
    
    /* Allocate device memory */
    void* dev_ptr = NULL;
    if (arena) {
        dev_ptr = bear_gpu_arena_alloc(arena, bytes);
    } else {
        cudaMalloc(&dev_ptr, bytes);
    }
    
    if (!dev_ptr) return -1;
    gpu->data = dev_ptr;
    
    /* Copy data H2D */
    cudaError_t err = cudaMemcpy(dev_ptr, host->data, bytes, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) return -1;
    
    return 0;
}

int bear_gpu_tensor_create(BearCudaContext* ctx, BearGpuArena* arena,
                            const int64_t* shape, int ndim, int dtype,
                            BearGpuTensor* gpu, const char* name) {
    (void)name;
    if (!ctx || !gpu) return -1;
    
    int64_t numel = tensor_numel(shape, ndim);
    size_t element_size = dtype_size(dtype);
    size_t bytes = numel * element_size;
    
    gpu->ndim = ndim;
    gpu->dtype = dtype;
    gpu->numel = numel;
    gpu->bytes = bytes;
    gpu->loc = BEAR_GPU_TENSOR_DEVICE;
    
    gpu->shape = malloc(ndim * sizeof(int64_t));
    if (!gpu->shape) return -1;
    memcpy(gpu->shape, shape, ndim * sizeof(int64_t));
    
    void* dev_ptr = NULL;
    if (arena) {
        dev_ptr = bear_gpu_arena_alloc(arena, bytes);
    } else {
        cudaMalloc(&dev_ptr, bytes);
    }
    
    if (!dev_ptr) {
        free(gpu->shape);
        return -1;
    }
    gpu->data = dev_ptr;
    
    return 0;
}

int bear_gpu_tensor_to_host(BearCudaContext* ctx, const BearGpuTensor* gpu, BearTensor* host) {
    if (!ctx || !gpu || !host) return -1;
    if (gpu->loc != BEAR_GPU_TENSOR_DEVICE) return -1;
    
    /* Allocate host tensor if needed */
    if (!host->data) {
        /* We can't easily create BearTensor here without arena, 
           assume caller pre-allocated */
    }
    
    cudaError_t err = cudaMemcpy(host->data, gpu->data, gpu->bytes, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) return -1;
    
    /* Copy shape */
    if (host->ndim != gpu->ndim) return -1;
    
    return 0;
}

int bear_gpu_tensor_copy(BearCudaContext* ctx, const BearGpuTensor* src, BearGpuTensor* dst) {
    if (!ctx || !src || !dst) return -1;
    if (src->numel != dst->numel || src->dtype != dst->dtype) return -1;
    
    cudaError_t err = cudaMemcpy(dst->data, src->data, src->bytes, cudaMemcpyDeviceToDevice);
    if (err != cudaSuccess) return -1;
    
    return 0;
}

void bear_gpu_tensor_free(BearGpuArena* arena, BearGpuTensor* gpu) {
    if (!gpu) return;
    if (gpu->shape) {
        free(gpu->shape);
        gpu->shape = NULL;
    }
    gpu->data = NULL;
    /* Note: memory is freed when arena is reset/destroyed */
}

/* ==================================================================
 * Profiling
 * ================================================================== */

void bear_cuda_profile_enable(BearCudaContext* ctx, int enable) {
    if (!ctx) return;
    ctx->profiling_enabled = enable;
    if (enable) {
        ctx->num_profile_events = 0;
        ctx->profile_start_time = 0;
    }
}

int bear_cuda_profile_get_events(BearCudaContext* ctx, BearCudaProfileEvent* out, int max_events) {
    if (!ctx || !out || max_events <= 0) return 0;
    int n = ctx->num_profile_events < max_events ? ctx->num_profile_events : max_events;
    memcpy(out, ctx->profile_events, n * sizeof(BearCudaProfileEvent));
    return n;
}

void bear_cuda_profile_reset(BearCudaContext* ctx) {
    if (!ctx) return;
    ctx->num_profile_events = 0;
}

/* ==================================================================
 * Stream Management
 * ================================================================== */

cudaStream_t bear_cuda_get_stream(BearCudaContext* ctx) {
    if (!ctx || ctx->num_streams == 0) return 0; /* default stream */
    cudaStream_t s = ctx->streams[ctx->next_stream];
    ctx->next_stream = (ctx->next_stream + 1) % ctx->num_streams;
    return s;
}

/* ==================================================================
 * Utility: Pick best GPU device
 * ================================================================== */

int bear_cuda_pick_best_device(void) {
    int count = 0;
    cudaGetDeviceCount(&count);
    if (count <= 0) return -1;
    
    int best = 0, best_score = 0;
    for (int i = 0; i < count; ++i) {
        CudaDeviceProp p;
        cudaGetDeviceProperties(&p, i);
        /* Score: SMs * clock_rate * compute_capability */
        int score = p.multiProcessorCount * (p.clockRate / 1000) * (p.major * 10 + p.minor);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}
