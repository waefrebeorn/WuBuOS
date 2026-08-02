/*
 * wubu_hwdetect.c -- WuBuOS hardware-acceleration detection. C11.
 * CPUID via inline asm (freestanding-safe); GPU probing via stat().
 */
#include "wubu_hwdetect.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

static void cpuid(uint32_t leaf, uint32_t sub,
                  uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile(
        "cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(sub));
#else
    (void)leaf; (void)sub; *a = *b = *c = *d = 0;
#endif
}

static int has_file(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
}

int wubu_hw_detect(wubu_hw_t *hw)
{
    if (!hw) return -1;
    memset(hw, 0, sizeof(*hw));

    /* ---- CPU vendor + brand ---- */
    uint32_t a, b, c, d;
    cpuid(0, 0, &a, &b, &c, &d);
    memcpy(hw->vendor + 0, &b, 4);
    memcpy(hw->vendor + 4, &d, 4);
    memcpy(hw->vendor + 8, &c, 4);
    hw->vendor[12] = 0;

    if (a >= 0x80000004) {
        char *dst = hw->brand;
        for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
            cpuid(leaf, 0, &a, &b, &c, &d);
            memcpy(dst, &a, 4); memcpy(dst + 4, &b, 4);
            memcpy(dst + 8, &c, 4); memcpy(dst + 12, &d, 4);
            dst += 16;
        }
        hw->brand[63] = 0;
    }

    /* ---- leaf 1: SSE/AVX/FMA ---- */
    cpuid(1, 0, &a, &b, &c, &d);
    int has_sse2 = (d >> 26) & 1;
    int has_ssse3 = (c >> 9) & 1;
    int has_sse41 = (c >> 19) & 1;
    int has_sse42 = (c >> 20) & 1;
    int has_avx = (c >> 28) & 1;
    int has_fma = (c >> 12) & 1;
    int has_osxsave = (c >> 27) & 1;

    /* the OS must have enabled the XSAVE/AVX state (the XCR0 check) */
    int avx_usable = has_avx && has_osxsave;
#if defined(__x86_64__)
    if (avx_usable) {
        uint32_t xcr0_lo, xcr0_hi;
        __asm__ volatile("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
        if (((xcr0_lo & 6) != 6)) avx_usable = 0;   /* XMM+YMM enabled */
    }
#endif

    /* ---- leaf 7: AVX2 / BMI / AVX512 ---- */
    int has_avx2 = 0, has_bmi1 = 0, has_bmi2 = 0;
    int has_avx512f = 0, has_avx512bw = 0, has_avx512vl = 0, has_vnni = 0;
    if (a >= 7) {
        cpuid(7, 0, &a, &b, &c, &d);
        has_avx2 = (b >> 5) & 1;
        has_bmi1 = (b >> 3) & 1;
        has_bmi2 = (b >> 8) & 1;
        has_avx512f = (b >> 16) & 1;
        has_avx512bw = (b >> 30) & 1;
        has_avx512vl = (b >> 31) & 1;
        has_vnni = (c >> 11) & 1;
    }

    hw->has_ssse3 = has_ssse3;
    hw->has_sse41 = has_sse41;
    hw->has_sse42 = has_sse42;
    hw->has_fma = has_fma && avx_usable;
    hw->has_bmi1 = has_bmi1;
    hw->has_bmi2 = has_bmi2;
    hw->has_avx512_vnni = has_vnni && avx_usable;
    hw->has_avx512f = has_avx512f && avx_usable;
    hw->has_avx512bw = has_avx512bw && avx_usable;
    hw->has_avx512vl = has_avx512vl && avx_usable;

    /* the tier: the highest fully-usable level */
    if (has_avx512f && has_avx512bw && has_avx512vl && avx_usable)
        hw->simd_tier = WUBU_HW_AVX512;
    else if (has_avx2 && avx_usable)
        hw->simd_tier = WUBU_HW_AVX2;
    else if (has_avx && avx_usable)
        hw->simd_tier = WUBU_HW_AVX;
    else if (has_sse2)
        hw->simd_tier = WUBU_HW_SSE2;
    else
        hw->simd_tier = WUBU_HW_SCALAR;

    /* ---- logical cores ---- */
    cpuid(1, 0, &a, &b, &c, &d);
    int lc = (b >> 24) & 0xFF;
    cpuid(0x80000008, 0, &a, &b, &c, &d);
    int nc = (a & 0xFF) + 1;
    hw->cores_logical = lc ? lc : nc;
    hw->cores_physical = nc;

    /* ---- GPU matrix ---- */
    hw->dxg_present = has_file("/dev/dxg");          /* WSL2 gfxstream */
    hw->cuda_present = has_file("/dev/nvidia0") || has_file("/dev/nvidiactl");

    if (hw->cuda_present) {
        hw->gpu_backend = WUBU_GPU_CUDA;
        hw->vulkan_devices = 1;   /* CUDA implies a real device */
    } else if (hw->dxg_present) {
        hw->gpu_backend = WUBU_GPU_VULKAN;
        hw->vulkan_devices = 1;
    } else {
        hw->gpu_backend = WUBU_GPU_SW;   /* llvmpipe always present */
        hw->vulkan_devices = 1;
    }
    return 0;
}

int wubu_hw_dispatch_tier(const wubu_hw_t *hw)
{
    if (!hw) return WUBU_HW_SCALAR;
    return hw->simd_tier;
}

int wubu_hw_kernel_choice(const wubu_hw_t *hw)
{
    /* the kernel family: 0 = scalar, 1 = sse2, 2 = avx2, 3 = avx512 */
    int t = wubu_hw_dispatch_tier(hw);
    switch (t) {
    case WUBU_HW_AVX512: return 3;
    case WUBU_HW_AVX2:   return 2;
    case WUBU_HW_AVX:    return 2;   /* AVX reuses the AVX2 family */
    default:             return t;
    }
}

int wubu_hw_gpu_choice(const wubu_hw_t *hw)
{
    if (!hw) return WUBU_GPU_SW;
    return hw->gpu_backend;
}

const char *wubu_hw_tier_name(int tier)
{
    switch (tier) {
    case WUBU_HW_AVX512: return "AVX512";
    case WUBU_HW_AVX2:   return "AVX2";
    case WUBU_HW_AVX:    return "AVX";
    case WUBU_HW_SSE2:   return "SSE2";
    default:             return "scalar";
    }
}

const char *wubu_hw_gpu_name(int backend)
{
    switch (backend) {
    case WUBU_GPU_CUDA:   return "cuda";
    case WUBU_GPU_VULKAN: return "vulkan";
    case WUBU_GPU_SW:     return "llvmpipe";
    default:              return "none";
    }
}

uint32_t wubu_hw_matrix(const wubu_hw_t *hw)
{
    if (!hw) return 0;
    uint32_t m = 0;
    m |= (uint32_t)hw->simd_tier;            /* bits 0-2 */
    m |= hw->has_fma ? (1u << 3) : 0;
    m |= hw->has_bmi2 ? (1u << 4) : 0;
    m |= hw->has_avx512_vnni ? (1u << 5) : 0;
    m |= (uint32_t)hw->gpu_backend << 8;     /* bits 8-9 */
    m |= hw->dxg_present ? (1u << 16) : 0;
    m |= hw->cuda_present ? (1u << 17) : 0;
    return m;
}
