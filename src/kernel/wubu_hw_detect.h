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

/* W1: run the detection. Call once at kernel init, before driver probe. */
void wubu_hw_detect(void);

/* W2: accessors */
int         wubu_hw_is_wsl(void);       /* nonzero if WSL2/Microsoft hypervisor */
int         wubu_hw_gpu_present(void);  /* nonzero if *any* GPU device found */
const char *wubu_hw_platform(void);     /* "bare_metal" | "wsl2" | "kvm" | "vmware" | "qemu" | "hyperv" | "unknown" */
const char *wubu_hw_gpu_path(void);     /* "/dev/dxg" | "/dev/nvidia0" | "/dev/dri/card0" | NULL */

/* W3: summary string for the boot console / kvfs world state */
int wubu_hw_summary(char *out, size_t cap);

#endif /* WUBU_HW_DETECT_H */
