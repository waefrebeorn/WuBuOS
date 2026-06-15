/*
 * bear_arena.h  --  PufferC/BearRL Arena Allocator + SoA Tensor Infrastructure
 *
 * Sovereign C11 RL stack  --  zero Python, zero PyTorch, zero external deps.
 * Performance: Arena allocation, SoA tensors, SIMD-ready.
 *
 * Design:
 *   - Arena per epoch/rollout (reset, don't free)
 *   - Global arena for weights/grads (persistent)
 *   - Tensors as Structure of Arrays (SoA) for SIMD vectorization
 *   - All shapes fixed at init  --  no runtime alloc after setup
 */

#ifndef BEAR_ARENA_H
#define BEAR_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ===================================================================
 * Configuration Constants
 * =================================================================== */

#define BEAR_MAX_TENSOR_DIMS    4
#define BEAR_MAX_TENSOR_NAME    32
#define BEAR_ARENA_ALIGN        64  /* cache-line + AVX-512 friendly */
#define BEAR_DEFAULT_ARENA_CAP  (64 * 1024 * 1024)  /* 64 MiB default */

/* ===================================================================
 * Arena Allocator  --  Linear bump pointer, reset per epoch
 * =================================================================== */

typedef struct {
    uint8_t* base;      /* start of allocation */
    uint8_t* ptr;       /* current bump pointer */
    size_t   cap;       /* total capacity in bytes */
    size_t   used;      /* bytes allocated so far */
    size_t   peak_used; /* high-water mark for debugging */
} BearArena;

/* Initialize arena from pre-allocated memory (mmap, malloc, static) */
static inline void bear_arena_init(BearArena* a, void* backing, size_t cap) {
    a->base = (uint8_t*)backing;
    a->ptr  = (uint8_t*)backing;
    a->cap  = cap;
    a->used = 0;
    a->peak_used = 0;
}

/* Create arena with malloc (for host) */
static inline int bear_arena_create(BearArena* a, size_t cap) {
    void* mem = aligned_alloc(BEAR_ARENA_ALIGN, cap);
    if (!mem) return -1;
    bear_arena_init(a, mem, cap);
    return 0;
}

/* Destroy arena (free backing if malloc'd) */
static inline void bear_arena_destroy(BearArena* a) {
    if (a->base) {
        free(a->base);
        a->base = a->ptr = NULL;
        a->cap = a->used = 0;
    }
}

/* Reset bump pointer  --  fast epoch/rollout boundary */
static inline void bear_arena_reset(BearArena* a) {
    if (a->used > a->peak_used) a->peak_used = a->used;
    a->ptr = a->base;
    a->used = 0;
}

/* Aligned allocation from arena  --  returns NULL on OOM */
static inline void* bear_arena_alloc(BearArena* a, size_t size, size_t align) {
    uintptr_t p = (uintptr_t)a->ptr;
    uintptr_t aligned = (p + align - 1) & ~(uintptr_t)(align - 1);
    size_t padding = aligned - p;
    if (a->used + padding + size > a->cap) return NULL;
    a->ptr += padding;
    void* out = a->ptr;
    a->ptr += size;
    a->used += padding + size;
    return out;
}

/* Convenience: typed allocation */
#define BEAR_ARENA_ALLOC(a, type, count) \
    (type*)bear_arena_alloc(a, (count) * sizeof(type), alignof(type))

/* Scratch arena for temporary allocations within a function scope */
typedef struct {
    BearArena* arena;
    size_t     mark;
} BearScratch;

static inline BearScratch bear_scratch_begin(BearArena* a) {
    return (BearScratch){ .arena = a, .mark = a->used };
}

static inline void bear_scratch_end(BearScratch s) {
    s.arena->ptr = s.arena->base + s.mark;
    s.arena->used = s.mark;
}

/* ===================================================================
 * SoA Tensor  --  Structure of Arrays for SIMD
 * =================================================================== */

typedef enum {
    BEAR_DTYPE_F32 = 0,
    BEAR_DTYPE_I32 = 1,
    BEAR_DTYPE_U8  = 2,
    BEAR_DTYPE_I64 = 3,
} BearDType;

static inline size_t bear_dtype_size(BearDType dt) {
    switch (dt) {
        case BEAR_DTYPE_F32: return sizeof(float);
        case BEAR_DTYPE_I32: return sizeof(int32_t);
        case BEAR_DTYPE_U8:  return sizeof(uint8_t);
        case BEAR_DTYPE_I64: return sizeof(int64_t);
    }
    return 0;
}

/* Tensor descriptor  --  data allocated from arena, strides computed at init */
typedef struct {
    void*        data;           /* base pointer (from arena) */
    int64_t      shape[BEAR_MAX_TENSOR_DIMS];
    int64_t      stride[BEAR_MAX_TENSOR_DIMS];
    int          ndim;
    BearDType    dtype;
    char         name[BEAR_MAX_TENSOR_NAME];
} BearTensor;

