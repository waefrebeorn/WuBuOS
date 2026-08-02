/*
 * wubu_hwdetect.h -- WuBuOS hardware-acceleration detection strategies.
 * C11. Freestanding-safe (CPUID via inline asm; no libc beyond memcpy).
 *
 * The strategy ladder (best-first):
 *   1. CPUID SIMD tier: scalar -> SSE2 -> AVX -> AVX2 -> AVX512 (with
 *      the FMA/BMI/AVX512-VNNI sub-flags). The tier selects the kernel
 *      family (the dispatch strategy).
 *   2. GPU backend matrix: CUDA (real NVIDIA) > Vulkan with a real
 *      physical device (gfxstream/dxg, mesa-radv, etc.) > llvmpipe
 *      (always-present CPU fallback) > none.
 *   3. The runtime dispatch: the engine picks the highest tier whose
 *      capability is present and whose kernel is compiled in.
 *
 * This mirrors the industry strategy (LocalAI/LM-Kit auto-detect):
 * enumerate everything, rank by real-device preference, never fail
 * (the software fallback is always last).
 */
#ifndef WUBU_HWDETECT_H
#define WUBU_HWDETECT_H

#include <stdint.h>

/* The SIMD tier (higher = more capable). */
enum {
    WUBU_HW_SCALAR = 0,   /* no SIMD */
    WUBU_HW_SSE2  = 1,
    WUBU_HW_AVX   = 2,
    WUBU_HW_AVX2  = 3,
    WUBU_HW_AVX512 = 4
};

/* The GPU backends, ranked by preference (low = best). */
enum {
    WUBU_GPU_NONE    = 0,
    WUBU_GPU_SW      = 1,   /* llvmpipe / software */
    WUBU_GPU_VULKAN  = 2,   /* a real Vulkan physical device */
    WUBU_GPU_CUDA    = 3    /* an NVIDIA device via /dev/nvidia* */
};

typedef struct {
    /* CPU */
    char vendor[16];
    char brand[64];
    int  simd_tier;         /* WUBU_HW_* */
    int  has_fma, has_bmi1, has_bmi2, has_avx512_vnni;
    int  has_ssse3, has_sse41, has_sse42;
    int  has_avx512f, has_avx512bw, has_avx512vl;
    int  cores_logical, cores_physical;
    /* GPU */
    int  gpu_backend;       /* WUBU_GPU_* (the best found) */
    int  vulkan_devices;    /* physical devices enumerated */
    int  cuda_present;      /* /dev/nvidia* visible */
    int  dxg_present;       /* WSL2 /dev/dxg (gfxstream) visible */
} wubu_hw_t;

/* Detect everything (cheap: CPUID + a couple of stat() calls).
 * Returns 0 on success. */
int wubu_hw_detect(wubu_hw_t *hw);

/* The best dispatch tier: the highest SIMD tier that has the flags. */
int wubu_hw_dispatch_tier(const wubu_hw_t *hw);

/* The strategy: which kernel family to use for a compute op.
 * Returns a stable enum usable as a dispatch index. */
int wubu_hw_kernel_choice(const wubu_hw_t *hw);

/* The GPU strategy: the best backend that can actually run. */
int wubu_hw_gpu_choice(const wubu_hw_t *hw);

/* Human-readable names (static strings). */
const char *wubu_hw_tier_name(int tier);
const char *wubu_hw_gpu_name(int backend);

/* The backend matrix as a compact bitmap (the detection telemetry). */
uint32_t wubu_hw_matrix(const wubu_hw_t *hw);

#endif
