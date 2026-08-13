/*
 * wubu_hw_detect.h -- the HARDWARE BUS DETECTOR interface.
 *
 * Auto-detects: bare metal vs WSL2 vs VM. Determines the GPU device path
 * (/dev/dxg for WSL, /dev/nvidia0 for bare-metal NVIDIA, /dev/dri/card0
 * for AMD/Intel). Zero user interaction — "magical operating system."
 *
 * C11, opaque-ish (accessors return const char*), minimal includes.
 */
#ifndef WUBU_HW_DETECT_H
#define WUBU_HW_DETECT_H

#include <stddef.h>   /* size_t */

#ifdef _GNU_SOURCE
/* File-existence probe (defined in wubu_gpu_icd.c, used by wubu_hw_summary). */
int wubu_file_exists(const char *path);
#endif

/* W1: run the detection. Call once at kernel init, before driver probe. */
void wubu_hw_detect(void);

/* W2: accessors */
int         wubu_hw_is_wsl(void);       /* nonzero if WSL2/Microsoft hypervisor */
int         wubu_hw_gpu_present(void);  /* nonzero if *any* GPU device found */
const char *wubu_hw_platform(void);     /* "bare_metal" | "wsl2" | "kvm" | "vmware" | "qemu" | "hyperv" | "unknown" */
const char *wubu_hw_gpu_path(void);     /* "/dev/dxg" | "/dev/nvidia0" | "/dev/dri/card0" | NULL */

/* W3: summary string for the boot console / kvfs world state */
int wubu_hw_summary(char *out, size_t cap);

/* W3b: the PCI vendor ID of the detected GPU (0x1002=AMD, 0x8086=Intel,
 * 0x10DE=NVIDIA, 0=unknown/not-bare-metal). Used to pick the right Vulkan
 * ICD (radeon_icd.json / intel_icd.json / nvidia_icd.json). */
int wubu_hw_gpu_vendor(void);
/* the PCI device id of the detected GPU (0 if none). */
int wubu_hw_gpu_device(void);

/* W3c: whether the detected GPU needs the amdgpu KMD to be FORCED with the
 * Southern Islands / Sea Islands module params. GCN1/2 (HD 7000, R7 260,
 * Kaveri) default to the radeon KMD, which has NO Vulkan. The kernel must
 * emit "amdgpu.si_support=1 amdgpu.cik_support=1" for these. Returns the
 * module-param string to append, or NULL if not needed. */
const char *wubu_hw_amdgpu_params(void);

/* W3d: whether the detected GPU prefers the AMDVLK ICD (RDNA4 Navi44/48).
 * RADV is partial on RDNA4; AMDVLK 2025.Q1.3+ is the full Vulkan 1.4 path. */
int wubu_hw_needs_amdvlk(void);

/* W3e: whether the detected Intel GPU prefers the "xe" KMD (Xe2: Lunar Lake
 * Arc, Battlemage, Celestial). Returns nonzero when xe, zero for i915. */
int wubu_hw_intel_uses_xe(void);

/* W3f: whether this is a hybrid iGPU+dGPU laptop (both a bare-metal iGPU and
 * a discrete GPU present). When true the kernel exposes DRI_PRIME. */
int wubu_hw_has_prime(void);

/* W4: Vulkan ICD selection -- returns the preferred ICD JSON path for the
 * detected GPU platform and backend (WSL2 dzn, bare-metal NVIDIA, etc).
 * Returns a static string; NULL if no Vulkan ICD is usable. */
const char *wubu_hw_vulkan_icd(void);

/* W5: full ICD chain for fallback (e.g. "gfxstream:lvp" or "nvidia:lvp").
 * Caller gets a heap-allocated, ':'-separated list; must free(). */
char *wubu_hw_vulkan_icd_chain(void);

/* W6: whether the dzn (Dozen) driver is available -- the WSL2 Vulkan->D3D12
 * bridge that translates Vulkan calls through /dev/dxg to the real GPU. */
int wubu_hw_has_dzn(void);

/* W7: whether the NVIDIA Vulkan ICD is available on bare metal. */
int wubu_hw_has_nvidia_icd(void);

#endif /* WUBU_HW_DETECT_H */