/* Compute strides (row-major / C-contiguous) */
static inline void bear_tensor_compute_strides(BearTensor* t) {
    t->stride[t->ndim - 1] = bear_dtype_size(t->dtype);
    for (int i = t->ndim - 2; i >= 0; --i) {
        t->stride[i] = t->stride[i + 1] * t->shape[i + 1];
    }
}

/* Create tensor  --  allocates data from arena */
static inline int bear_tensor_create(BearArena* a, BearTensor* t,
                                      const int64_t* shape, int ndim,
                                      BearDType dtype, const char* name) {
    if (ndim > BEAR_MAX_TENSOR_DIMS) return -1;
    
    t->ndim = ndim;
    t->dtype = dtype;
    size_t elems = 1;
    for (int i = 0; i < ndim; ++i) {
        t->shape[i] = shape[i];
        elems *= shape[i];
    }
    
    size_t byte_size = elems * bear_dtype_size(dtype);
    t->data = bear_arena_alloc(a, byte_size, BEAR_ARENA_ALIGN);
    if (!t->data) return -1;
    
    bear_tensor_compute_strides(t);
    
    if (name) {
        strncpy(t->name, name, BEAR_MAX_TENSOR_NAME - 1);
        t->name[BEAR_MAX_TENSOR_NAME - 1] = '\0';
    } else {
        t->name[0] = '\0';
    }
    
    return 0;
}

/* Create tensor from existing memory (no alloc) */
static inline void bear_tensor_wrap(BearTensor* t, void* data,
                                     const int64_t* shape, int ndim,
                                     BearDType dtype, const char* name) {
    t->data = data;
    t->ndim = ndim;
    t->dtype = dtype;
    for (int i = 0; i < ndim; ++i) t->shape[i] = shape[i];
    bear_tensor_compute_strides(t);
    if (name) strncpy(t->name, name, BEAR_MAX_TENSOR_NAME - 1);
    else t->name[0] = '\0';
}

/* Tensor element access (bounds-check in debug) */
#define BEAR_TENSOR_AT(t, dtype, ...) \
    *(dtype*)bear_tensor_ptr(t, (int64_t[]){__VA_ARGS__})

static inline void* bear_tensor_ptr(const BearTensor* t, const int64_t* idx) {
    uintptr_t offset = 0;
    for (int i = 0; i < t->ndim; ++i) {
        offset += (uintptr_t)idx[i] * (uintptr_t)t->stride[i];
    }
    return (uint8_t*)t->data + offset;
}

/* Total elements */
static inline int64_t bear_tensor_numel(const BearTensor* t) {
    int64_t n = 1;
    for (int i = 0; i < t->ndim; ++i) n *= t->shape[i];
    return n;
}

/* Bytes per element */
static inline size_t bear_tensor_elem_bytes(const BearTensor* t) {
    return bear_dtype_size(t->dtype);
}

/* ===================================================================
 * Common RL Tensor Shapes (convenience factories)
 * =================================================================== */

/* Observation batch: [num_envs, obs_dim] */
static inline int bear_tensor_obs_batch(BearArena* a, BearTensor* t,
                                         int num_envs, int obs_dim, const char* name) {
    int64_t shape[2] = { num_envs, obs_dim };
    return bear_tensor_create(a, t, shape, 2, BEAR_DTYPE_F32, name);
}

/* Action batch: [num_envs, act_dim] */
static inline int bear_tensor_act_batch(BearArena* a, BearTensor* t,
                                         int num_envs, int act_dim, const char* name) {
    int64_t shape[2] = { num_envs, act_dim };
    return bear_tensor_create(a, t, shape, 2, BEAR_DTYPE_F32, name);
}

/* Scalar per env: [num_envs] */
static inline int bear_tensor_scalar_batch(BearArena* a, BearTensor* t,
                                            int num_envs, BearDType dtype, const char* name) {
    int64_t shape[1] = { num_envs };
    return bear_tensor_create(a, t, shape, 1, dtype, name);
}

/* 1D vector */
static inline int bear_tensor_vec(BearArena* a, BearTensor* t,
                                   int len, BearDType dtype, const char* name) {
    int64_t shape[1] = { len };
    return bear_tensor_create(a, t, shape, 1, dtype, name);
}

/* 2D weight matrix: [out_features, in_features] */
static inline int bear_tensor_weight(BearArena* a, BearTensor* t,
                                      int out_f, int in_f, const char* name) {
    int64_t shape[2] = { out_f, in_f };
    return bear_tensor_create(a, t, shape, 2, BEAR_DTYPE_F32, name);
}

/* ===================================================================
 * Tensor Operations (in-place, SIMD-friendly where possible)
 * =================================================================== */

