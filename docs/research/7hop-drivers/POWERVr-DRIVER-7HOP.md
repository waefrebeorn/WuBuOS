# POWERVr-DRIVER-7HOP.md

## Kevin-Bacon 7-hop research: Imagination PowerVR gaps

PowerVR ROGUE GPUs bind pvrsrvkm driver. Mesa 25.3 adds
open-source PowerVR Vulkan; kernel 6.16+ for ROGUE. Phoronix:
Imagination PowerVR open-source driver (6.8 DRM) written
from scratch, DMA-BUF/PRIME, DRM sync objects.

### Impl routing (wubu_powervr.c)

| Route | Path |
|-------|------|
| GPU device presence | /sys/class/drm/card0/device/uevent |
| PCI vendor metric    | /sys/class/drm/card0/device/vendor |

pvrsrvkm needs kernel 6.16+. Vulkan via Mesa 25.3.
