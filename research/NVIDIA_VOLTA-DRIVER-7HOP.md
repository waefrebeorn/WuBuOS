# NVIDIA_VOLTA-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: NVIDIA Volta gaps

NVIDIA Volta (V100) binds nvidia datacenter driver 535/550. CUDA
12.0/12.4. V100-SXM2-32GB confirmed on 535.183.01 (CUDA 12.2).
r/LocalLLaMA: "NVIDIA drops Pascal" (Volta still datacenter).

### Impl routing (wubu_nvidia_volta.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Volta = sm_70 datacenter (V100). 535 datacenter driver family.
CUDA 12.x gencode for sm_70. NVK does NOT support Volta (dc).
