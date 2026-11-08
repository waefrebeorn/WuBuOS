/*
 * wubu_gpu_driver_test.c -- REAL GPU behaviour gate.
 *
 * The old wubu_cuda_selftest asserted tautologies (`has_cuda_cc(1) == 1`) and
 * the module only checked that libcuda.so is readable -- nothing ever ran on
 * the GPU.  This gate proves the GPU leg end to end:
 *
 *   1. the CUDA driver API initialises and enumerates an NVIDIA device;
 *   2. its compute capability and name are read from the hardware;
 *   3. a real kernel is compiled to PTX at runtime with nvcc (the proven
 *      "build the artifact at runtime" pattern);
 *   4. the PTX is loaded through the driver (cuModuleLoadData) and launched;
 *   5. the computed result is copied back and VERIFIED.
 *
 * If no CUDA toolchain/driver is present the gate skips with exit code 2
 * (distinct from a failure), so it can live in the gate safely.
 *
 * C11, hosted.  Exit 0 = verified, 1 = failed, 2 = skipped.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cuda.h>

#define N 256

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  [PASS] %s\n", msg); } \
    else      { g_fail++; printf("  [FAIL] %s\n", msg); } \
} while (0)

/* -- The kernel under test (compiled to PTX at runtime) ----------------- */

static const char *k_vector_add =
    "#include <cuda_runtime.h>\n"
    "extern \"C\" __global__ void vector_add(const float *a, const float *b,\n"
    "                                       float *c, int n) {\n"
    "    int i = blockIdx.x * blockDim.x + threadIdx.x;\n"
    "    if (i < n) c[i] = a[i] + b[i];\n"
    "}\n";

static int build_ptx(const char *ptx_path) {
    FILE *f = fopen("/tmp/wubu_gpu_kernel.cu", "w");
    if (!f) return -1;
    fputs(k_vector_add, f);
    fclose(f);
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "nvcc -ptx -o %s /tmp/wubu_gpu_kernel.cu 2>/tmp/wubu_gpu_nvcc.log",
             ptx_path);
    int rc = system(cmd);
    return (rc == 0) ? 0 : -1;
}

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return buf;
}

int main(void) {
    printf("=== WuBuOS GPU driver behaviour gate ===\n");

    /* -- Toolchain precondition (skip, not fail) --------------------- */
    if (system("command -v nvcc >/dev/null 2>&1") != 0) {
        printf("  [SKIP] nvcc not installed; GPU gate skipped.\n");
        return 2;
    }

    /* -- Driver init + device enumeration --------------------------- */
    CUresult r = cuInit(0);
    CHECK(r == CUDA_SUCCESS, "cuInit initialises the CUDA driver");
    if (r != CUDA_SUCCESS) { printf("\n=== gpu: %d passed, %d failed ===\n", g_pass, g_fail); return 1; }

    int count = 0;
    cuDeviceGetCount(&count);
    CHECK(count >= 1, "at least one CUDA device is enumerated");
    if (count < 1) return 1;

    CUdevice dev;
    cuDeviceGet(&dev, 0);

    char name[256] = {0};
    cuDeviceGetName(name, sizeof(name) - 1, dev);
    printf("        device: %s\n", name);
    CHECK(strstr(name, "NVIDIA") != NULL, "device 0 is an NVIDIA GPU");

    int major = 0, minor = 0;
    cuDeviceComputeCapability(&major, &minor, dev);
    printf("        compute capability: %d.%d\n", major, minor);
    CHECK(major >= 7, "compute capability >= 7.0 (tensor cores available)");

    CUcontext ctx;
    r = cuCtxCreate(&ctx, 0, dev);
    CHECK(r == CUDA_SUCCESS, "CUDA context created on the device");

    /* -- Build real PTX at runtime --------------------------------- */
    size_t ptx_len = 0;
    char *ptx = NULL;
    if (build_ptx("/tmp/wubu_gpu_kernel.ptx") != 0) {
        CHECK(0, "nvcc compiles the kernel to PTX at runtime");
        printf("        nvcc log: %s\n",
               access("/tmp/wubu_gpu_nvcc.log", R_OK) == 0 ? "(see /tmp/wubu_gpu_nvcc.log)" : "(none)");
    } else {
        CHECK(1, "nvcc compiles the kernel to PTX at runtime");
        ptx = read_file("/tmp/wubu_gpu_kernel.ptx", &ptx_len);
        CHECK(ptx != NULL && ptx_len > 0, "generated PTX is readable and non-empty");
    }
    if (!ptx) { printf("\n=== gpu: %d passed, %d failed ===\n", g_pass, g_fail); return 1; }

    /* -- Load + launch --------------------------------------------- */
    CUmodule mod;
    r = cuModuleLoadData(&mod, ptx);
    CHECK(r == CUDA_SUCCESS, "driver loads the PTX module (JIT)");
    if (r != CUDA_SUCCESS) { free(ptx); printf("\n=== gpu: %d passed, %d failed ===\n", g_pass, g_fail); return 1; }

    CUfunction fn;
    r = cuModuleGetFunction(&fn, mod, "vector_add");
    CHECK(r == CUDA_SUCCESS, "kernel function resolves from the module");

    float hA[N], hB[N], hC[N];
    for (int i = 0; i < N; i++) { hA[i] = (float)i; hB[i] = (float)(i * 2); }

    CUdeviceptr dA, dB, dC;
    size_t bytes = sizeof(float) * N;
    int n = N;
    if (cuMemAlloc(&dA, bytes) != CUDA_SUCCESS ||
        cuMemAlloc(&dB, bytes) != CUDA_SUCCESS ||
        cuMemAlloc(&dC, bytes) != CUDA_SUCCESS) {
        CHECK(0, "device memory allocated for A/B/C");
        free(ptx); return 1;
    }
    CHECK(1, "device memory allocated for A/B/C");

    cuMemcpyHtoD(dA, hA, bytes);
    cuMemcpyHtoD(dB, hB, bytes);

    void *args[] = { &dA, &dB, &dC, &n };
    r = cuLaunchKernel(fn, N / 128, 1, 1, 128, 1, 1, 0, NULL, args, NULL);
    CHECK(r == CUDA_SUCCESS, "kernel launched on the GPU");
    CHECK(cuCtxSynchronize() == CUDA_SUCCESS, "kernel execution synchronised");

    memset(hC, 0, bytes);
    cuMemcpyDtoH(hC, dC, bytes);

    int correct = 1;
    for (int i = 0; i < N; i++)
        if (hC[i] != hA[i] + hB[i]) { correct = 0;
                                      printf("        mismatch at %d: %.1f != %.1f\n",
                                             i, hC[i], hA[i] + hB[i]);
                                      break; }
    CHECK(correct, "GPU result verified: C[i] == A[i] + B[i] for all 256 elements");

    cuMemFree(dA); cuMemFree(dB); cuMemFree(dC);
    cuModuleUnload(mod);
    cuCtxDestroy(ctx);
    free(ptx);

    printf("\n=== gpu: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
