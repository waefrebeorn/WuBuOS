/*
 * wubu_hw_stub.c -- Minimal hardware-detection stubs.
 *
 * wubu_wine_env.c calls wubu_hw_is_wsl / wubu_hw_has_prime / etc. to pick
 * the Wine/Proton graphics backend. In the hosted/AGI test environment
 * we are not probing the host PCI/DRM stack, so these resolve to harmless
 * no-detected-hardware returns. The real implementations live in the kernel
 * hardware layer (wubu_drv_gpu.c, wubu_gpu_icd.c, wubu_hw_detect.c) and
 * are linked into the full kernel build; they are omitted from the GUI/
 * syscall test binaries to keep the dependency closure small.
 */
#include <stdint.h>

int  wubu_hw_is_wsl(void) { return 0; }
int  wubu_hw_has_prime(void) { return 0; }
int  wubu_hw_has_dzn(void) { return 0; }
int  wubu_hw_has_nvidia_icd(void) { return 0; }
const char *wubu_hw_vulkan_icd(void) { return 0; }
const char *wubu_hw_gpu_path(void) { return 0; }
const char *wubu_hw_vulkan_icd_chain(void) { return 0; }
int  wubu_hw_detect(void) { return 0; }

/* GPU backend probes (kernel-level in wubu_drv_gpu.c; stubs for tests) */
const char *wubu_hw_platform(void) { return "linux"; }
int  wubu_gpu_present_wsl(void) { return 0; }
int  wubu_gpu_vram_mb(void) { return 0; }
