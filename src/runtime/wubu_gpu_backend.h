/*
 * wubu_gpu_backend.h -- GPU backend dispatcher interface.
 *
 * Auto-selects the GPU device path and Vulkan ICD at init, with zero
 * user configuration. Works on bare metal or WSL2.
 */
#ifndef WUBU_GPU_BACKEND_H
#define WUBU_GPU_BACKEND_H

#include <stdint.h>

/* B1: auto-init — detects platform, opens the GPU device, finds the ICD. */
int wubu_gpu_init(void);

/* B2: accessors */
int      wubu_gpu_backend_fd(void);        /* device fd, or -1 */
uint32_t wubu_gpu_backend_vram_mb(void);    /* reported VRAM */
const char *wubu_gpu_backend_device_name(void); /* "wsl2/dev/dxg icd=..." */

#endif /* WUBU_GPU_BACKEND_H */
