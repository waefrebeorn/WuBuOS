# NVIDIA_PASCAL-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: NVIDIA Pascal gaps

NVIDIA Pascal (GTX 10xx) binds nvidia 535/550/590 driver. CUDA 12.0,
OpenGL 4.6, Vulkan. Reddit r/LocalLLaMA: "NVIDIA drops Pascal on
Linux" (gradually phasing out). Debian wiki: 535 supports Pascal.

### Impl routing (wubu_nvidia_pascal.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

Pascal needs 535+ for CUDA 12.0. 470.xx legacy also supports but is EOL.
NVIDIA 590 drops GTX 900/Maxwell; Pascal on 550+ still supported.
NVK enabled for Maxwell+ (April 2025).
