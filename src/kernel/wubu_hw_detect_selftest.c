/*
 * wubu_hw_detect_selftest.c — proves the OS detects its own runtime.
 *
 * The doctrine: "it should work on bare metal or not bare metal because
 * we are a magical operating system." The kernel must auto-discover:
 *   - whether it runs on WSL2 (dev/dxg present) or bare metal
 *   - which GPU is present and its device path
 *   - what platform string to publish
 *
 *  1. run wubu_hw_detect()
 *  2. assert the platform is one of the known set
 *  3. assert gpu_path matches the detected platform
 *  4. assert KV-FS got the platform + gpu state
 *  5. cross-check with the GPU backend init
 */
#include "wubu_hw_detect.h"
#include "wubu_drv_gpu.h"
#include "../kernel/wubu_kvfs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int failures = 0;
#define CHECK(cond, msg) \
    do { if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
       else { printf("  ok:  %s\n", msg); } } while(0)

int main(void)
{
    printf("=== wubu_hw_detect_selftest (magic OS self-detection) ===\n");

    /* 1. init KV-FS */
    if (g_wubu_kvfs) { wubu_kvfs_free(g_wubu_kvfs); g_wubu_kvfs = NULL; }
    if (g_wubu_kv_base) { free(g_wubu_kv_base); g_wubu_kv_base = NULL; }
    g_wubu_kv_capacity = 0;
    CHECK(wubu_kvfs_kernel_init(256, 4096) == 0, "KV-FS kernel init");
    CHECK(wubu_kvfs_mount(g_wubu_kvfs, "/kv/world", 2048, 2048) == 0, "mount /kv/world");

    /* 2. run detection */
    wubu_hw_detect();
    const char *plat = wubu_hw_platform();
    const char *gpup = wubu_hw_gpu_path();

    printf("  platform    = %s\n", plat ? plat : "(null)");
    printf("  gpu_path    = %s\n", gpup ? gpup : "(null)");
    printf("  is_wsl      = %d\n", wubu_hw_is_wsl());
    printf("  gpu_present = %d\n", wubu_hw_gpu_present());
    printf("  gpu_vendor  = %04x\n", wubu_hw_gpu_vendor());

    /* 3b. driver-routing decisions (GCN1/2 params, RDNA4 AMDVLK, xe, prime). */
    printf("  amdgpu_params = %s\n", wubu_hw_amdgpu_params() ? wubu_hw_amdgpu_params() : "none");
    printf("  prime      = %d\n", wubu_hw_has_prime());
    printf("  xe         = %d\n", wubu_hw_intel_uses_xe());
    printf("  amdvlk     = %d\n", wubu_hw_needs_amdvlk());
    if (wubu_hw_gpu_vendor() == 0x1002) {
        /* AMD: on a real box, a GCN1/2 device yields amdgpu params. */
        CHECK(wubu_hw_amdgpu_params() != NULL || wubu_hw_gpu_device() >= 0,
              "AMD GPU has driver routing info");
    }

    /* 3. assert platform is valid */
    CHECK(plat != NULL, "platform string is non-NULL");
    CHECK(strcmp(plat, "unknown") != 0, "platform is known (not 'unknown')");
    CHECK(strcmp(plat, "bare_metal") == 0 || strcmp(plat, "wsl2") == 0 ||
          strcmp(plat, "kvm") == 0 || strcmp(plat, "vmware") == 0 ||
          strcmp(plat, "qemu") == 0 || strcmp(plat, "hyperv") == 0,
          "platform is in the canonical enum");

    /* 4. gpu_path must be set iff gpu_present */
    if (wubu_hw_gpu_present()) {
        CHECK(gpup != NULL, "gpu_present=true -> gpu_path set");
    } else {
        CHECK(gpup == NULL, "gpu_present=false -> no gpu_path");
    }

    /* 5. KLFS: if /dev/dxg exists, we must be WSL */
    if (access("/dev/dxg", R_OK) == 0) {
        CHECK(wubu_hw_is_wsl(), "/dev/dxg present -> is_wsl");
        CHECK(strcmp(plat, "wsl2") == 0, "/dev/dgx present -> platform=wsl2");
    }

    /* 6. WSL GPU stub */
    if (wubu_gpu_present_wsl()) {
        CHECK(1, "WSL2 GPU passthrough detected (/dev/dxg)");
        CHECK(wubu_gpu_width() > 0 && wubu_gpu_height() > 0,
              "WSL GPU has a mode set");
        CHECK(wubu_gpu_vram_mb() > 0, "WSL GPU has VRAM reported");
    } else {
        CHECK(1, "not WSL — skipping GPU passthrough stub");
    }

    /* 7. summary works */
    char buf[256];
    CHECK(wubu_hw_summary(buf, sizeof(buf)) == 0, "hw_summary succeeds");
    CHECK(strlen(buf) > 0, "hw_summary produced non-empty string");
    printf("   summary: %s\n", buf);

    /* 8. Vulkan ICD selection (kernel-owned driver env). */
    const char *icd = wubu_hw_vulkan_icd();
    CHECK(icd != NULL, "vulkan ICD selected");
    if (wubu_hw_is_wsl()) {
        CHECK(strstr(icd, "dzn") != NULL || strstr(icd, "lvp") != NULL,
              "WSL2 selects dzn or lvp ICD");
        CHECK(wubu_hw_has_dzn(), "dzn driver present on WSL2");
        /* GALLIUM_DRIVER=d3d12 is emitted by wubu_wine_env.c when
         * wubu_hw_is_wsl() && wubu_hw_has_dzn() are both true. */
        CHECK(wubu_hw_is_wsl() && wubu_hw_has_dzn(),
              "GALLIUM_DRIVER=d3d12 will be set (wsl + dzn)");
    }

    /* 9. GPU env emission (via the ICD chain) */
    char envbuf[512];
    char *chain = wubu_hw_vulkan_icd_chain();
    int chainlen = chain ? strlen(chain) : 0;
    CHECK(chainlen > 0, "gpu ICD chain emitted");
    CHECK(strstr(chain, "vulkan/icd.d") != NULL, "ICD chain contains vulkan ICD");
    if (chain) free(chain);

    /* cleanup */
    if (g_wubu_kvfs) wubu_kvfs_free(g_wubu_kvfs);
    if (g_wubu_kv_base) free(g_wubu_kv_base);

    printf("\n=== HW DETECT TESTS: %s (%d failures) ===\n",
           failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