/* Fill tensor with value */
static inline void bear_tensor_fill(BearTensor* t, float val) {
    if (t->dtype != BEAR_DTYPE_F32) return;
    int64_t n = bear_tensor_numel(t);
    float* p = (float*)t->data;
    for (int64_t i = 0; i < n; ++i) p[i] = val;
}

/* Copy tensor (must have same shape/dtype) */
static inline void bear_tensor_copy(BearTensor* dst, const BearTensor* src) {
    if (dst->dtype != src->dtype || dst->ndim != src->ndim) return;
    for (int i = 0; i < dst->ndim; ++i) if (dst->shape[i] != src->shape[i]) return;
    size_t bytes = bear_tensor_numel(src) * bear_dtype_size(src->dtype);
    memcpy(dst->data, src->data, bytes);
}

/* Scale tensor: dst = src * scale */
static inline void bear_tensor_scale(BearTensor* dst, const BearTensor* src, float scale) {
    if (dst->dtype != BEAR_DTYPE_F32 || src->dtype != BEAR_DTYPE_F32) return;
    int64_t n = bear_tensor_numel(src);
    float* d = (float*)dst->data;
    const float* s = (const float*)src->data;
    for (int64_t i = 0; i < n; ++i) d[i] = s[i] * scale;
}

/* Add: dst += src */
static inline void bear_tensor_add(BearTensor* dst, const BearTensor* src) {
    if (dst->dtype != BEAR_DTYPE_F32 || src->dtype != BEAR_DTYPE_F32) return;
    int64_t n = bear_tensor_numel(src);
    float* d = (float*)dst->data;
    const float* s = (const float*)src->data;
    for (int64_t i = 0; i < n; ++i) d[i] += s[i];
}

/* Element-wise multiply: dst *= src */
static inline void bear_tensor_mul(BearTensor* dst, const BearTensor* src) {
    if (dst->dtype != BEAR_DTYPE_F32 || src->dtype != BEAR_DTYPE_F32) return;
    int64_t n = bear_tensor_numel(src);
    float* d = (float*)dst->data;
    const float* s = (const float*)src->data;
    for (int64_t i = 0; i < n; ++i) d[i] *= s[i];
}

/* ===================================================================
 * Weight / Gradient Pair (for optimizer)
 * =================================================================== */

typedef struct {
    BearTensor weight;
    BearTensor bias;     /* Bias vector */
    BearTensor grad;
    BearTensor mom;      /* Adam: 1st moment */
    BearTensor var;      /* Adam: 2nd moment */
    BearTensor vel;      /* Muon: velocity */
    int        step;     /* Adam: step counter */
} BearParam;

/* Create parameter with all optimizer state */
static inline int bear_param_create(BearArena* a, BearParam* p,
                                     int out_f, int in_f, const char* name) {
    char wname[64], bname[64], gname[64], mname[64], vname[64], v2name[64];
    snprintf(wname, sizeof(wname), "%s.w", name);
    snprintf(bname, sizeof(bname), "%s.b", name);
    snprintf(gname, sizeof(gname), "%s.g", name);
    snprintf(mname, sizeof(mname), "%s.m", name);
    snprintf(vname, sizeof(vname), "%s.v", name);
    snprintf(v2name, sizeof(v2name), "%s.v2", name);
    
    if (bear_tensor_weight(a, &p->weight, out_f, in_f, wname) != 0) return -1;
    if (bear_tensor_weight(a, &p->bias,   1, out_f, bname) != 0) return -1;
    if (bear_tensor_weight(a, &p->grad,   out_f, in_f, gname) != 0) return -1;
    if (bear_tensor_weight(a, &p->mom,    out_f, in_f, mname) != 0) return -1;  /* Adam */
    if (bear_tensor_weight(a, &p->var,    out_f, in_f, vname) != 0) return -1;  /* Adam */
    if (bear_tensor_weight(a, &p->vel,    out_f, in_f, v2name) != 0) return -1; /* Muon */
    /* Zero-initialize optimizer state (arena memory is uninitialized) */
    bear_tensor_fill(&p->grad, 0.0f);
    bear_tensor_fill(&p->mom,  0.0f);
    bear_tensor_fill(&p->var,  0.0f);
    bear_tensor_fill(&p->vel,  0.0f);
    p->step = 0;
    /* Zero-init bias */
    bear_tensor_fill(&p->bias, 0.0f);
    return 0;
}

/* Zero gradient */
static inline void bear_param_zero_grad(BearParam* p) {
    bear_tensor_fill(&p->grad, 0.0f);
}

/* ===================================================================
 * Global Arenas (for easy access)
 * =================================================================== */

extern BearArena g_bear_global_arena;   /* weights, optimizer state */
extern BearArena g_bear_rollout_arena;  /* trajectory data, reset per epoch */

#endif /* BEAR_ARENA_H */