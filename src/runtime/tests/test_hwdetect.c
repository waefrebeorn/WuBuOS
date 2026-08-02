/* test_hwdetect.c -- WuBuOS hardware-acceleration detection strategies. */
#include <stdio.h>
#include <string.h>
#include "wubu_hwdetect.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_hwdetect ===\n");
    wubu_hw_t hw;
    CHECK(wubu_hw_detect(&hw) == 0, "detect succeeds");

    /* every x86-64 CPU has SSE2; the tier must be >= SSE2 */
    CHECK(hw.simd_tier >= WUBU_HW_SSE2, "SSE2 baseline on x86-64");
    CHECK(hw.vendor[0] != 0, "vendor string present");
    /* GenuineIntel or AuthenticAMD on x86 */
    CHECK(strncmp(hw.vendor, "GenuineIntel", 12) == 0 ||
          strncmp(hw.vendor, "AuthenticAMD", 12) == 0,
          "known x86 vendor");
    CHECK(hw.cores_logical >= 1, "logical cores >= 1");
    CHECK(hw.cores_physical >= 1, "physical cores >= 1");

    /* the dispatch strategy */
    int tier = wubu_hw_dispatch_tier(&hw);
    CHECK(tier == hw.simd_tier, "dispatch tier matches");
    int kc = wubu_hw_kernel_choice(&hw);
    CHECK(kc >= 1 && kc <= 3, "kernel family in 1..3");

    /* the GPU strategy: never NONE; llvmpipe at minimum */
    int gpu = wubu_hw_gpu_choice(&hw);
    CHECK(gpu >= WUBU_GPU_SW && gpu <= WUBU_GPU_CUDA, "gpu backend valid");
    CHECK(wubu_hw_gpu_name(gpu) != NULL, "gpu name present");
    CHECK(wubu_hw_tier_name(tier) != NULL, "tier name present");

    /* the backend matrix bitmap */
    uint32_t m = wubu_hw_matrix(&hw);
    CHECK((m & 0x7) == (uint32_t)hw.simd_tier, "matrix tier bits");
    CHECK(((m >> 8) & 0x3) == (uint32_t)hw.gpu_backend, "matrix gpu bits");
    if (hw.cuda_present) CHECK((m & (1u << 17)) != 0, "cuda bit set");
    if (hw.dxg_present) CHECK((m & (1u << 16)) != 0, "dxg bit set");

    /* the null guards */
    CHECK(wubu_hw_detect(NULL) == -1, "null rejected");
    CHECK(wubu_hw_dispatch_tier(NULL) == WUBU_HW_SCALAR, "null tier safe");
    CHECK(wubu_hw_gpu_choice(NULL) == WUBU_GPU_SW, "null gpu safe");

    /* the tier flags are mutually consistent */
    if (hw.simd_tier >= WUBU_HW_AVX2) CHECK(hw.has_bmi1 || hw.has_fma,
        "AVX2-tier CPUs typically carry BMI/FMA (informational)");
    if (hw.simd_tier >= WUBU_HW_AVX) CHECK(hw.has_ssse3,
        "AVX implies SSSE3");
    if (hw.simd_tier >= WUBU_HW_SSE2) CHECK(hw.has_sse41 || hw.has_sse42,
        "modern CPUs carry SSE4 (informational)");

    /* print the detected matrix for the telemetry */
    printf("  vendor=%s tier=%s gpu=%s cores=%d/%d fma=%d bmi2=%d vnni=%d\n",
           hw.vendor, wubu_hw_tier_name(hw.simd_tier),
           wubu_hw_gpu_name(hw.gpu_backend), hw.cores_logical,
           hw.cores_physical, hw.has_fma, hw.has_bmi2,
           hw.has_avx512_vnni);
    printf("  brand=%s\n", hw.brand[0] ? hw.brand : "(hidden)");

    if (failures == 0) printf("ALL HWDETECT TESTS PASSED\n");
    else printf("%d HWDETECT FAILURES\n", failures);
    return failures ? 1 : 0;
}
